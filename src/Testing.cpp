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

int main()
{
	using namespace SoraMem;
	print << "TEST starting\n";

	std::unique_ptr<ThreadPool> ManagerWorkerPool = std::make_unique<ThreadPool>(2);

	MemMng.initManager();
	MemMng.setThreadPool(ManagerWorkerPool);
	MemMng.setTmpDir("temp\\");
	CRC32_64::init();

	{
	print << std::setw(20) << std::left << "Tmp dir assignment: " << test(MemMng.tmpDir == "temp\\");
	print << std::setw(20) << std::left << "SYS Granularity: " << test(MemMng.dwSysGran == 65536);
	}

	MMFile* mmf = nullptr;
	MemMng.createTmp(mmf, 1023);

	CRC32_64 crc;

	{
		print << std::setw(20) << std::left << "File Creation: " << test(mmf != nullptr && mmf->isValid());
		print << std::setw(20) << std::left << "File size: " << 1023 << " vs " << mmf->getFileSize() << "\n";
	}

	MemView& view = mmf->load(0, 1024);

	print << std::setw(20) << std::left << "View loaded: " << test(view.getViewOrigin() != nullptr);

	for (uint16_t i = 0; i < mmf->getFileSize() / 4; ++i)
	{
		view.at<uint16_t>(i) = i;
	}

	print << std::setw(20) << std::left << "Data loaded: " << test(view.at<uint16_t>(mmf->getFileSize() / 4 -1) == mmf->getFileSize() / 4 - 1);

	{
		Timer("CRC32");
		crc.reset();
		crc.appendCRC32((uint8_t*)view.getPtr(), 1024);
		crc.appendCRC64((uint8_t*)view.getPtr(), 1024);
		crc.finallize();
		print << "CRC32: " << std::hex << crc.getCRC32() << "\n";
		print << "CRC64: " << std::hex << crc.getCRC64() << "\n";
	}

	MMFile* mmf2 = nullptr;
	MemMng.createTmp(mmf2, 1023);

	print << std::setw(20) << std::left << "File Creation: " << test(mmf2 != nullptr && mmf2->isValid());

	for (uint8_t i = 0; i < 10; ++i) {
		Timer("memcopy avx2");
		MemMng.memcopy_AVX2(mmf2, view.getPtr(), 1024);
	}
	print << "Copy completed\n";
	
	MemView& view2 = mmf2->load(0, 1024);
	crc.reset();
	crc.appendCRC32((uint8_t*)view2.getPtr(), 1024);
	crc.appendCRC64((uint8_t*)view2.getPtr(), 1024);
	crc.finallize();
	print << "CRC32: " << std::hex << crc.getCRC32() << "\n";
	print << "CRC64: " << std::hex << crc.getCRC64() << "\n";

	crc.reset();
	crc.appendCRC64((uint8_t*)view2.getPtr() + 512, 512);
	crc.appendCRC32((uint8_t*)view2.getPtr() + 512, 512);
	crc.finallize();
	uint64_t crc641 = crc.getCRC64();
	uint32_t crc321 = crc.getCRC32();
	crc.reset();
	crc.appendCRC64((uint8_t*)view2.getPtr(), 512);
	crc.appendCRC32((uint8_t*)view2.getPtr(), 512);
	crc.finallize();
	uint64_t crc642 = crc.getCRC64();
	uint32_t crc322 = crc.getCRC32();

	print << "Testing combined CRC32: " << crc.combineCRC32(crc321, crc322, 512) << "\n";
	print << "Testing combined CRC64: " << crc.combineCRC64(crc641, crc642, 512) << "\n";

	print << "------ Passed: " << passCase << " --- Failed: " << failCase << " --------\n";
	return 0;
}