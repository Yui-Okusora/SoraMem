#pragma once

#include <unordered_map>
#include <Windows.h>

#include "src/MemoryManager/MemoryManager.hpp"

namespace SoraMem
{

    class MMFile;

    class MemView
    {
    public:
        MemView& operator=(const MemView& view)
        {
            lpMapAddress    = view.lpMapAddress;
            dwMapViewSize   = view.dwMapViewSize;
            iViewDelta      = view.iViewDelta;
            _offset         = view._offset;
            parent          = view.parent;
            return *this;
        }

        void*       getPtr() const { return (char*)lpMapAddress + iViewDelta;}
        void*       getPtr_s() const;
        void*       getViewOrigin() const { return lpMapAddress; }

        uint32_t    getOffsetViewOrigin() const { return iViewDelta; }

        uint64_t    getOffsetFromOrigin() const { return _offset; }

        uint64_t    getAllocatedViewSize() const { return static_cast<uint64_t>(dwMapViewSize) - iViewDelta;}

        template<typename T>
        T& at(size_t index)
        {
            if (index >= (getAllocatedViewSize()) / sizeof(T)) {
                throw std::out_of_range("Index out of range");
            }
            return *(reinterpret_cast<T*>(getPtr()) + index);
        }

        void  warmPages();
        ~MemView();

    private:
        friend class MMFile;

        LPVOID        lpMapAddress = nullptr;     // first address of the mapped view
        DWORD         dwMapViewSize = 0;          // the size of the view
        uint32_t      iViewDelta = 0;             // Offset from lpMapAddr
        uint64_t      _offset = 0;                // Offset from origin

        MMFile*       parent = nullptr;

        std::shared_ptr<std::shared_mutex> mutex = std::make_shared<std::shared_mutex>();
    };

    class MMFile
    {
    public:
        friend class MemoryManager;

        MemView&                load(size_t offset, size_t size); // offset and size in bytes
        void                    unload(MemView& view);
        void                    unloadAll();
        void                    resize(const size_t& fileSize); // in bytes
        void                    reset();
        void                    createMapObj();

        bool                    isValid() const;

        HANDLE                  getFileHandle() const { return m_hFile; }
        HANDLE                  getMapHandle() const { return m_hMapFile; }

        size_t                  getID() const { return m_fileID; }

        uint32_t                getSysGran() const { return sysGran; }
        uint32_t                getSysPageSize() const { return sysPageSize; }

        size_t                  getFileSize() const { return m_fileSize; }

        ~MMFile();

        //--------- Thread-safe methods ----------

        MemView&                load_s(size_t offset, size_t size); // offset and size in bytes

        void                    unload_s(MemView& view);
        void                    unloadAll_s();
        void                    resize_s(const size_t& fileSize); // in bytes
        void                    createMapObj_s();

        size_t                  getID_s();

        size_t                  getFileSize_s() const;

        template<typename T>
        T& refAt_s(const size_t& index, const MemView& view)
        {
            std::unique_lock<std::shared_mutex> lock(*view.mutex);
            if (index >= (static_cast<size_t>(view.dwMapViewSize) - view.iViewDelta) / sizeof(T)) {
                throw std::out_of_range("Index out of range");
            }
            return *(reinterpret_cast<T*>(view.getPtr()) + index);
        }

        template<typename T>
        T readAt(const size_t& index, const MemView& view) const
        {
            std::shared_lock<std::shared_mutex> lock(*view.mutex);
            if (index >= (static_cast<size_t>(view.dwMapViewSize) - view.iViewDelta) / sizeof(T)) {
                throw std::out_of_range("Index out of range");
            }
            return *(reinterpret_cast<T*>(view.getPtr()) + index);
        }


    private:
        MemView&                _load(MemView& view, size_t offset, size_t size);

        void                    closeAllPtr();
        void                    closeAllPtr_s();

        HANDLE&                 setFileHandle()     { return m_hFile; }
        HANDLE&                 setMapHandle()      { return m_hMapFile; }

        size_t&                 setID()             { return m_fileID; }

        uint32_t&               setSysGran()        { return sysGran; }
        uint32_t&               setSysPageSize()    { return sysPageSize; }


        //--- Member Variables

        HANDLE m_hMapFile = nullptr;        // handle for the file's memory-mapped region
        HANDLE m_hFile = nullptr;           // the file handle

        size_t m_fileSize = 0;              // temporary storage for file sizes  
        size_t m_fileID = 0;

        uint32_t sysGran = 0;               // System granularity size
        uint32_t sysPageSize = 0;           // System page size

        const uint64_t alignment = 1024;    // Standard file alignment in bytes

        std::unordered_map<LPVOID, MemView> views;
        std::shared_ptr<std::shared_mutex> mutex = std::make_shared<std::shared_mutex>();
    };

}