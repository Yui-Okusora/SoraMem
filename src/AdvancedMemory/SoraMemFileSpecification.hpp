#pragma once
#include <cstdint>

namespace SoraMem
{
	enum SoraMemFlags : uint32_t
	{
		Compressed	= 0x0001,
		LZ4			= 0x0002, // LZ4(1) or ZSTD(0) compression
		CRC			= 0x0004, // Enabled integrity check
		CRC32		= 0x0008, // CRC32-ISO_HDLC(1) or CRC64-XZ(0) integrity check
		Encrypted	= 0x0010
	};
	namespace SoraMemSubMagicNumber //Big-endian
	{
		constexpr char CONF[8]		= { 'C','O','N','F',' ',' ',' ',' ' };
		constexpr char DATA[8]		= { 'D','A','T','A',' ',' ',' ',' ' };
		constexpr char SNAPLIST[8]	= { 'S','N','A','P','L','I','S','T' };
		constexpr char SNAPDATA[8]	= { 'S','N','A','P','D','A','T','A' };
	}

//#pragma pack(push, 1)
	struct SoraMemFileDescriptor
	{
		char		baseMagic[4] = { 'S','M','M','F' };	// Base magic number
		uint32_t	version;							// Metadata format version
		uint64_t	chunkSize;							// Size of data in bytes
		uint64_t	timestamp;							// Unix timestamp
		uint64_t	flags;								// Bitmasked flags
		uint64_t	crc;								// CRC32-ISO_HDLC/CRC64-XZ

		char		subMagic[8];
		uint64_t	subChunkSize;



		uint8_t reserved[64 - 56];
	};
//#pragma pack(pop)
	struct SoraMemSnapshotListFormat
	{

	};
	struct SoraMemSnapshotListItemFormat
	{
		uint64_t	snapshotID;
		char		nextSnapshotFileName[256];
		char		prevSnapshotFileName[256];
		uint64_t	snapshotOffset;
		uint64_t	snapshotLength;
	};
}