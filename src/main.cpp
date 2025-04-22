#include <iostream>
#include "AdvancedMemory/AdvancedMemory.hpp"
#include "MemoryManager/MemoryManager.hpp"

#include "Timer.hpp"

int main()
{
	MemMng.initManager();
	MemMng.setTmpDir("temp\\");

	using namespace SoraMem;

	AdvancedMemory* admem1, * admem2;

	constexpr int size = 4096 * 10;

	MemMng.createTmp(admem1, size);
	MemMng.createTmp(admem2, size);

	ViewOfAdvancedMemory& view1 = admem1->load(0, size);
	for (int i = 1; i <= (size / sizeof(int)); ++i)
	{
		admem1->refAt<int>(i - 1, view1) = i;
	}
	{
		Timer timer("memcopy");
		MemMng.memcopy(admem2, admem1->getViewPtr(view1), size);
	}

	{
		Timer timer("memcopy_AVX2");
		MemMng.memcopy_AVX2(admem2, admem1->getViewPtr(view1), size);
	}
	admem1->unload(view1);
}