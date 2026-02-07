#pragma once
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <cerrno>
#define THREAD_TYPE pthread_t
#include <thread>
#include <sys/prctl.h>

inline void SetThreadName(std::thread &thread,const char *name)
{
	pthread_setname_np(thread.native_handle(), name);
}
inline void SetThisThreadName(const char *name)
{
	prctl(PR_SET_NAME, (long)name, 0, 0, 0);
}

inline int MapPriorityToSchedPriority(int p)
{
	switch(p)
	{
		case -2:
			return 1;
		case -1:
			return 25;
		case 0:
			return 50;
		case 1:
			return 75;
		case 2:
			return 99;
		default:
			return 50;
	}
}

inline void SetThreadPriority(std::thread &thread,int p)
{
	sched_param sch_params;
	sch_params.sched_priority = MapPriorityToSchedPriority(p);
	pthread_setschedparam(thread.native_handle(), SCHED_RR, &sch_params);
}

inline void SetThisThreadPriority(int p)
{
	sched_param sch_params;
	sch_params.sched_priority = MapPriorityToSchedPriority(p);
	pthread_setschedparam(pthread_self(), SCHED_RR, &sch_params);
}

inline int fopen_s(FILE** pFile, const char *filename, const char *mode)
{
	*pFile = fopen(filename, mode);
	return errno;
}

inline int fdopen_s(FILE** pFile, int fildes, const char *mode)
{
	*pFile = fdopen(fildes, mode);
	return errno;
}