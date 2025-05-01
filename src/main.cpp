#include <iostream>
#include "AdvancedMemory/AdvancedMemory.hpp"
#include "MemoryManager/MemoryManager.hpp"

#include "Timer.hpp"

#include <array>

bool isLittleEndian() {
	uint16_t num = 1;
	return *reinterpret_cast<uint8_t*>(&num) == 1;
}

constexpr uint8_t reflect_byte(uint8_t b) {
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return b;
}

constexpr uint32_t reflect32(uint32_t x) {
	x = (x >> 1) & 0x55555555 | (x & 0x55555555) << 1;
	x = (x >> 2) & 0x33333333 | (x & 0x33333333) << 2;
	x = (x >> 4) & 0x0F0F0F0F | (x & 0x0F0F0F0F) << 4;
	x = (x >> 8) & 0x00FF00FF | (x & 0x00FF00FF) << 8;
	return (x >> 16) | (x << 16);
}

constexpr uint64_t reflect64(uint64_t x) {
	x = ((x & 0x5555555555555555ULL) << 1) | ((x & 0xAAAAAAAAAAAAAAAAULL) >> 1);
	x = ((x & 0x3333333333333333ULL) << 2) | ((x & 0xCCCCCCCCCCCCCCCCULL) >> 2);
	x = ((x & 0x0F0F0F0F0F0F0F0FULL) << 4) | ((x & 0xF0F0F0F0F0F0F0F0ULL) >> 4);
	x = ((x & 0x00FF00FF00FF00FFULL) << 8) | ((x & 0xFF00FF00FF00FF00ULL) >> 8);
	x = ((x & 0x0000FFFF0000FFFFULL) << 16) | ((x & 0xFFFF0000FFFF0000ULL) >> 16);
	return (x << 32) | (x >> 32);
}

const uint32_t crc32poly = reflect32(0x04C11DB7); //0xEDB88320
const uint64_t crc64poly = reflect64(0x42F0E1EBA9EA3693); //0xC96C5795D7870F42

template<typename T>
std::array<T, 256> calcLUT(T poly)
{
	std::array<T, 256> table;
	for (int i = 0; i < 256; ++i) {
		T crc = i;
		for (int j = 0; j < 8; ++j) {
			if (crc & 1) {
				crc = (crc >> 1) ^ poly;
			}
			else {
				crc >>= 1;
			}
		}
		table[i] = crc;
	}
	return table;
}

template<typename T>
T crc_lut(const uint8_t* data, size_t len, std::array<T, 256>& table) {
	T crc = (sizeof(T) == 4) ? 0xFFFFFFFF : 0xFFFFFFFFFFFFFFFF;
	for (size_t i = 0; i < len; ++i) {
		const uint8_t& val = data[len - 1 - i];
		crc = (crc >> 8) ^ table[(crc ^ val) & 0xFF];
	}

	crc = crc ^ ((sizeof(T) == 4) ? 0xFFFFFFFF : 0xFFFFFFFFFFFFFFFF);

	return crc;
}

int main()
{
	MemMng.initManager();
	MemMng.setTmpDir("temp\\");

	using namespace SoraMem;

	const uint64_t message = 0x1238AEBD2BED2983;
	const uint8_t test[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };

	auto table32 = calcLUT(crc32poly);
	auto table64 = calcLUT(crc64poly);

	std::cout << std::hex << test << std::endl;
	std::cout << std::hex << crc_lut((uint8_t*)(&test), 9, table64) << std::endl;
	std::cout << std::hex << crc_lut((uint8_t*)(&message), 8, table32);
}