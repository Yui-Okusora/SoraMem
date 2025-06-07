#define TESTING

#include <iostream>
#include "MMFile/MMFile.hpp"
#include "MemoryManager/MemoryManager.hpp"
#include "MemoryManager/CRC.hpp"

#include "Timer.hpp"

#define test(a) ((a) ? "PASSED\n" : "FAILED\n")

auto& print = std::cout;

int main()
{
	using namespace SoraMem;
	print << "TEST starting\n";

	MemMng.initManager();
	MemMng.setTmpDir("temp\\");
	
	print << "Tmp dir assignment: " << test(MemMng.tmpDir == "temp\\");
	print << "SYS Granularity: " << test(MemMng.dwSysGran == 65536);
	
	MMFile* mmf = nullptr;
	MemMng.createTmp(mmf, 1023);

	print << "File Creation: " << test(mmf != nullptr && mmf->isValid());
	print << "File size: " << 1023 << " vs " << mmf->getFileSize() << "\n";
	MemView& view = mmf->load(0, 1024);

	print << "View loaded: " << test(view.lpMapAddress != nullptr);

	for (uint16_t i = 0; i < mmf->getFileSize() / 4; ++i)
	{
		view.at<uint16_t>(i) = i;
	}

	print << "Data loaded: " << test(mmf->refAt<uint16_t>(mmf->getFileSize() / 4 -1, view) == mmf->getFileSize() / 4 - 1);

	{
		Timer("CRC32");
		print << "CRC32: " << std::hex << CRC::crc_lut((uint8_t*)view.getPtr(), 1024, CRC::table32) << "\n";
	}

	MMFile* mmf2 = nullptr;
	MemMng.createTmp(mmf2, 1023);

	print << "File Creation: " << test(mmf2 != nullptr && mmf2->isValid());

	for (uint8_t i = 0; i < 10; ++i) {
		Timer("memcopy avx2");
		MemMng.memcopy_AVX2(mmf2, view.getPtr(), 1024);
	}
	print << "Copy completed\n";
	MemView& view2 = mmf2->load(0, 1024);
	print << "CRC32: " << std::hex << CRC::crc_lut((uint8_t*)view2.getPtr(), 1024, CRC::table32) << "\n";

	return 0;
}