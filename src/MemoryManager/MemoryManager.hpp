#pragma once

#include <mutex>
#include <shared_mutex>
#include <list>

#include "src/AdvancedMemory/AdvancedMemory.hpp"

namespace SoraMem
{
    class AdvancedMemory;

    class MemoryFilePool{
    public:
        AdvancedMemory* acquire();

        inline void release(AdvancedMemory* ptr);
        inline void clear();

        inline size_t size();
    private:
        std::list<AdvancedMemory*>			filePool;
        std::shared_mutex					mutex;
    };

    class MemoryManager {
    public:
        static MemoryManager& getManager();
        void initManager();
        void setTmpDir(const std::string& dir);

        MemoryManager(MemoryManager const&) = delete;
        void operator=(MemoryManager const&) = delete;


        void createTmp(AdvancedMemory*& memPtr, const size_t& fileSize);
        void createPmnt(AdvancedMemory* memPtr, const size_t& fileSize);


        void memcopy_AVX2(AdvancedMemory*& _dst, void* _src, const size_t& _size);


        void memcopy(AdvancedMemory*& _dst, void* _src, const size_t& _size);
        void memcopy(AdvancedMemory*& _dst, AdvancedMemory* _src, const short& _typeSize, const size_t& _size);
        

        void move(AdvancedMemory* _dst, AdvancedMemory* _src);

        void free(AdvancedMemory* ptr);
        void addTmpInactive(const unsigned long& id) { inactiveFileID.emplace_back(id); }
        
        
        DWORD getSysGranularity() const { return dwSysGran; }
        
        
        std::atomic<unsigned long long>& getUsedMemory() { return m_usedMem; }
    
    private:
        MemoryManager() {};
        void copyThreadsRawPtr(AdvancedMemory* _dst, void* _src, size_t offset, size_t _size);
        void copyThreadsRawPtr_AVX2(AdvancedMemory* _dst, void* _src, size_t offset, size_t _size);

        unsigned							n_FileCreated = 0;
        unsigned long						dwSysGran = 0;
        std::string							tmpDir = "";

        std::atomic<unsigned long long>		m_usedMem;
        std::atomic<unsigned long>			m_fileID;
        std::atomic<unsigned long>			permFileID;

        std::unique_ptr<std::shared_mutex>	mutex = std::make_unique<std::shared_mutex>();

        std::list<unsigned long>			inactiveFileID;
        MemoryFilePool						filePool;
    };
}

static SoraMem::MemoryManager& MemMng = SoraMem::MemoryManager::getManager();