#include "MemoryManager.hpp"

#include <immintrin.h>
#include <algorithm>
#include <thread>
#include <iostream>

#include <vector>

#include "src/Timer.hpp"

#include "src/MMFile/MMFile.hpp"

namespace SoraMem
{
    void MemoryManager::initManager()
    {
        static std::once_flag initFlag;
        std::call_once(initFlag, [&]() {
            SYSTEM_INFO SysInfo;
            GetSystemInfo(&SysInfo);
            dwSysGran = SysInfo.dwAllocationGranularity;
            dwPageSize = SysInfo.dwPageSize;
            m_usedMem = 0;
            m_fileID = 0;
            permFileID = 0;
            });
        CRC32_64::init();
    }

    void MemoryManager::setTmpDir(const std::string& dir)
    {
        std::unique_lock<std::shared_mutex> lock(*MemMng.mutex);
        tmpDir = dir;
        if (!CreateDirectory(dir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            throw std::runtime_error("Failed to create directory: " + dir);
        }
    }

    void MemoryManager::createTmp(MMFile*& memPtr, const size_t& fileSize)
    {
        MMFile* tmp = filePool.acquire();

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

        tmp->setID() = tmpID;
        tmp->setSysGran() = dwSysGran;
        tmp->setSysPageSize() = dwPageSize;
        tmp->setManager() = this;
        tmp->setFileHandle() = CreateFile(dir.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (tmp->setFileHandle() == INVALID_HANDLE_VALUE) {
            delete tmp;
            throw std::runtime_error("Failed to create temporary file: " + std::string(dir));
        }

        tmp->resize(fileSize);
        memPtr = tmp;
    }

    void MemoryManager::createPmnt(MMFile* memPtr, const size_t& fileSize)
    {
        MMFile& tmp = *memPtr;
    }

    void MemoryManager::memcopy_AVX2(MMFile*& _dst, void* _src, const size_t& _size)
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
        const size_t maxThreads = 100;
        const size_t totalChunks = (_size + granularity - 1) / granularity;
        const size_t numThreads = (std::min)(maxThreads, totalChunks);

        const size_t chunksPerThread = totalChunks / numThreads;
        const size_t remainderChunks = totalChunks % numThreads;

        std::vector<std::future<bool>> threads;
        threads.reserve(numThreads);

        size_t currentOffset = 0;

        for (size_t i = 0; i < numThreads; ++i) {
            size_t thisChunkCount = chunksPerThread + (i < remainderChunks ? 1 : 0);
            size_t chunkSize = thisChunkCount * granularity;

            size_t offset = currentOffset;
            currentOffset += chunkSize;

            size_t actualSize = (std::min)(chunkSize, _size - offset); // Clamp at the end

            auto tmp = workerPool->submit([this](MMFile* _dst, void* _src, size_t offset, size_t _size) {
                MemoryManager::copyThreadsRawPtr_AVX2(_dst, _src, offset, _size);
                return true;
                }, _dst, _src, offset, actualSize);
            threads.push_back(std::move(tmp));
        }

        // Ensure all of the tasks are done
        for (auto& thread : threads) {
            volatile bool tmp = thread.get();
        }
    }

    void MemoryManager::memcopy(MMFile*& _dst, void* _src, const size_t& _size)
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

        std::vector<std::future<bool>> threads;
        threads.reserve(numThreads);

        for (size_t i = 0; i < numThreads; ++i) {
            size_t offset = i * chunkSize;
            size_t chunkCpy = (std::min)(chunkSize, _size - offset);
            auto tmp = workerPool->submit([this](MMFile* _dst, void* _src, size_t offset, size_t _size) {
                MemoryManager::copyThreadsRawPtr(_dst, _src, offset, _size);
                return true;
                }, _dst, _src, offset, chunkCpy);
            threads.push_back(std::move(tmp));
        }

        // Ensure all of the tasks are done
        for (auto& thread : threads) {
            volatile bool tmp = thread.get();
        }
    }

    void MemoryManager::memcopy(MMFile*& _dst, MMFile* _src, const short& _typeSize, const size_t& _size)
    {
        Timer timer("MemoryManager::memcopy");
        if (_dst == nullptr) MemMng.createTmp(_dst, _size);
        std::unique_lock<std::shared_mutex> lockDst(*_dst->mutex);
        std::unique_lock<std::shared_mutex> lockSrc(*_src->mutex);

        if (_dst->getFileHandle() == nullptr)
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
        _dst->setFileHandle() = CreateFile(lpDstFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (_dst->getFileHandle() == nullptr)
            int e = GetLastError();

        _src->setFileHandle() = CreateFile(lpSrcFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        _dst->m_fileSize = GetFileSize(_dst->getFileHandle(), NULL);
        _dst->createMapObj();
        _src->createMapObj();
    }

    void MemoryManager::copyThreadsRawPtr(MMFile* _dst, void* _src, size_t offset, size_t _size)
    {
        MemView& dstView = _dst->load_s(offset, _size);
        memcpy(dstView.getPtr_s(), (char*)_src + offset, _size);
        _dst->unload_s(dstView);
    }

    void MemoryManager::copyThreadsRawPtr_AVX2(MMFile* _dst, void* _src, size_t offset, size_t _size)
    {
        // Load destination view
        MemView& dstView = _dst->load_s(offset, _size);

        uint8_t* dstPtr = (uint8_t*)dstView.getPtr_s();
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
        if(i < _size) memcpy(&dstPtr[i], &srcPtr[i], _size - i - 1);

        // Unload destination view
        _dst->unload_s(dstView);
    }

    void MemoryManager::move(MMFile* _dst, MMFile* _src)
    {
        HANDLE thisProcess = GetCurrentProcess();
        _dst->closeAllPtr();

        if (!DuplicateHandle(thisProcess, _src->getFileHandle(), thisProcess, &_dst->setFileHandle(), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            throw std::runtime_error("Failed to duplicate file handle.");
        }

        if (!DuplicateHandle(thisProcess, _src->getMapHandle(), thisProcess, &_dst->setMapHandle(), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            throw std::runtime_error("Failed to duplicate map handle.");
        }
        
        _dst->m_fileSize = _src->getFileSize();
        //_dst->m_AllocatedSize = _src->getAllocatedSize();
        _dst->m_fileID = _src->m_fileID;

        _src->closeAllPtr();
    }

    void MemoryManager::free(MMFile* ptr) {
        filePool.release(ptr);
    }

    uint32_t MemoryManager::calcCRC32(MMFile* _src) {
        const uint64_t totalChunks = (_src->getFileSize() + getSysGranularity() - 1) / getSysGranularity();
        const uint64_t fullChunks = _src->getFileSize() / getSysGranularity();
        
        auto task = [](MMFile* _src, uint64_t size, uint64_t offset) {
            thread_local CRC32_64 crc;
            MemView& view = _src->load(offset, size);
            crc.reset32();
            crc.appendCRC32((uint8_t*)view.getPtr(), size);
            crc.finallize32();
            _src->unload(view);
            return crc.getCRC32();
            };

        std::vector<std::future<uint32_t>> crcCache;
        crcCache.reserve(totalChunks);

        if (totalChunks > fullChunks)
        {
            auto crc = workerPool->submit(task, _src, _src->getFileSize() - fullChunks * getSysGranularity(), fullChunks * getSysGranularity());
            crcCache.push_back(std::move(crc));
        }
        for (int64_t i = fullChunks - 1; i >= 0; --i) {
            auto crc = workerPool->submit(task, _src, getSysGranularity(), i * getSysGranularity());
            crcCache.push_back(std::move(crc));
        }
        uint32_t crc = 0;
        try {
            crc = crcCache[0].get();
            for (uint64_t i = 1; i < totalChunks; ++i) {
                crc = CRC32_64::combineCRC32(crc, crcCache[i].get(), getSysGranularity());
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "Exception from future: " << ex.what() << '\n';
        }
        _src->getCRC().getCRC32() = crc;
        return crc;
    }

    uint64_t MemoryManager::calcCRC64(MMFile* _src) {
        const uint64_t totalChunks = (_src->getFileSize() + getSysGranularity() - 1) / getSysGranularity();
        const uint64_t fullChunks = _src->getFileSize() / getSysGranularity();

        auto task = [](MMFile* _src, uint64_t size, uint64_t offset) {
            thread_local CRC32_64 crc;
            MemView& view = _src->load(offset, size);
            crc.reset64();
            crc.appendCRC64((uint8_t*)view.getPtr(), size);
            crc.finallize64();
            _src->unload(view);
            return crc.getCRC64();
            };

        std::vector<std::future<uint64_t>> crcCache;
        crcCache.reserve(totalChunks);

        if (totalChunks > fullChunks)
        {
            auto crc = workerPool->submit(task, _src, _src->getFileSize() - fullChunks * getSysGranularity(), fullChunks * getSysGranularity());
            crcCache.push_back(std::move(crc));
        }
        for (int64_t i = fullChunks - 1; i >= 0; --i) {
            auto crc = workerPool->submit(task, _src, getSysGranularity(), i * getSysGranularity());
            crcCache.push_back(std::move(crc));
        }

        uint64_t crc = 0;
        try {
            crc = crcCache[0].get();
            for (uint64_t i = 1; i < totalChunks; ++i) {
                crc = CRC32_64::combineCRC64(crc, crcCache[i].get(), getSysGranularity());
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "Exception from future: " << ex.what() << '\n';
        }
        _src->getCRC().getCRC64() = crc;
        return crc;
    }

    //------ Memory File Pool --------

    MMFile* MemoryFilePool::acquire()
    {
            
        if (!filePool.empty()) {
            std::unique_lock<std::shared_mutex> lock(mutex);
            MMFile* memPtr = filePool.front();
            filePool.pop_front();
            return memPtr;
        }
        return new MMFile();
    }

    void MemoryFilePool::release(MMFile* ptr)
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