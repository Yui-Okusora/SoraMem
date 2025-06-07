#include "AdvancedMemory.hpp"

#include <string>

namespace SoraMem
{
    bool AdvancedMemory::isValid() const
    {
        return (m_hFile != INVALID_HANDLE_VALUE) && (m_hMapFile != INVALID_HANDLE_VALUE);
    }

    ViewOfAdvancedMemory& AdvancedMemory::_load(ViewOfAdvancedMemory& view, size_t offset, size_t size)
    {
        // Cache system granularity to avoid repeated calls
        const DWORD sysGranularity = MemMng.getSysGranularity();


        // Calculate file map start and view size
        DWORD dwFileMapStart = static_cast<DWORD>((offset / sysGranularity) * sysGranularity);
        view.parent = this;
        view._offset = offset;
        view.dwMapViewSize = static_cast<DWORD>((offset % sysGranularity) + size);
        view.iViewDelta = static_cast<unsigned long>(offset - dwFileMapStart);


        // Split high and low parts for 64-bit offset
        LARGE_INTEGER newSize;
        newSize.QuadPart = static_cast<LONGLONG>(dwFileMapStart);


        // Map the file view
        view.lpMapAddress = MapViewOfFile(getMapHandle(), FILE_MAP_ALL_ACCESS, newSize.HighPart, newSize.LowPart, view.dwMapViewSize);

        if (view.lpMapAddress == NULL) {
            throw std::runtime_error("Failed to map view of file. Error code: " + std::to_string(GetLastError()));
        }

        // Update memory usage atomically
        MemMng.getUsedMemory().fetch_add(view.dwMapViewSize, std::memory_order_relaxed);

        return view;
    }

    ViewOfAdvancedMemory& AdvancedMemory::load(size_t offset, size_t size)
    {
        if (!isValid()) {
            throw std::invalid_argument("Invalid file or map handle.");
        }

        if (offset + size >= getFileSize()) {
            throw std::out_of_range("Offset exceeds file size. File size: " + std::to_string(getFileSize()) + ", Offset: " + std::to_string(offset));
        }

        ViewOfAdvancedMemory view;
        _load(view, offset, size);

        // Use emplace for efficient insertion
        return views.emplace(view.lpMapAddress, view).first->second;
    }

    ViewOfAdvancedMemory& AdvancedMemory::load_s(size_t offset, size_t size)
    {
        {
            std::shared_lock<std::shared_mutex> lock(*mutex);
            if (!isValid()) {
                throw std::invalid_argument("Invalid file or map handle.");
            }
        }

        if (offset + size >= getFileSize()) {
            throw std::out_of_range("Offset exceeds file size. File size: " + std::to_string(getFileSize()) + ", Offset: " + std::to_string(offset));
        }

        ViewOfAdvancedMemory view;
        {
            std::unique_lock<std::shared_mutex> lock(*view.mutex);
            _load(view, offset, size);
        }

        {
            std::unique_lock<std::shared_mutex> lock(*mutex);
            return views.emplace(view.lpMapAddress, view).first->second;
        }
    }

    void AdvancedMemory::resize(const size_t& fileSize)
    {
        // Align file size up to the next multiple of alignment
        size_t alignedSize = (fileSize + alignment - 1) & ~(alignment - 1);


        if (alignedSize == m_fileSize) {
            return; // No need to resize if the size is unchanged
        }

        unloadAll();

        HANDLE& mapHandle = getMapHandle();
        HANDLE& fileHandle = getFileHandle();

        if (mapHandle != NULL) {
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

    void AdvancedMemory::resize_s(const size_t& fileSize)
    {
        std::unique_lock<std::shared_mutex> lock(*mutex);
        resize(fileSize);
    }

    void AdvancedMemory::createMapObj()
    {
        getMapHandle() = CreateFileMapping(getFileHandle(), NULL, PAGE_READWRITE, 0, 0, NULL);
    }

    void AdvancedMemory::createMapObj_s()
    {
        std::unique_lock<std::shared_mutex> lock(*mutex);
        createMapObj();
    }

    void AdvancedMemory::unload(ViewOfAdvancedMemory& view)
    {
        auto it = views.find(view.lpMapAddress);
        if (it == views.end()) {
            return; // View not found
        }

        MemMng.getUsedMemory().fetch_sub(view.dwMapViewSize, std::memory_order_relaxed);

        UnmapViewOfFile(view.lpMapAddress);
        views.erase(it);
    }

    void AdvancedMemory::unload_s(ViewOfAdvancedMemory& view)
    {
        std::unique_lock<std::shared_mutex> lock(*mutex);
        unload(view);
    }

    void AdvancedMemory::unloadAll()
    {
        size_t totalFreedMemory = 0;

        for (auto it = views.begin(); it != views.end(); ) {
            totalFreedMemory += it->second.dwMapViewSize;
            UnmapViewOfFile(it->second.lpMapAddress);
            it = views.erase(it); // Efficiently erase while iterating
        }

        MemMng.getUsedMemory().fetch_sub(totalFreedMemory, std::memory_order_relaxed);
    }

    void AdvancedMemory::unloadAll_s()
    {
        std::unique_lock<std::shared_mutex> lock(*mutex);
        unloadAll();
    }

    void AdvancedMemory::closeAllPtr()
    {
        unloadAll();
        if (getMapHandle() != NULL) {
            CloseHandle(getMapHandle());
            getMapHandle() = NULL;
        }
        if (getFileHandle() != NULL) {
            CloseHandle(getFileHandle());
            getFileHandle() = NULL;
        }
    }

    void AdvancedMemory::closeAllPtr_s()
    {
        std::unique_lock<std::shared_mutex> lock(*mutex);
        closeAllPtr();
    }

    void* AdvancedMemory::getViewPtr(const ViewOfAdvancedMemory& view) const
    {
        return (char*)view.lpMapAddress + view.iViewDelta;
    }

    void* AdvancedMemory::getViewPtr_s(const ViewOfAdvancedMemory& view) const
    {
        std::unique_lock<std::shared_mutex> lock(*view.mutex);
        return getViewPtr(view);
    }

    const size_t& AdvancedMemory::getFileSize() const
    {
        return m_fileSize;
    }

    const size_t& AdvancedMemory::getFileSize_s() const
    {
        std::shared_lock<std::shared_mutex> lock(*mutex);
        return getFileSize();
    }

    size_t AdvancedMemory::getID() const
    {
        return m_fileID;
    }

    size_t AdvancedMemory::getID_s() const
    {
        std::shared_lock<std::shared_mutex> lock(*mutex);
        return getID();
    }

    void AdvancedMemory::reset()
    {
        unloadAll_s();
        CloseHandle(getMapHandle());
        getMapHandle() = NULL;
        m_fileSize = 0;
    }

    AdvancedMemory::~AdvancedMemory()
    {
        closeAllPtr();
        std::unique_lock<std::shared_mutex> lock(*mutex);
        MemMng.addTmpInactive((unsigned long)m_fileID);
        m_fileSize = 0;
        m_fileID = 0;
    }
}