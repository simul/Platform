#pragma once
typedef unsigned int THREAD_TYPE;

#include <thread>
#include <windows.h>
#include <processthreadsapi.h>
#include "Platform/Core/RuntimeError.h"

const DWORD MS_VC_SETTHREADNAME_EXCEPTION = 0x406D1388;  

#pragma pack(push,8)  
struct THREADNAME_INFO  
{  
    DWORD dwType=0x1000;	// Must be 0x1000.  
    LPCSTR szName=0;		// Pointer to name (in user addr space).  
    DWORD dwThreadID=0;		// Thread ID (-1=caller thread).  
    DWORD dwFlags=0;		// Reserved for future use, must be zero.  
 } ;  
#pragma pack(pop)

inline void SetThreadName(std::thread &thread,const char *name)
{  
	DWORD ThreadId = ::GetThreadId( static_cast<HANDLE>( thread.native_handle() ) );
    THREADNAME_INFO info;
    info.szName = name;  
    info.dwThreadID = ThreadId;
#pragma warning(push)  
#pragma warning(disable: 6320 6322)  
    __try
	{  
        RaiseException(MS_VC_SETTHREADNAME_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);  
    }  
    __except (EXCEPTION_EXECUTE_HANDLER)
	{  
    }  
#pragma warning(pop)  
}


inline void SetThreadPriority(std::thread &thread,int p)
{
	DWORD nPriority=0;
	// Roderick: Upping the priority to 99 really seems to help avoid dropped packets.
	switch(p)
	{
		case -2:
		nPriority = THREAD_PRIORITY_LOWEST;
		break;
		case -1:
		nPriority = THREAD_PRIORITY_BELOW_NORMAL;
		break;
		case 0:
		nPriority = THREAD_PRIORITY_NORMAL;
		break;
		case 1:
		nPriority = THREAD_PRIORITY_ABOVE_NORMAL;
		break;
		case 2:
		nPriority = THREAD_PRIORITY_HIGHEST;
		break;
		default:
		break;
	}
	HANDLE ThreadHandle = static_cast<HANDLE>( thread.native_handle());
	BOOL result=SetThreadPriority(ThreadHandle,nPriority);
	if(!result)
	{
		SIMUL_BREAK_ONCE_INTERNAL("SetThreadPriority failed\n");
	}
}