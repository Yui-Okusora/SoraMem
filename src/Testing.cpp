#define TESTING

#include <iostream>
#include <iomanip>
#include "MMFile/MMFile.hpp"
#include "MemoryManager/MemoryManager.hpp"
#include "CRC32_64/CRC32_64.hpp"

#include "Timer.hpp"

//#define test(a) ((a) ? "PASSED\n" : "FAILED\n")

uint16_t passCase = 0;
uint16_t failCase = 0;

auto test(bool a)
{
	(a ? passCase : failCase)++;
	return (a) ? "PASSED\n" : "FAILED\n";
}

auto& print = std::cout;

const uint64_t maxSize = 4 * 65536 + 100;

int main()
{
	using namespace SoraMem;
	print << "TEST starting\n";

	std::unique_ptr<ThreadPool> ManagerWorkerPool = std::make_unique<ThreadPool>(4);

	MemMng.initManager();
	MemMng.setThreadPool(ManagerWorkerPool);
	MemMng.setTmpDir("temp\\");
	CRC32_64::init();

	{
	print << std::setw(20) << std::left << "Tmp dir assignment: " << test(MemMng.tmpDir == "temp\\");
	print << std::setw(20) << std::left << "SYS Granularity: " << test(MemMng.dwSysGran == 65536);
	}

	

	MMFile* mmf = nullptr;
	MemMng.createTmp(mmf, maxSize);

	{
		print << std::setw(20) << std::left << "File Creation: " << test(mmf != nullptr && mmf->isValid());
#pragma warning(suppress: 6011)
		print << std::setw(20) << std::left << "File size: " << maxSize << " vs " << mmf->getFileSize() << "\n";
	}

	MemView& view = mmf->load(0, mmf->getFileSize());

	print << std::setw(20) << std::left << "View loaded: " << test(view.getViewOrigin() != nullptr);

	for (uint64_t i = 0; i < mmf->getFileSize() / 8; ++i)
	{
		view.at<uint64_t>(i) = i;
	}

	print << std::setw(20) << std::left << "Data loaded: " << test(view.at<uint64_t>(mmf->getFileSize() / 8 -1) == mmf->getFileSize() / 8 - 1);

	{
		Timer("CRC32");
		MemMng.calcCRC32(mmf);
		MemMng.calcCRC64(mmf);
		print << "CRC32: " << std::hex << mmf->getCRC32() << "\n";
		print << "CRC64: " << std::hex << mmf->getCRC64() << "\n";
	}

	MMFile* mmf2 = nullptr;
	MemMng.createTmp(mmf2, maxSize);

	print << std::setw(20) << std::left << "File Creation: " << test(mmf2 != nullptr && mmf2->isValid());

	for (uint8_t i = 0; i < 10; ++i) {
		Timer("memcopy avx2");
		MemMng.memcopy_AVX2(mmf2, view.getPtr(), mmf2->getFileSize());
	}
	print << "Copy completed\n";
	
	CRC32_64 crc;
	MemView& view2 = mmf2->load(0, mmf2->getFileSize());
	crc.reset();
	crc.appendCRC32((uint8_t*)view2.getPtr(), mmf2->getFileSize());
	crc.appendCRC64((uint8_t*)view2.getPtr(), mmf2->getFileSize());
	crc.finallize();

	print << "CRC32: " << std::hex << crc.getCRC32() << "\n";
	print << "CRC64: " << std::hex << crc.getCRC64() << "\n";

	MemMng.calcCRC32(mmf2);
	MemMng.calcCRC64(mmf2);

	print << "Testing combined CRC32: " << std::hex << mmf2->getCRC32() << "\n";
	print << "Testing combined CRC64: " << std::hex << mmf2->getCRC64() << "\n";

	print << std::setw(20) << "CRC: " << test(crc.getCRC32() == mmf2->getCRC32() && crc.getCRC64() == mmf2->getCRC64());

	print << "------ Passed: " << passCase << " --- Failed: " << failCase << " --------\n";
	return 0;
}