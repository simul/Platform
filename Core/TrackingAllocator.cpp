#include "Platform/Core/TrackingAllocator.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/RuntimeError.h"
#include <string.h> // for strlen
#include <fmt/format.h>

using namespace platform;
using namespace core;
using namespace std;

#if SIMUL_INTERNAL_CHECKS
	#define CHECK_ALLOCS 1
#else
	#define CHECK_ALLOCS 0
#endif

static string memstring(size_t bytes)
{
	string memtxt;
	if(bytes<1024)
		memtxt+=fmt::format("{0} bytes",bytes);
	else
	{
		size_t k=bytes/1024;
		if(k<1024)
		{
			memtxt+=fmt::format("{0} kb",k);
		}
		else
		{
			size_t meg=k/1024;
			memtxt+=fmt::format("{0} Mb",meg);
		}
	}
	return memtxt;
}

TrackingAllocator::TrackingAllocator()
	:maxVideoAllocated(0)
	,totalVideoAllocated(0)
	,totalVideoFreed(0)
	,show_output(false)
	,active(true)
{
}

void TrackingAllocator::Shutdown()
{
	if(!active)
		return;
	if(GetCurrentVideoBytesAllocated()!=totalVideoAllocated-totalVideoFreed)
	{
		std::cerr<<"Video memory is tracking issue"<<endl;
	}
	if(GetCurrentVideoBytesAllocated()!=0)
	{
		std::cerr<<"Video memory is not all freed, or tracking problem."<<endl;
		for(auto i:memoryTracks)
		{
			if(i.second!=0)
			{
				cerr<<"\t"<<i.first.c_str()<<": "<<i.second<<endl;
			}
		}
	}
	active=false;
}

TrackingAllocator::~TrackingAllocator()
{
	Shutdown();
}

void* TrackingAllocator::AllocateTracked(size_t nbytes,size_t align,const char *fn)
{
	if(align==0)
		align=1;
	void *ptr=malloc(nbytes);
	memBlocks[ptr]=(int)nbytes;
	debugTextValid=false;
	return ptr;
}
//! De-allocate the memory at \param address (requires that this memory was allocated with Allocate()).
void TrackingAllocator::Deallocate(void* ptr)
{
	if(ptr)
	{
		auto u=memBlocks.find(ptr);
		if(u!=memBlocks.end())
			memBlocks.erase(u);
		free(ptr);
	}
	debugTextValid=false;
}

void TrackingAllocator::TrackVideoMemory(const void* ptr,size_t nbytes,const char *fn)
{
	if(ptr)
	{
#if CHECK_ALLOCS
		if(GetCurrentVideoBytesAllocated()!=totalVideoAllocated-totalVideoFreed)
		{
			SIMUL_CERR<<"Memory tracking issue"<<endl;
		}
#endif
		totalVideoAllocated	+=nbytes;
		size_t currentVideoAllocated=(size_t)totalVideoAllocated-(size_t)totalVideoFreed;
		if(currentVideoAllocated>maxVideoAllocated)
		{
			maxVideoAllocated=currentVideoAllocated;
			if(show_output&&maxVideoAllocated>(1024*1024*12)&&nbytes>1024)
				std::cout<<"Max video memory allocated "<<memstring(maxVideoAllocated).c_str()<<std::endl;
		}
		auto m=memBlocks.find(ptr);
		if(m!=memBlocks.end())
		{
			SIMUL_CERR<<"Trying to allocate memory that's currently allocated: "<<(int64_t)ptr<<", already allocated by "<<allocationNames[ptr].c_str()<<endl;
		}
		memBlocks[ptr]	=nbytes;
		std::string name;
		if(fn&&strlen(fn)>0)
			name=fn;
		else
		{
			name="Unnamed";
		}
		{
			auto i=memoryTracks.find(name);
			if(i==memoryTracks.end())
			{
				memoryTracks[name]=0;
				i=memoryTracks.find(name);
			}
			else
			{
				if(show_output&&i->second>0&&nbytes>(1024*1024*1))
					std::cout<<"Adding "<<memstring(nbytes)<<" to existing memory allocation for "<<name.c_str()<<endl;
			}
			i->second			+=nbytes;
			allocationNames[ptr]=name;
		}
		//std::cout<<"Allocated %s for %s"),*memstring(nbytes),ANSI_TO_TCHAR(name.c_str()));
#if CHECK_ALLOCS
		if(GetCurrentVideoBytesAllocated()!=totalVideoAllocated-totalVideoFreed)
		{
			SIMUL_CERR<<"Memory tracking issue"<<endl;
		}
#endif
	}
	else
	{
		SIMUL_CERR<<"Failed to allocate GPU memory";
	}
	debugTextValid=false;
}

void TrackingAllocator::UntrackVideoMemory(const void* ptr)
{
	// We ignore null, this is like doing delete on a null ptr.
	if(ptr)
	{
		auto m=memBlocks.find(ptr);
		if(m==memBlocks.end())
		{
			std::cerr<<"Trying to deallocate memory that's not been allocated: "<<(int64_t)ptr<<endl;
			return;
		}
		size_t size=m->second;
		totalVideoFreed+=size;
		memBlocks.erase(m);
		auto n=allocationNames.find(ptr);
		if(n!=allocationNames.end())
		{
			const std::string &name=n->second;
			//std::cout<<"Freed %s for %s"),*memstring(size),ANSI_TO_TCHAR(name.c_str()));
			if(name.length())
			{
				auto i=memoryTracks.find(name);
				if(i!=memoryTracks.end())
				{
					i->second-=size;
					if(size>(1024*1024*1))
					{
						if(i->second <= 0)
							allocationNames.erase(n);
						if(show_output)
							std::cout<<"Deallocating "<<memstring(size).c_str()<<" from "<<name.c_str()<<endl;
					}
				}
			}
		}
#if CHECK_ALLOCS
		if(GetCurrentVideoBytesAllocated()!=totalVideoAllocated-totalVideoFreed)
		{
			SIMUL_CERR<<"Memory tracking issue"<<endl;
		}
#endif
	}
	debugTextValid=false;
}
const char *TrackingAllocator::GetNameAtIndex(int index) const
{
	std::map<std::string,size_t>::const_iterator i=memoryTracks.begin();
	for(size_t j=0;j<index&&i!=memoryTracks.end();j++)
	{
		i++;
	}
	if(i==memoryTracks.end())
		return nullptr;
	return i->first.c_str();
}
size_t TrackingAllocator::GetBytesAllocated(const char *name) const
{
	if(!name)
		return 0;
	auto i=memoryTracks.find(name);
	if(i==memoryTracks.end())
		return 0;
	return (size_t)i->second;
}
size_t TrackingAllocator::GetTotalBytesAllocated() const
{
	return 0;
}
size_t TrackingAllocator::GetTotalVideoBytesAllocated() const
{
	return (size_t)totalVideoAllocated;
}
size_t TrackingAllocator::GetTotalVideoBytesFreed() const
{
	return (size_t)totalVideoFreed;
}
size_t TrackingAllocator::GetCurrentVideoBytesAllocated() const
{
	size_t bytes=0;
	for(auto i:memoryTracks)
		bytes+=(size_t)i.second;
	return bytes;
}
const char *TrackingAllocator::GetDebugText() const
{
	if(!debugTextValid)
		UpdateDebugText();
	return debugText.c_str();
}

void TrackingAllocator::UpdateDebugText() const
{
	debugTextValid=true;
	debugText.clear();
	debugText=	fmt::format("  Current {0}\n",memstring(totalVideoAllocated-totalVideoFreed));
	debugText+=	fmt::format("Allocated {0}\n",memstring(totalVideoAllocated));
	debugText+=	fmt::format("    Freed {0}\n",memstring(totalVideoFreed));
}