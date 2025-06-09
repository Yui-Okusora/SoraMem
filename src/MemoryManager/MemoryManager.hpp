#pragma once

#include <mutex>
#include <shared_mutex>
#include <list>

#include "src/MMFile/MMFile.hpp"

namespace SoraMem
{
    class MMFile;

    class MemoryFilePool{
    public:
        MMFile*                 acquire();

        inline void             release(MMFile* ptr);
        inline void             clear();

        inline size_t           size();
    private:
        std::list<MMFile*>      filePool;
        std::shared_mutex       mutex;
    };

    class MemoryManager {
    public:
        MemoryManager() {};
        void initManager();
        void setTmpDir(const std::string& dir);

        MemoryManager(MemoryManager const&) = delete;
        void operator=(MemoryManager const&) = delete;


        void createTmp(MMFile*& memPtr, const size_t& fileSize);
        void createPmnt(MMFile* memPtr, const size_t& fileSize);


        void memcopy_AVX2(MMFile*& _dst, void* _src, const size_t& _size);


        void memcopy(MMFile*& _dst, void* _src, const size_t& _size);
        void memcopy(MMFile*& _dst, MMFile* _src, const short& _typeSize, const size_t& _size);
        

        void move(MMFile* _dst, MMFile* _src);

        void free(MMFile* ptr);
        void addTmpInactive(const unsigned long& id) { inactiveFileID.emplace_back(id); }
        
        
        DWORD getSysGranularity() const { return dwSysGran; }
        
        
        std::atomic<unsigned long long>& getUsedMemory() { return m_usedMem; }
        

    private:
        void copyThreadsRawPtr(MMFile* _dst, void* _src, size_t offset, size_t _size);
        void copyThreadsRawPtr_AVX2(MMFile* _dst, void* _src, size_t offset, size_t _size);

#ifdef TESTING
    public:
#endif // TESTING

        unsigned                            n_FileCreated = 0;
        unsigned long                       dwSysGran = 0;
        unsigned long                       dwPageSize = 0;
        
        std::string                         tmpDir = "";

        std::atomic<unsigned long long>     m_usedMem;
        std::atomic<unsigned long>          m_fileID;
        std::atomic<unsigned long>          permFileID;

        std::unique_ptr<std::shared_mutex>  mutex = std::make_unique<std::shared_mutex>();

        std::list<unsigned long>            inactiveFileID;
        MemoryFilePool                      filePool;
    };

}

static SoraMem::MemoryManager MemMng; //Default manager

