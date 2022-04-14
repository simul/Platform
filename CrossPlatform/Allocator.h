#pragma once
#include "Platform/Core/MemoryInterface.h"
#include <mutex>
#include <map>

namespace platform
{
	namespace crossplatform
	{
	//! A passthrough memory allocator which does not release memory immediately, instead allowing allocations a number of frames to age out.
		class Allocator:public platform::core::MemoryInterface
		{
			std::map<void*,size_t> to_free_video;
			std::map<void*, size_t> to_free_cpu;
			size_t max_age=8;
			platform::core::MemoryInterface* memoryInterface =nullptr;
			mutable std::mutex mutex;
		public:
			Allocator();
			~Allocator();
			//! Set max age for blocks to be freed.
			void SetMaxAge(size_t m);
			//! Free all memory still being held.
			void Shutdown();
			//! Age all memory blocks by 1. Free any blocks that are old enough.
			void CheckForReleases();
			void SetExternalAllocator(platform::core::MemoryInterface *m);
			void* AllocateTracked(size_t nbytes,size_t align,const char *fn) override;
			void Deallocate(void* ptr)override;
			void* AllocateVideoMemoryTracked(size_t nbytes,size_t align,const char *fn)override;
			void DeallocateVideoMemory(void* address) override;
			const char *GetNameAtIndex(int index) const override;
			size_t GetBytesAllocated(const char *name) const override;
			size_t GetTotalBytesAllocated() const override;
			size_t GetCurrentVideoBytesAllocated() const override;
			size_t GetTotalVideoBytesAllocated() const override;
			size_t GetTotalVideoBytesFreed() const override;
		};
	}
}