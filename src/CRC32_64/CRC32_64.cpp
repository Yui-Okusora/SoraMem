#include "CRC32_64.hpp"

using Matrix64 = std::array<uint64_t, 64>;
using Matrix32 = std::array<uint32_t, 32>;

std::array<uint32_t, 256> CRC32_64::table32;
std::array<uint64_t, 256> CRC32_64::table64;
std::array<Matrix64, 64> CRC32_64::crc64_shift_matrices;
std::array<Matrix32, 32> CRC32_64::crc32_shift_matrices;

uint32_t CRC32_64::poly32 = 0;
uint64_t CRC32_64::poly64 = 0;

uint32_t CRC32_64::appendCRC32(const uint8_t* data, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		const uint8_t& val = data[len - 1 - i];
		crc32 = (crc32 >> 8) ^ table32[(crc32 ^ val) & 0xFF];
	}
	return crc32;
}

uint64_t CRC32_64::appendCRC64(const uint8_t* data, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		const uint8_t& val = data[len - 1 - i];
		crc64 = (crc64 >> 8) ^ table64[(crc64 ^ val) & 0xFF];
	}
	return crc64;
}

uint64_t CRC32_64::gf2_matrix_times(const Matrix64& mat, uint64_t vec) {
	uint64_t sum = 0;
	for (int i = 0; vec; ++i, vec >>= 1)
		if (vec & 1) sum ^= mat[i];
	return sum;
}

uint32_t CRC32_64::gf2_matrix_times(const Matrix32& mat, uint32_t vec) {
	uint32_t sum = 0;
	for (int i = 0; vec; ++i, vec >>= 1)
		if (vec & 1) sum ^= mat[i];
	return sum;
}

void CRC32_64::gf2_matrix_square(Matrix64& square, const Matrix64& mat) {
	for (int i = 0; i < 64; ++i)
		square[i] = gf2_matrix_times(mat, mat[i]);
}

void CRC32_64::gf2_matrix_square(Matrix32& square, const Matrix32& mat) {
	for (int i = 0; i < 32; ++i)
		square[i] = gf2_matrix_times(mat, mat[i]);
}


void CRC32_64::crc64_init_shift_matrix() {
	Matrix64 mat;

	// Initial matrix: x^1 mod poly
	mat[0] = poly64;
	for (int i = 1; i < 64; ++i)
		mat[i] = 1ULL << (i - 1);

	crc64_shift_matrices[0] = mat;

	for (int i = 1; i < 64; ++i) {
		Matrix64 next;
		gf2_matrix_square(next, crc64_shift_matrices[i - 1]);
		crc64_shift_matrices[i] = next;
	}
}

void CRC32_64::crc32_init_shift_matrix() {
	Matrix32 mat;

	// Initial matrix: x^1 mod poly
	mat[0] = poly32;
	for (int i = 1; i < 32; ++i)
		mat[i] = 1ULL << (i - 1);

	crc32_shift_matrices[0] = mat;

	for (int i = 1; i < 32; ++i) {
		Matrix32 next;
		gf2_matrix_square(next, crc32_shift_matrices[i - 1]);
		crc32_shift_matrices[i] = next;
	}
}

uint64_t CRC32_64::crc64_shift(uint64_t crc, size_t len_bytes) {
	size_t n = len_bytes * 8;  // convert to bits
	for (int i = 0; n; ++i, n >>= 1)
		if (n & 1)
			crc = gf2_matrix_times(crc64_shift_matrices[i], crc);
	return crc;
}

uint32_t CRC32_64::crc32_shift(uint32_t crc, size_t len_bytes) {
	size_t n = len_bytes * 8;  // convert to bits
	for (int i = 0; n; ++i, n >>= 1)
		if (n & 1)
			crc = gf2_matrix_times(crc32_shift_matrices[i], crc);
	return crc;
}

uint64_t CRC32_64::combineCRC64(uint64_t crc1, uint64_t crc2, size_t len2) {
	return CRC32_64::crc64_shift(crc1, len2) ^ crc2;
}

uint32_t CRC32_64::combineCRC32(uint32_t crc1, uint32_t crc2, size_t len2) {
	return crc32_shift(crc1, len2) ^ crc2;
}