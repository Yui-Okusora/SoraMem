#pragma once
#include <cstdint>

namespace SoraMem
{
    enum SoraMemFlags : uint64_t	// Bitmasked flags for file
    {
        Compressed	= 0x00000001,	// Is compressed
        LZ4		    = 0x00000002,	// LZ4(1) or ZSTD(0) compression
        CRC		    = 0x00000004,	// Enabled integrity check
        CRC32		= 0x00000008,	// CRC32-ISO_HDLC(1) or CRC64-XZ(0) integrity check
        Encrypted	= 0x00000010	// Is encrypted
    };
    namespace SoraMemSubMagicNumber //Big-endian sub magic number
    {
        constexpr char CONF[8]		= { 'C','O','N','F',' ',' ',' ',' ' };	// Configuration file
        constexpr char DATA[8]		= { 'D','A','T','A',' ',' ',' ',' ' };	// Data file
        constexpr char SNAPLIST[8]	= { 'S','N','A','P','L','I','S','T' };	// Snapshots manager file
        constexpr char SNAPDATA[8]	= { 'S','N','A','P','D','A','T','A' };	// Snapshot file
    }

#pragma pack(push, 1)
    struct SoraMemFileDescriptor // (.bin).soramem
    {
        char		baseMagic[4] = { 'S','M','M','F' };	// Base magic number
        uint32_t	version;				            // Metadata format version
        uint64_t	chunkSize;				            // Size of data in bytes
        uint64_t	timestamp;				            // Unix timestamp
        uint64_t	flags;					            // Bitmasked flags
        uint64_t	crc;					            // CRC32-ISO_HDLC/CRC64-XZ

        char		subMagic[8];				        // Sub chunk magic number
        uint64_t	subChunkSize;				        // Sub chunk size in bytes

        uint8_t		alignment[64 - 56];			        // 64 bytes alignment header
    };

    struct SoraMemConfigFormat // .conf.soramem
    {
        uint64_t	maximumSaves;					    // Default maximum saves file (history)
        uint32_t	sysGran = 0;					    // System allocation granularity
        uint32_t	fileSizeAlignment = 0;				// File alignment
        char		temporaryDir[256];				    // Temporary data file directory
        char		permanentDir[256];				    // Permanent data file directory
        uint32_t	alignment1;					        // Alignment
        uint32_t	crc32Poly;					        // CRC32-ISO_HDLC polynomial (reflected)
        uint64_t	crc64Poly;					        // CRC64-XZ polynomial (reflected)
        uint32_t	crc32_8bitLookup[256];				// CRC32 8bit lookup table
        uint64_t	crc64_8bitLookup[256];				// CRC64 8bit lookup table
        uint32_t	crc32_16bitLookup[65536];			// CRC32 16bit lookup table
        uint64_t	crc64_16bitLookup[65536];			// CRC64 16bit lookup table
        
        uint8_t		alignment2[851968 - 790112];        // 832KB alignment (this struct + 64 bytes header)
    };

    struct SoraMemSnapshotListFormat // .sl.soramem
    {
        uint64_t	maximumSaves;				        // Maximum saves file (history)
        char		originFileName[256];                // Owner file of this list
        uint64_t	currentID;					        // Current save ID
        uint64_t	currentOffset;			            // Current save offset pos
        uint64_t	lastID;						        // Last save ID
        uint64_t	lastOffset;					        // Last save offset pos

        uint8_t		alignment[320 - 296];
    };

    struct SoraMemSnapshotListItemFormat
    {
        uint64_t	snapshotID;					        // Snapshot (save) ID
        uint64_t	offset;						        // Offset position
        uint64_t	lastSnapshotID;				        // Last save ID
        uint64_t	lastOffset;					        // Last save offset position
    };

    struct SoraMemSnapshotChunkFormat // .si.soramem
    {
        uint64_t originOffset;						    // Original data offset position in source file
        uint64_t originSize;						    // Original data size
        uint64_t snapshotOffset;					    // Snapshot offset position
        uint64_t snapshotSize;						    // Snapshot size
    };
#pragma pack(pop)
}
