#include <iostream>
#include "AdvancedMemory/AdvancedMemory.hpp"
#include "MemoryManager/MemoryManager.hpp"
#include "MemoryManager/CRC.hpp"

#include "Timer.hpp"

#include <array>



int main()
{
	MemMng.initManager();
	MemMng.setTmpDir("temp\\");

	const uint64_t message = 0x1238AEBD2BED2983;
	const uint8_t test[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };

	using namespace SoraMem::CRC;

	//std::cout << std::hex << test << std::endl;
	std::cout << std::hex << crc_lut((uint8_t*)(&test), 9, table64) << std::endl;
	std::cout << std::hex << crc_lut((uint8_t*)(&message), 8, table32);
}