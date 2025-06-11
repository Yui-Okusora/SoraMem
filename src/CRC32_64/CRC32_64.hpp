#pragma once
#include <array>

class CRC32_64
{
public:
	CRC32_64() { reset(); }
	
	static void init(uint32_t _poly32 = 0x04C11DB7, uint64_t _poly64 = 0x42F0E1EBA9EA3693)
	{
		if (poly32 == 0) poly32 = reflect32(_poly32);
		if (poly64 == 0) poly64 = reflect64(_poly64);
		if (table32[1] == 0) {
			table32 = calcLUT(poly32);
			crc32_init_shift_matrix();
		}
		if (table64[1] == 0) {
			table64 = calcLUT(poly64);
			crc64_init_shift_matrix();
		}
	}

	constexpr static uint8_t reflect_byte(uint8_t b) {
		b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
		b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
		b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
		return b;
	}
	constexpr static uint32_t reflect32(uint32_t x) {
		x = (x >> 1) & 0x55555555 | (x & 0x55555555) << 1;
		x = (x >> 2) & 0x33333333 | (x & 0x33333333) << 2;
		x = (x >> 4) & 0x0F0F0F0F | (x & 0x0F0F0F0F) << 4;
		x = (x >> 8) & 0x00FF00FF | (x & 0x00FF00FF) << 8;
		return (x >> 16) | (x << 16);
	}
	constexpr static uint64_t reflect64(uint64_t x) {
		x = ((x & 0x5555555555555555ULL) << 1) | ((x & 0xAAAAAAAAAAAAAAAAULL) >> 1);
		x = ((x & 0x3333333333333333ULL) << 2) | ((x & 0xCCCCCCCCCCCCCCCCULL) >> 2);
		x = ((x & 0x0F0F0F0F0F0F0F0FULL) << 4) | ((x & 0xF0F0F0F0F0F0F0F0ULL) >> 4);
		x = ((x & 0x00FF00FF00FF00FFULL) << 8) | ((x & 0xFF00FF00FF00FF00ULL) >> 8);
		x = ((x & 0x0000FFFF0000FFFFULL) << 16) | ((x & 0xFFFF0000FFFF0000ULL) >> 16);
		return (x << 32) | (x >> 32);
	}

	void reset() {
		reset32();
		reset64();
	}

	void reset32() { crc32 = 0xFFFFFFFF; }
	void reset64() { crc64 = 0xFFFFFFFFFFFFFFFF; }

	void finallize() {
		finallize32();
		finallize64();
	}
	void finallize32() { crc32 ^= 0xFFFFFFFF; }
	void finallize64() { crc64 ^= 0xFFFFFFFFFFFFFFFF; }

	uint32_t& getCRC32() { return crc32; }
	uint64_t& getCRC64() { return crc64; }

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

	uint32_t appendCRC32(const uint8_t* data, size_t len);

	uint64_t appendCRC64(const uint8_t* data, size_t len);

	static uint64_t combineCRC64(uint64_t crc1, uint64_t crc2, size_t len2);

	static uint32_t combineCRC32(uint32_t crc1, uint32_t crc2, size_t len2);

private:
	using Matrix64 = std::array<uint64_t, 64>;
	using Matrix32 = std::array<uint32_t, 32>;

	template<typename T = uint32_t>
	static std::array<T, 256> calcLUT(T poly)
	{
		std::array<T, 256> table;
		for (int i = 0; i < 256; ++i) {
			T crc = static_cast<T>(i);
			for (int j = 0; j < 8; ++j) {
				if (crc & 1) {
					crc = (crc >> 1) ^ poly;
				}
				else {
					crc >>= 1;
				}
			}
#pragma warning(suppress: 28020)
			table[i] = crc;
		}
		return table;
	}

	static uint64_t gf2_matrix_times(const Matrix64& mat, uint64_t vec);

	static uint32_t gf2_matrix_times(const Matrix32& mat, uint32_t vec);

	static void gf2_matrix_square(Matrix64& square, const Matrix64& mat);

	static void gf2_matrix_square(Matrix32& square, const Matrix32& mat);

	static void crc64_init_shift_matrix();

	static void crc32_init_shift_matrix();

	static uint64_t crc64_shift(uint64_t crc, size_t len_bytes);

	static uint32_t crc32_shift(uint32_t crc, size_t len_bytes);

	static std::array<uint32_t, 256> table32;
	static std::array<uint64_t, 256> table64;
	static std::array<Matrix64, 64> crc64_shift_matrices;
	static std::array<Matrix32, 32> crc32_shift_matrices;

	static uint32_t poly32;
	static uint64_t poly64;
	uint32_t crc32 = 0;
	uint64_t crc64 = 0;
};

