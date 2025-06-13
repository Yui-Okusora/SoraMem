#pragma once

#include <unordered_map>
#include <Windows.h>
#include <shared_mutex>
#include <mutex>
#include "src/CRC32_64/CRC32_64.hpp"

namespace SoraMem
{
    class MemoryManager;

    class MMFile;

    class MemView
    {
    public:
        MemView() {}

        MemView(const MemView& other)
            : lpMapAddress(other.lpMapAddress),
            dwMapViewSize(other.dwMapViewSize),
            iViewDelta(other.iViewDelta),
            _offset(other._offset),
            parent(other.parent)
        {}

        MemView(MemView&& other) noexcept
            : lpMapAddress(other.lpMapAddress),
            dwMapViewSize(other.dwMapViewSize),
            iViewDelta(other.iViewDelta),
            _offset(other._offset),
            parent(other.parent)
        {
            // Leave other's data in a valid state if necessary
            other.lpMapAddress = nullptr;
            other.dwMapViewSize = 0;
            other.iViewDelta = 0;
            other._offset = 0;
            other.parent = nullptr;
            // mutex is default constructed, not moved
        }

        MemView& operator=(const MemView& view)
        {
            lpMapAddress    = view.lpMapAddress;
            dwMapViewSize   = view.dwMapViewSize;
            iViewDelta      = view.iViewDelta;
            _offset         = view._offset;
            parent          = view.parent;
            return *this;
        }

        MemView& operator=(MemView&& other) noexcept
        {
            if (this != &other) {
                lpMapAddress = other.lpMapAddress;
                dwMapViewSize = other.dwMapViewSize;
                iViewDelta = other.iViewDelta;
                _offset = other._offset;
                parent = other.parent;

                other.lpMapAddress = nullptr;
                other.dwMapViewSize = 0;
                other.iViewDelta = 0;
                other._offset = 0;
                other.parent = nullptr;
                // mutex is not moved
            }
            return *this;
        }

        void*       getPtr() const { return (char*)lpMapAddress + iViewDelta;}
        void*       getPtr_s() const;
        void*       getViewOrigin() const noexcept { return lpMapAddress; }

        uint32_t    getOffsetViewOrigin() const noexcept { return iViewDelta; }

        uint64_t    getOffsetFromOrigin() const noexcept { return _offset; }

        uint64_t    getAllocatedViewSize() const { return getViewSize() - iViewDelta;}

        uint64_t    getViewSize() const { return static_cast<uint64_t>(dwMapViewSize); }

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
        uint32_t      dwMapViewSize = 0;          // the size of the view
        uint32_t      iViewDelta = 0;             // Offset from lpMapAddr
        uint64_t      _offset = 0;                // Offset from origin

        MMFile*       parent = nullptr;

        mutable std::mutex mutex;
    };

    class MMFile
    {
    public:
        friend class MemoryManager;

        MMFile(){}

        MemView&                load(size_t offset, size_t size); // offset and size in bytes
        void                    unload(MemView& view);
        void                    unloadAll();
        void                    resize(const size_t& fileSize); // in bytes
        void                    reset();
        void                    createMapObj();

        bool                    isValid()           const noexcept;

        HANDLE                  getFileHandle()     const noexcept { return m_hFile; }
        HANDLE                  getMapHandle()      const noexcept { return m_hMapFile; }

        size_t                  getID()             const noexcept { return m_fileID; }

        uint32_t                getSysGran()        const noexcept { return sysGran; }
        uint32_t                getSysPageSize()    const noexcept { return sysPageSize; }

        size_t                  getFileSize()       const noexcept { return m_fileSize; }

        CRC32_64&               getCRC()                  noexcept { return crc; }
        uint32_t                getCRC32()                noexcept;
        uint64_t                getCRC64()                noexcept;

        ~MMFile();

        //--------- Thread-safe methods ----------

        MemView&                load_s(size_t offset, size_t size); // offset and size in bytes

        void                    unload_s(MemView& view);
        void                    unloadAll_s();
        void                    resize_s(const size_t& fileSize); // in bytes
        void                    createMapObj_s();

        size_t                  getID_s();

        size_t                  getFileSize_s() const;

    private:
        MemView&                _load(MemView& view, size_t offset, size_t size);

        void                    closeAllPtr();
        void                    closeAllPtr_s();

        HANDLE&                 setFileHandle()     noexcept { return m_hFile; }
        HANDLE&                 setMapHandle()      noexcept { return m_hMapFile; }

        size_t&                 setID()             noexcept { return m_fileID; }

        uint32_t&               setSysGran()        noexcept { return sysGran; }
        uint32_t&               setSysPageSize()    noexcept { return sysPageSize; }

        MemoryManager*&         setManager()        noexcept { return manager; }


        //--- Member Variables

        HANDLE m_hMapFile = nullptr;        // handle for the file's memory-mapped region
        HANDLE m_hFile = nullptr;           // the file handle

        size_t m_fileSize = 0;              // temporary storage for file sizes  
        size_t m_fileID = 0;

        uint32_t sysGran = 0;               // System granularity size
        uint32_t sysPageSize = 0;           // System page size

        const uint64_t alignment = 64;    // Standard file alignment in bytes
        
        MemoryManager* manager = nullptr;
        CRC32_64 crc;

        std::unordered_map<LPVOID, MemView> views;
        mutable std::shared_mutex mutex;
    };

}