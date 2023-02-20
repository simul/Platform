#pragma once
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <cerrno>
#define THREAD_TYPE pthread_t

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