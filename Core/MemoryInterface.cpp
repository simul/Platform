#include "MemoryInterface.h"
#ifdef _MSC_VER
#include <malloc.h>
#elif __APPLE__
#include <stdlib.h>
#endif
#include <map>
namespace platform
{
	namespace core
	{
		/// Default implementation of MemoryInterface, using standard allocators.
		class DefaultMemoryInterface:public MemoryInterface
		{
			std::map<const char *,size_t> memoryTracks;
			std::map<void *,const char *> allocationNames;
		public:
			DefaultMemoryInterface()
			{
			}
			void* AllocateTracked(size_t nbytes,size_t align,const char *) override
			{
				if(align==0)
				{
					align=1;
				}
		#ifdef _MSC_VER
				return _aligned_malloc(nbytes,align);
		#else
            #ifndef UNIX
				return memalign(align,nbytes);
			#else
				if(align==1)
				{
					return malloc(nbytes);
				}
				else
				{
					void* pBuffer = nullptr;
					posix_memalign(&pBuffer,align,nbytes);
					return pBuffer;
				}
			#endif	// __APPLE__.
		#endif
			}
			void Deallocate(void* address) override
			{
				if(address)
		#ifdef _MSC_VER
					_aligned_free(address);
		#else
					free(address);
		#endif
			}
			void* AllocateVideoMemoryTracked(size_t nbytes,size_t align,const char *f) override
			{
				return AllocateTracked(nbytes,align,f);
			}
			void DeallocateVideoMemory(void* address) override
			{
				if(address)
					return Deallocate(address);
			}
			
			const char *GetNameAtIndex(int index) const override
			{
				std::map<void *,const char *>::const_iterator i=allocationNames.begin();
				for(int j=0;j<index&&i!=allocationNames.end();j++)
				{
					i++;
				}
				if(i==allocationNames.end())
					return nullptr;
				return i->second;
			}
			size_t GetBytesAllocated(const char *name) const override
			{
				if(!name)
					return 0;
				auto i=memoryTracks.find(name);
				if(i==memoryTracks.end())
					return 0;
				return i->second;
			}
			size_t GetTotalBytesAllocated() const override
			{
				size_t bytes=0;
				for(auto i:memoryTracks)
				{
					bytes+=i.second;
				}
				return bytes;
			}
			virtual size_t GetCurrentVideoBytesAllocated() const override
			{
				return 0;
			}
			virtual size_t GetTotalVideoBytesAllocated() const override
			{
				return 0;
			}
			virtual size_t GetTotalVideoBytesFreed() const override
			{
				return 0;
			}
		};

		static DefaultMemoryInterface _defaultMemoryInterface;

		MemoryInterface *GetDefaultMemoryInterface()
		{
			return &_defaultMemoryInterface;
		}


		namespace memory
		{
			static MemoryInterface* s_pMemInterface = &_defaultMemoryInterface;

			void SetMemInterface(platform::core::MemoryInterface& mem)
			{
				s_pMemInterface = &mem;
			}

			platform::core::MemoryInterface* GetMemInterface()
			{
				return s_pMemInterface;
			}
		}
	}
}
