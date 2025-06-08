#define TESTING

#include <iostream>
#include <iomanip>
#include "MMFile/MMFile.hpp"
#include "MemoryManager/MemoryManager.hpp"
#include "MemoryManager/CRC.hpp"

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

	MemMng.initManager();
	MemMng.setTmpDir("temp\\");

	{
	print << std::setw(20) << std::left << "Tmp dir assignment: " << test(MemMng.tmpDir == "temp\\");
	print << std::setw(20) << std::left << "SYS Granularity: " << test(MemMng.dwSysGran == 65536);
	}

	MMFile* mmf = nullptr;
	MemMng.createTmp(mmf, 1023);

	{
		print << std::setw(20) << std::left << "File Creation: " << test(mmf != nullptr && mmf->isValid());
		print << std::setw(20) << std::left << "File size: " << 1023 << " vs " << mmf->getFileSize() << "\n";
	}

	MemView& view = mmf->load(0, 1024);

	print << std::setw(20) << std::left << "View loaded: " << test(view.lpMapAddress != nullptr);

	for (uint16_t i = 0; i < mmf->getFileSize() / 4; ++i)
	{
		view.at<uint16_t>(i) = i;
	}

	print << std::setw(20) << std::left << "Data loaded: " << test(view.at<uint16_t>(mmf->getFileSize() / 4 -1) == mmf->getFileSize() / 4 - 1);

	{
		Timer("CRC32");
		print << "CRC32: " << std::hex << CRC::crc_lut((uint8_t*)view.getPtr(), 1024, CRC::table32) << "\n";
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
	print << "CRC32: " << std::hex << CRC::crc_lut((uint8_t*)view2.getPtr(), 1024, CRC::table32) << "\n";


	print << "------ Passed: " << passCase << " --- Failed: " << failCase << " --------\n";
	return 0;
}