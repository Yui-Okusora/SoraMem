#include "MMFile.hpp"
#include "src/MemoryManager/MemoryManager.hpp"

#include <string>

namespace SoraMem
{
    bool MMFile::isValid() const noexcept
    {
        return (m_hFile != INVALID_HANDLE_VALUE) && (m_hMapFile != INVALID_HANDLE_VALUE);
    }

    MemView& MMFile::_load(MemView& view, size_t offset, size_t size)
    {
        // Calculate file map start and view size
        DWORD dwFileMapStart = static_cast<DWORD>((offset / sysGran) * sysGran);
        view.parent = this;
        view._offset = offset;
        view.dwMapViewSize = static_cast<DWORD>((offset % sysGran) + size);
        view.iViewDelta = static_cast<unsigned long>(offset - dwFileMapStart);


        // Split high and low parts for 64-bit offset
        LARGE_INTEGER newSize;
        newSize.QuadPart = static_cast<LONGLONG>(dwFileMapStart);


        // Map the file view
        view.lpMapAddress = MapViewOfFile(getMapHandle(), FILE_MAP_ALL_ACCESS, newSize.HighPart, newSize.LowPart, view.dwMapViewSize);

        if (view.lpMapAddress == nullptr) {
            throw std::runtime_error("Failed to map view of file. Error code: " + std::to_string(GetLastError()));
        }

        // Update memory usage atomically
        manager->getUsedMemory().fetch_add(view.dwMapViewSize, std::memory_order_relaxed);

        return view;
    }

    MemView& MMFile::load(size_t offset, size_t size)
    {
        if (!isValid()) {
            throw std::invalid_argument("Invalid file or map handle.");
        }

        if (offset + size > getFileSize()) {
            throw std::out_of_range("Offset exceeds file size. File size: " + std::to_string(getFileSize()) + ", Offset: " + std::to_string(offset));
        }

        MemView view;
        _load(view, offset, size);

        // Use emplace for efficient insertion
        return views.emplace(view.lpMapAddress, std::move(view)).first->second;
    }

    MemView& MMFile::load_s(size_t offset, size_t size)
    {
        {
            std::shared_lock<std::shared_mutex> lock(mutex);
            if (!isValid()) {
                throw std::invalid_argument("Invalid file or map handle.");
            }
        }

        if (offset + size > getFileSize()) {
            throw std::out_of_range("Offset exceeds file size. File size: " + std::to_string(getFileSize()) + ", Offset: " + std::to_string(offset));
        }

        MemView view;
        {
            std::lock_guard<std::mutex> lock(view.mutex);
            _load(view, offset, size);
        }

        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            return views.emplace(view.lpMapAddress, std::move(view)).first->second;
        }
    }

    void MMFile::resize(const size_t& fileSize)
    {
        // Align file size up to the next multiple of alignment
        size_t alignedSize = (fileSize + alignment - 1) & ~(alignment - 1);


        if (alignedSize == m_fileSize) {
            return; // No need to resize if the size is unchanged
        }

        unloadAll();

        HANDLE& mapHandle = setMapHandle();
        HANDLE& fileHandle = setFileHandle();

        if (mapHandle != nullptr) {
            CloseHandle(mapHandle);
        }

        LARGE_INTEGER newSize;
        newSize.QuadPart = static_cast<LONGLONG>(alignedSize);

        if (!SetFilePointerEx(fileHandle, newSize, NULL, FILE_BEGIN) || !SetEndOfFile(fileHandle)) {
            throw std::runtime_error("Failed to resize file to " + std::to_string(alignedSize) + " bytes. Error code: " + std::to_string(GetLastError()));
        }

        m_fileSize = alignedSize;
        createMapObj();
    }

    void MMFile::resize_s(const size_t& fileSize)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        resize(fileSize);
    }

    void MMFile::createMapObj()
    {
        setMapHandle() = CreateFileMapping(getFileHandle(), NULL, PAGE_READWRITE, 0, 0, NULL);
    }

    void MMFile::createMapObj_s()
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        createMapObj();
    }

    void MMFile::unload(MemView& view)
    {
        auto it = views.find(view.lpMapAddress);
        if (it == views.end()) {
            return; // View not found
        }

        manager->getUsedMemory().fetch_sub(view.dwMapViewSize, std::memory_order_relaxed);
        FlushViewOfFile(view.getViewOrigin(), view.getViewSize());  // flush modified view to file cache
        UnmapViewOfFile(view.lpMapAddress);
        views.erase(it);
    }

    void MMFile::unload_s(MemView& view)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        unload(view);
    }

    void MMFile::unloadAll()
    {
        size_t totalFreedMemory = 0;

        for (auto it = views.begin(); it != views.end(); ) {
            totalFreedMemory += it->second.dwMapViewSize;
            UnmapViewOfFile(it->second.lpMapAddress);
            it = views.erase(it); // Efficiently erase while iterating
        }
        FlushFileBuffers(getFileHandle());
        manager->getUsedMemory().fetch_sub(totalFreedMemory, std::memory_order_relaxed);
    }

    void MMFile::unloadAll_s()
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        unloadAll();
    }

    void MMFile::closeAllPtr()
    {
        unloadAll();
        if (getMapHandle() != nullptr) {
            CloseHandle(getMapHandle());
            setMapHandle() = nullptr;
        }
        if (getFileHandle() != nullptr) {
            CloseHandle(getFileHandle());
            setFileHandle() = nullptr;
        }
    }

    void MMFile::closeAllPtr_s()
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        closeAllPtr();
    }

    size_t MMFile::getFileSize_s() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return getFileSize();
    }

    size_t MMFile::getID_s()
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return getID();
    }

    void MMFile::reset()
    {
        unloadAll_s();
        CloseHandle(getMapHandle());
        setMapHandle() = nullptr;
        m_fileSize = 0;
    }
    
    uint32_t MMFile::getCRC32() noexcept
    {
        return getCRC().getCRC32();
    }
    
    uint64_t MMFile::getCRC64() noexcept
    {
        return getCRC().getCRC64();
    }

    MMFile::~MMFile()
    {
        FlushFileBuffers(getFileHandle());
        closeAllPtr();
        std::unique_lock<std::shared_mutex> lock(mutex);
        manager->addTmpInactive((unsigned long)m_fileID);
        m_fileSize = 0;
        m_fileID = 0;
    }

    //-------- MemView definitions ---------

    MemView::~MemView()
    {
        if(parent != nullptr)
        parent->unload(*this);
    }

    void* MemView::getPtr_s() const 
    {
        std::lock_guard<std::mutex> lock(mutex);
        return getPtr();
    }

    void MemView::warmPages()
    {
        for (uint64_t i = 0; i + parent->getSysPageSize() <= getAllocatedViewSize(); i += parent->getSysPageSize())
        {
            volatile char tmp = at<char>(i);
        }
    }
}