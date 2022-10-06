#pragma once
#include "Platform/Core/MemoryInterface.h"
#include "Platform/Core/Export.h"
#include <map>
#include <string>
#include <iostream>

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif

namespace platform
{
	namespace core
	{
		/// A pseudo allocator that tracks video memory but does not actually allocate it.
		class PLATFORM_CORE_EXPORT TrackingAllocator :
			public platform::core::MemoryInterface
		{
			size_t maxVideoAllocated;
			size_t totalVideoAllocated;
			size_t totalVideoFreed;
			std::map<const void*,size_t> memBlocks;
			std::map<const void*, size_t> gpuMemBlocks;
			std::map<std::string, size_t> memoryTracks;
			std::map<const void *,std::string> allocationNames;
			bool show_output;
			bool active;
			mutable bool debugTextValid=true;
			mutable std::string debugText;
		public:
			TrackingAllocator();
			virtual ~TrackingAllocator();
			//! Allocate nbytes bytes of memory, aligned to align and return a pointer to them.
			virtual void* AllocateTracked(size_t nbytes,size_t align,const char *fn) override;
			//! De-allocate the memory at \param address (requires that this memory was allocated with Allocate()).
			virtual void Deallocate(void* ptr) override;
			//! Track (but don't allocate) nbytes bytes of memory.
			virtual void TrackVideoMemory(const void* ptr,size_t nbytes,const char *fn) override;
			//! Free the pointer from video memory tracking.
			virtual void UntrackVideoMemory(const void* ptr) override;
			 
			const char *GetNameAtIndex(int index) const override;
			size_t GetBytesAllocated(const char *name) const override;
			size_t GetTotalBytesAllocated() const override;
			virtual size_t GetTotalVideoBytesAllocated() const override;
			virtual size_t GetTotalVideoBytesFreed() const override;
			virtual size_t GetCurrentVideoBytesAllocated() const override;
			const char *GetDebugText() const;
			//! Shut down and report any leaks.
			void Shutdown();
			void UpdateDebugText() const;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif
