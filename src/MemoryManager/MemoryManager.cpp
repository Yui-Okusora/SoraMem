#include "MemoryManager.hpp"

#include <immintrin.h>
#include <algorithm>
#include <thread>
#include <iostream>
#include <windows.h>
#include <vector>

#include "src/Timer.hpp"

namespace SoraMem
{
    MemoryManager& MemoryManager::getManager()
    {
        static MemoryManager instance;
        return instance;
    }

    void MemoryManager::initManager()
    {
        static std::once_flag initFlag;
        std::call_once(initFlag, [&]() {
            SYSTEM_INFO SysInfo;
            GetSystemInfo(&SysInfo);
            dwSysGran = SysInfo.dwAllocationGranularity;
            m_usedMem = 0;
            m_fileID = 0;
            permFileID = 0;
            });
    }

    void MemoryManager::setTmpDir(const std::string& dir)
    {
        std::unique_lock<std::shared_mutex> lock(*MemMng.mutex);
        tmpDir = dir;
        if (!CreateDirectory(dir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            throw std::runtime_error("Failed to create directory: " + dir);
        }
    }

    void MemoryManager::createTmp(AdvancedMemory*& memPtr, const size_t& fileSize)
    {
        AdvancedMemory* tmp = filePool.acquire();

        std::string dir;

        unsigned long tmpID;
        {
            std::unique_lock<std::shared_mutex> lockIDqueue(*MemMng.mutex);
            if (inactiveFileID.empty()) {
                tmpID = m_fileID++;
            }
            else {
                tmpID = inactiveFileID.front();
                inactiveFileID.pop_front();
            }
        }

        dir = tmpDir + std::to_string(tmpID) + ".tmpbin";

        tmp->m_fileID = tmpID;
        tmp->getFileHandle() = CreateFile(dir.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (tmp->getFileHandle() == INVALID_HANDLE_VALUE) {
            delete tmp;
            throw std::runtime_error("Failed to create temporary file: " + std::string(dir));
        }

        tmp->resize(fileSize);
        memPtr = tmp;
    }

    void MemoryManager::createPmnt(AdvancedMemory* memPtr, const size_t& fileSize)
    {
        AdvancedMemory& tmp = *memPtr;
    }

    void MemoryManager::memcopy_AVX2(AdvancedMemory*& _dst, void* _src, const size_t& _size)
    {
        if (_dst == nullptr) {
            MemMng.createTmp(_dst, _size);
        }

        {
            std::shared_lock<std::shared_mutex> lockDst(*_dst->mutex);
            if (_dst->getFileSize() < _size) {
                _dst->resize(_size);
            }
        }

        const size_t granularity = dwSysGran; // e.g., 65536
        const size_t maxThreads = 4;
        const size_t totalChunks = (_size + granularity - 1) / granularity;
        const size_t numThreads = (std::min)(maxThreads, totalChunks);

        const size_t chunksPerThread = totalChunks / numThreads;
        const size_t remainderChunks = totalChunks % numThreads;

        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        size_t currentOffset = 0;

        for (size_t i = 0; i < numThreads; ++i) {
            size_t thisChunkCount = chunksPerThread + (i < remainderChunks ? 1 : 0);
            size_t chunkSize = thisChunkCount * granularity;

            size_t offset = currentOffset;
            currentOffset += chunkSize;

            size_t actualSize = (std::min)(chunkSize, _size - offset); // Clamp at the end

            threads.emplace_back(&MemoryManager::copyThreadsRawPtr, this, _dst, _src, offset, actualSize);
        }

        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void MemoryManager::memcopy(AdvancedMemory*& _dst, void* _src, const size_t& _size)
    {
        if (_dst == nullptr) {
            MemMng.createTmp(_dst, _size);
        }

        {
            std::shared_lock<std::shared_mutex> lockDst(*_dst->mutex);
            if (_dst->getFileSize() < _size) {
                _dst->resize(_size);
            }
        }

        const size_t chunkSize = (std::min)(static_cast<size_t>(dwSysGran * 1024), _size);
        const size_t numThreads = (_size + chunkSize - 1) / chunkSize;

        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        for (size_t i = 0; i < numThreads; ++i) {
            size_t offset = i * chunkSize;
            size_t chunkCpy = (std::min)(chunkSize, _size - offset);
            threads.emplace_back(&MemoryManager::copyThreadsRawPtr, this, _dst, _src, offset, chunkCpy);
        }

        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void MemoryManager::memcopy(AdvancedMemory*& _dst, AdvancedMemory* _src, const short& _typeSize, const size_t& _size)
    {
        Timer timer("MemoryManager::memcopy");
        if (_dst == NULL) MemMng.createTmp(_dst, _size);
        std::unique_lock<std::shared_mutex> lockDst(*_dst->mutex);
        std::unique_lock<std::shared_mutex> lockSrc(*_src->mutex);

        if (_dst->getFileHandle() == NULL)
        {
            lockDst.unlock();
            MemMng.createTmp(_dst, _size);
            lockDst.lock();
        }

        _dst->unloadAll();
        _src->unloadAll();

        CHAR lpDstFileName[MAX_PATH] = "";
        CHAR lpSrcFileName[MAX_PATH] = "";

        GetFinalPathNameByHandle(_dst->getFileHandle(), lpDstFileName, MAX_PATH, FILE_NAME_NORMALIZED);
        GetFinalPathNameByHandle(_src->getFileHandle(), lpSrcFileName, MAX_PATH, FILE_NAME_NORMALIZED);

        _dst->closeAllPtr();
        _src->closeAllPtr();
        CopyFile(lpSrcFileName, lpDstFileName, false);
        _dst->getFileHandle() = CreateFile(lpDstFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (_dst->getFileHandle() == NULL)
            int e = GetLastError();

        _src->getFileHandle() = CreateFile(lpSrcFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        _dst->m_fileSize = GetFileSize(_dst->getFileHandle(), NULL);
        _dst->createMapObj();
        _src->createMapObj();
    }

    void MemoryManager::copyThreadsRawPtr(AdvancedMemory* _dst, void* _src, size_t offset, size_t _size)
    {
        ViewOfAdvancedMemory& dstView = _dst->load_s(offset, _size);
        memcpy(_dst->getViewPtr_s(dstView), (char*)_src + offset, _size);
        _dst->unload_s(dstView);
    }

    void MemoryManager::copyThreadsRawPtr_AVX2(AdvancedMemory* _dst, void* _src, size_t offset, size_t _size)
    {
        // Load destination view
        ViewOfAdvancedMemory& dstView = _dst->load_s(offset, _size);

        uint8_t* dstPtr = (uint8_t*)_dst->getViewPtr_s(dstView);
        uint8_t* srcPtr = (uint8_t*)_src + offset;

        size_t i = 0;

        // Process 32 bytes (256 bits) at a time using AVX2 with aligned loads and stores
        for (; i + 31 < _size; i += 32) {
            // Load 32 bytes (256 bits) from source into AVX2 register (aligned)
            __m256i data = _mm256_load_si256(reinterpret_cast<const __m256i*>(srcPtr + i));

            // Store 32 bytes (256 bits) to destination from AVX2 register (aligned)
            _mm256_store_si256(reinterpret_cast<__m256i*>(dstPtr + i), data);
        }

        // Handle any remaining bytes that are less than 32 bytes
        for (; i < _size; ++i) {
            dstPtr[i] = srcPtr[i];
        }

        // Unload destination view
        _dst->unload_s(dstView);
    }

    void MemoryManager::move(AdvancedMemory* _dst, AdvancedMemory* _src)
    {
        HANDLE thisProcess = GetCurrentProcess();
        _dst->closeAllPtr();

        if (!DuplicateHandle(thisProcess, _src->getFileHandle(), thisProcess, &_dst->getFileHandle(), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            throw std::runtime_error("Failed to duplicate file handle.");
        }

        if (!DuplicateHandle(thisProcess, _src->getMapHandle(), thisProcess, &_dst->getMapHandle(), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            throw std::runtime_error("Failed to duplicate map handle.");
        }
        
        _dst->m_fileSize = _src->getFileSize();
        //_dst->m_AllocatedSize = _src->getAllocatedSize();
        _dst->m_fileID = _src->m_fileID;

        _src->closeAllPtr();
    }

    void MemoryManager::free(AdvancedMemory* ptr) {
        filePool.release(ptr);
    }

    AdvancedMemory* MemoryFilePool::acquire()
    {
            
        if (!filePool.empty()) {
            std::unique_lock<std::shared_mutex> lock(mutex);
            AdvancedMemory* memPtr = filePool.front();
            filePool.pop_front();
            return memPtr;
        }
        return new AdvancedMemory();
    }

    void MemoryFilePool::release(AdvancedMemory* ptr)
    {
        ptr->reset();
        filePool.push_back(ptr);
    }

    size_t MemoryFilePool::size()
    {
        return filePool.size();
    }

    void MemoryFilePool::clear()
    {
        filePool.clear();
    }
}