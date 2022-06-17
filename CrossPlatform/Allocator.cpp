#include "Platform/CrossPlatform/Allocator.h"
#include "Platform/Core/RuntimeError.h"

using namespace simul;
using namespace base;
using namespace platform;
using namespace core;
using namespace crossplatform;
using namespace std;

Allocator::Allocator()
{
}

Allocator::~Allocator()
{
	Shutdown();
}
//! Set max age for blocks to be freed.
void Allocator::SetMaxAge(size_t m)
{
	max_age=m;
}

//! Free all memory still being held.
void Allocator::Shutdown()
{
	mutex.lock();
	for ( auto i = to_free_cpu.begin(); i != to_free_cpu.end(); i++)
	{
		memoryInterface->Deallocate(i->first);
	}
	to_free_cpu.clear();
	for ( auto i = to_free_video.begin(); i != to_free_video.end();i++ )
	{
		memoryInterface->DeallocateVideoMemory(i->first);
	}
	to_free_video.clear();
	mutex.unlock();
}

//! Age all memory blocks by 1. Free any blocks that are old enough.
void Allocator::CheckForReleases()
{
	mutex.lock();
	for (auto i = to_free_cpu.begin(); i != to_free_cpu.end();i++ )
	{
		i->second++;
		if(i->second>=max_age)
		{
			memoryInterface->Deallocate(i->first);
			to_free_cpu.erase(i);
			break;
		}
	}
	for (auto i = to_free_video.begin(); i != to_free_video.end();i++ )
	{
		i->second++;
		if (i->second >= max_age)
		{
			memoryInterface->DeallocateVideoMemory(i->first);
			to_free_video.erase(i);
			break;
		}
	}
	mutex.unlock();
}

void Allocator::SetExternalAllocator(simul::base::MemoryInterface* m)
{
	if(m==memoryInterface)
		return;
	if(to_free_cpu.size()||to_free_video.size())
	{
		SIMUL_BREAK_INTERNAL("Changing allocator when memory is waiting to be freed.")
		Shutdown();
	}
	memoryInterface=m;
}

//! Allocate \a nbytes bytes of memory, aligned to \a align and return a pointer to them.
void* Allocator::AllocateTracked(size_t nbytes,size_t align,const char *fn)
{
	return memoryInterface->AllocateTracked(nbytes,align,fn);
}
//! De-allocate the memory at \param address (requires that this memory was allocated with Allocate()).
void Allocator::Deallocate(void* address)
{
	to_free_cpu[address] = 0;
}
//! Allocate \a nbytes bytes of memory, aligned to \a align and return a pointer to them.
void* Allocator::AllocateVideoMemoryTracked(size_t nbytes,size_t align,const char *fn)
{
	return memoryInterface->AllocateVideoMemoryTracked(nbytes, align,  fn);
}
//! De-allocate the memory at \param address (requires that this memory was allocated with Allocate()).
void Allocator::DeallocateVideoMemory(void* address)
{
	to_free_video[address]=0;
}

const char *Allocator::GetNameAtIndex(int index) const
{
	return memoryInterface->GetNameAtIndex(index);
}
size_t Allocator::GetBytesAllocated(const char *name) const
{
	return memoryInterface->GetBytesAllocated(name);
}
size_t Allocator::GetTotalBytesAllocated() const
{
	return memoryInterface->GetTotalBytesAllocated();
}
size_t Allocator::GetCurrentVideoBytesAllocated() const
{
	return memoryInterface->GetCurrentVideoBytesAllocated();
}
size_t Allocator::GetTotalVideoBytesAllocated() const
{
	return memoryInterface->GetTotalVideoBytesAllocated();

}
size_t Allocator::GetTotalVideoBytesFreed() const
{
	return memoryInterface->GetTotalVideoBytesFreed();
}