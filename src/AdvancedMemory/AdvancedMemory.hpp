#pragma once

#define NeuroSoraCore_AdvancedMemory

#include <unordered_map>
#include <list>
#include <Windows.h>

#include "src/MemoryManager/MemoryManager.hpp"

namespace SoraMem
{
	enum SoraMemMagicNumber : uint64_t
	{
		SMMF = 0x464D4D53,
		SMMFCONF = ((uint64_t)0x464E4F43 << 32) | SMMF,
		SMMFSNAP = ((uint64_t)0x50414E53 << 32) | SMMF,
		SMMFDATA = ((uint64_t)0x41544144 << 32) | SMMF
	};

#pragma pack(push, 1)
	struct SoraMemFileDescriptor
	{
		uint64_t	magic;				// Magic number defined in enum
		uint64_t	dataSize;			// Size of data in bytes
		uint64_t	timestamp;			// Unix timestamp
		uint32_t	version;			// Metadata format version

		uint8_t		compressionType;	// 0 = None, 1 = zstd, 2 = lz4
		uint8_t		isCompressed;		// 0 = Not compressed, 1 = Compressed
		uint8_t		encryptionType;		// 0 = None, 1 = AES-256 (not supported)
		uint8_t		isEncrypted;		// 0 = Not encrypted, 1 = Encrypted
		uint32_t	flags;				// Bitmasked flags
		uint32_t	reservedFlags;		// Reserved for future flags

		char		nextSnapshotFileName[256];
		char		prevSnapshotFileName[256];
		uint64_t	snapshotID;
		uint64_t	snapshotOffset;
		uint64_t	snapshotLength;

		char		description[128];	// Optional description or label (null-terminated)

		uint64_t	crc;				// CRC32/64


		uint8_t reserved[1024 - 712];
	};
#pragma pack(pop)

	class ViewOfAdvancedMemory;

	class MMFViewPool
	{
	public:
		ViewOfAdvancedMemory* acquire();

		inline void release(ViewOfAdvancedMemory* ptr);
		inline void clear();

		inline size_t size();
	private:
		std::list<ViewOfAdvancedMemory*> viewPool;
	};

	class AdvancedMemory
	{
	public:
		friend class MemoryManager;

		ViewOfAdvancedMemory&	load(size_t offset, size_t size); // offset and size in bytes
		void					unload(ViewOfAdvancedMemory& view);
		void					unloadAll();
		void					resize(const size_t& fileSize); // in bytes
		void					reset();

		void*					getViewPtr(const ViewOfAdvancedMemory& view) const;

		bool					isValid() const;
		const size_t&			getFileSize() const;
		const size_t&			getAllocatedSize() const;

		inline size_t			getID() const;
		inline void				createMapObj();


		template<typename T>
		T& refAt(const size_t& index, const ViewOfAdvancedMemory& view) // recommended using
		{
			if (index >= (static_cast<size_t>(view.dwMapViewSize) - view.iViewDelta) / sizeof(T)) {
				throw std::out_of_range("Index out of range");
			}
			return *(reinterpret_cast<T*>(getViewPtr(view)) + index);
		}

		~AdvancedMemory();


		//--------- Thread-safe methods ----------

		ViewOfAdvancedMemory&	load_s(size_t offset, size_t size); // offset and size in bytes

		void					unload_s(ViewOfAdvancedMemory& view);
		void					unloadAll_s();
		void					resize_s(const size_t& fileSize); // in bytes
		void					createMapObj_s();

		void*					getViewPtr_s(const ViewOfAdvancedMemory& view) const;

		inline const size_t&	getFileSize_s() const;
		inline size_t			getID_s() const;


		template<typename T>
		T& refAt_s(const size_t& index, const ViewOfAdvancedMemory& view)
		{
			std::unique_lock<std::shared_mutex> lock(*view.mutex);
			if (index >= (static_cast<size_t>(view.dwMapViewSize) - view.iViewDelta) / sizeof(T)) {
				throw std::out_of_range("Index out of range");
			}
			return *(reinterpret_cast<T*>(getViewPtr(view)) + index);
		}

		template<typename T>
		T readAt(const size_t& index, const ViewOfAdvancedMemory& view) const
		{
			std::shared_lock<std::shared_mutex> lock(*view.mutex);
			if (index >= (static_cast<size_t>(view.dwMapViewSize) - view.iViewDelta) / sizeof(T)) {
				throw std::out_of_range("Index out of range");
			}
			return *(reinterpret_cast<T*>(getViewPtr(view)) + index);
		}

	private:
		ViewOfAdvancedMemory&	_load(ViewOfAdvancedMemory& view, size_t offset, size_t size);

		void					closeAllPtr();
		void					closeAllPtr_s();

		HANDLE& getFileHandle() { return m_hFile; }
		HANDLE& getMapHandle() { return m_hMapFile; }

		HANDLE m_hMapFile = NULL;     // handle for the file's memory-mapped region
		HANDLE m_hFile = NULL;        // the file handle

		size_t m_FileSize = 0;		// temporary storage for file sizes  
		size_t m_AllocatedSize = 0;
		size_t m_fileID = 0;

		std::unordered_map<LPVOID, ViewOfAdvancedMemory> views;
		std::shared_ptr<std::shared_mutex> mutex = std::make_shared<std::shared_mutex>();
	};

	class ViewOfAdvancedMemory
	{
	public:
		ViewOfAdvancedMemory& operator=(const ViewOfAdvancedMemory& view)
		{
			lpMapAddress = view.lpMapAddress;
			dwMapViewSize = view.dwMapViewSize;
			iViewDelta = view.iViewDelta;
			_offset = view._offset;
			parent = view.parent;
			autoRelease = view.autoRelease;
			return *this;
		}

		friend class AdvancedMemory;

		LPVOID					lpMapAddress	= nullptr;		// first address of the mapped view
		DWORD					dwMapViewSize	= 0;			// the size of the view
		unsigned long			iViewDelta		= 0;
		unsigned long long		_offset			= 0;

		AdvancedMemory* parent = nullptr;
		long long autoRelease = 0;

		~ViewOfAdvancedMemory()
		{
			if (autoRelease != 0 && parent != nullptr)
			{
				parent->unload_s(*this);
			}
		}
	private:
		std::shared_ptr<std::shared_mutex> mutex = std::make_shared<std::shared_mutex>();
	};
}





