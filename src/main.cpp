#include <iostream>
#include "AdvancedMemory/AdvancedMemory.hpp"
#include "MemoryManager/MemoryManager.hpp"

#include "Timer.hpp"

bool isLittleEndian() {
	uint16_t num = 1;
	return *reinterpret_cast<uint8_t*>(&num) == 1;
}

int main()
{
	MemMng.initManager();
	MemMng.setTmpDir("temp\\");

	using namespace SoraMem;

	AdvancedMemory* admem1, * admem2;

	constexpr int size = 4096 * 10;

	MemMng.createTmp(admem1, size);
	MemMng.createTmp(admem2, size);

	SoraMemFileDescriptor des;
	des.magic = SoraMemMagicNumber::SMMFDATA;

	//MemMng.memcopy(admem1, &des, sizeof(des));

	ViewOfAdvancedMemory& view1 = admem1->load(0, size);
	memcpy(admem1->getViewPtr(view1), &(des.magic), 8);
	/*for (int i = 1; i <= (size / sizeof(int)); ++i)
	{
		admem1->refAt<int>(i - 1, view1) = i;
	}*/
	{
		Timer timer("memcopy");
		MemMng.memcopy(admem2, admem1->getViewPtr(view1), size);
	}

	{
		Timer timer("memcopy_AVX2");
		MemMng.memcopy_AVX2(admem2, admem1->getViewPtr(view1), size);
	}
	std::cout << reinterpret_cast<char*>(&(admem1->refAt<uint64_t>(0, view1)));
	admem1->unload(view1);

	
}