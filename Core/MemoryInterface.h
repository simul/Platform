#ifndef SIMUL_META_MEMORYINTERFACE_H
#define SIMUL_META_MEMORYINTERFACE_H
#include <stdio.h>
#include <cstdlib>
#include <memory>
#include <limits>
#include "Platform/Core/Export.h"
#define SIMUL_TRACK_ALLOCS
#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#undef DH_MEMORY
// DH_MEMORY is best disabled because it overrides the default new and delete, and because having two separate styles of allocation
// mixed together is likely to cause bugs. Use with caution.

#ifdef DH_MEMORY
	//Define new and delete operators for a class in a lib.
	//new/delete operators are implicitly static and only need to be defined in the base class of your 
	//inheritance hierarchy.
	#define DEFINE_NEW_DELETE_OVERRIDES \
		inline void* operator new(std::size_t size)                       { return platform::core::memory::New(size, 0); } \
		inline void* operator new(std::size_t size, std::size_t align)    { return platform::core::memory::New(size, align); } \
		inline void  operator delete(void* p)                             { platform::core::memory::Delete(p); } \
		inline void  operator delete(void* p, std::size_t /*align*/)      { platform::core::memory::Delete(p); } \
		inline void* operator new[] (std::size_t size)                    { return platform::core::memory::NewArray(size, 0); } \
		inline void* operator new[] (std::size_t size, std::size_t align) { return platform::core::memory::NewArray(size, align); } \
		inline void  operator delete[] (void* p)                          { platform::core::memory::DeleteArray(p); } \
		inline void  operator delete[] (void* p, std::size_t /*align*/)   { platform::core::memory::DeleteArray(p); } \
		inline void* operator new(std::size_t, void* /*p*/)               { return 0; /*Unimplemented placement new*/ } \
		inline void  operator delete(void*, void*)                        { } \
		class MacroMustEndWithSemiColon
#else
	#define DEFINE_NEW_DELETE_OVERRIDES class MacroMustEndWithSemiColon
#endif

namespace platform
{
	namespace core
	{
		//! A virtual interface class for classes that can allocate and de-allocate memory.

		//! Inherit from MemoryInterface to take control of memory allocation for any class that uses
		//! this interface as a resource.
		class PLATFORM_CORE_EXPORT MemoryInterface
		{
		public:
			//! Allocate nbytes bytes of memory and return a pointer to them.
			//! Reimplement this in the derived class.
			void* Allocate(size_t nbytes)
			{
				return AllocateTracked(nbytes,1,NULL);
			}
			//! Allocate nbytes bytes of memory, aligned to align and return a pointer to them. for unaligned, use align=0.
			//! Reimplement this in the derived class.
			void* Allocate(size_t nbytes,size_t align)
			{
				return AllocateTracked(nbytes,align,NULL);
			}
			//! Allocate nbytes bytes of memory, aligned to align and return a pointer to them. for unaligned, use align=0.
			//! Reimplement this in the derived class.
			void* AllocateVideoMemory(size_t nbytes,size_t align)
			{
				return AllocateVideoMemoryTracked(nbytes,align,NULL);
			}
			//! Reimplement this if needed in the derived class. The function_name can be ignored if you are not tracking memory allocations.
			virtual void* AllocateTracked(size_t /*nbytes*/,size_t /*align*/,const char * /*fn_name*/){return NULL;}
			//! Reimplement this if needed in the derived class. The function_name can be ignored if you are not tracking memory allocations.
			virtual void* AllocateVideoMemoryTracked(size_t /*nbytes*/,size_t /*align*/,const char * /*fn_name*/){return NULL;}

			//! De-allocate the memory at \param address (requires that this memory was allocated with Allocate()).
			//! Implementations MUST ignore NULL values.
			virtual void Deallocate(void* address)=0;
			//! \brief Free some Video memory
			//! Implementations MUST ignore NULL values.
			virtual void DeallocateVideoMemory( void*  ){}
			virtual const char *GetNameAtIndex(int ) const{return "";}
			virtual size_t GetBytesAllocated(const char *) const {return 0;}
			virtual size_t GetTotalBytesAllocated() const {return 0;}
			virtual size_t GetCurrentVideoBytesAllocated() const {return 0;}
			virtual size_t GetTotalVideoBytesAllocated() const {return 0;}
			virtual size_t GetTotalVideoBytesFreed() const {return 0;}
			virtual void TrackVideoMemory(const void* , size_t,const char *){}
			virtual void UntrackVideoMemory(const void* ){}
		};
		extern PLATFORM_CORE_EXPORT MemoryInterface *GetDefaultMemoryInterface();
	}
}

#ifdef SIMUL_TRACK_ALLOCS
#define Allocate(n,a) AllocateTracked(n,a,__PRETTY_FUNCTION__)
#define AllocateVideoMemory(n,a) AllocateVideoMemoryTracked(n,a,__PRETTY_FUNCTION__)
#endif

inline void* operator new(size_t nbytes,platform::core::MemoryInterface *mi)
{
	if(!mi)
		return malloc(nbytes);
	return mi->Allocate(nbytes,0);
}

inline void* operator new[](size_t nbytes,platform::core::MemoryInterface *mi)
{
	if(!mi)
		return malloc(nbytes);
	return mi->Allocate(nbytes,0);
}

inline void operator delete[](void *p,platform::core::MemoryInterface *mi)
{
	if(!mi)
		free(p);
	else
		mi->Deallocate(p);
}

inline void operator delete(void *p,platform::core::MemoryInterface *mi)
{
	if(!mi)
		free(p);
	else
		mi->Deallocate(p);
}

template<class T> void del(T* p, platform::core::MemoryInterface *mi)
{
	if (p)
	{
		p->~T();		// explicit destructor call
		if(mi)
		{
			mi->Deallocate(p);
		}
		else
            free(p);
	}
}


namespace platform
{
	namespace core
	{
		//! An allocator to use MemoryInterface in STL classes.
		template<typename T> class MemoryInterfaceAllocator: public std::allocator<T>
		{
		public :
			MemoryInterface *memoryInterface;
			//    typedefs
			typedef T value_type;
			typedef value_type* pointer;
			typedef const value_type* const_pointer;
			typedef value_type& reference;
			typedef const value_type& const_reference;
			typedef std::size_t size_type;
			typedef std::ptrdiff_t difference_type;

		public :
			//    convert an allocator<T> to allocator<U>
			template<typename U> struct rebind 
			{
				typedef MemoryInterfaceAllocator<U> other;
			};

		public :
			inline explicit MemoryInterfaceAllocator(MemoryInterface *m)
				:memoryInterface(m)
			{
				if(!memoryInterface)
					memoryInterface=GetDefaultMemoryInterface();
			}
			inline ~MemoryInterfaceAllocator()
			{
			}
			inline explicit MemoryInterfaceAllocator( std::allocator<T> a)
				:memoryInterface(a.memoryInterface)
			{
				if(!memoryInterface)
					memoryInterface=GetDefaultMemoryInterface();
			}
			inline explicit MemoryInterfaceAllocator( std::allocator<T> const & a)
				:memoryInterface(a.memoryInterface)
			{
				if(!memoryInterface)
					memoryInterface=GetDefaultMemoryInterface();
			}
			template<typename U> inline explicit MemoryInterfaceAllocator( MemoryInterfaceAllocator<U> const & a) 
				:memoryInterface(a.memoryInterface)
			{
				if(!memoryInterface)
					memoryInterface=GetDefaultMemoryInterface();
			}

			//    address

			inline pointer address(reference r)
			{
				return &r;
			}
			inline const_pointer address(const_reference r)
			{
				return &r;
			}

			//    memory allocation
#if 0 // This causes warnings in C++17. Not clear why.
			inline pointer allocate(size_type cnt,typename std::allocator<T>::const_pointer = 0)
			{
				pointer new_memory = reinterpret_cast<pointer>(memoryInterface->Allocate(cnt * sizeof (T),1));
				return new_memory;
			}
#endif
			inline void deallocate(pointer p, size_type )
			{
				memoryInterface->Deallocate(p);
			}
			// Strictly speaking we should do this:
#ifndef _MSC_VER
			//    size
			// Note, Microsoft's #defines min and max get in the way here so use #define NOMINMAX before the windows includes.
#ifdef max
#error("Please #define NOMINMAX at the top of the .cpp file so that max() does not cause a compiler error on Windows platforms.")
#endif
			inline size_type max_size() const
			{
				return std::numeric_limits<size_type>::max() / sizeof(T);
			}
#else
			// but because it causes so much trouble with Windows platforms, we just do this:
			inline size_type max_size() const
			{
				return UINT_MAX/sizeof(T);
			}
#endif
			//    construction/destruction

			inline void construct(pointer p, const T& t)
			{
				new(p) T(t);
			}
			inline void destroy(pointer p)
			{
#ifdef _MSC_VER
				p;
#endif
				p->~T();
			}
			inline bool operator==(MemoryInterfaceAllocator const&)
			{
				return true;
			}
			inline bool operator!=(MemoryInterfaceAllocator const& a) 
			{
				return !operator==(a);
			}
		}; 
	}
}
#ifdef SIMUL_TRACK_ALLOCS
#undef Allocate
#undef AllocateVideoMemory
#endif
#endif
