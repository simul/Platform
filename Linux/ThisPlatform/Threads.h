#pragma once
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <cerrno>
#define THREAD_TYPE pthread_t

inline int strcpy_s(char* dst, size_t size, const char* src)
{
	assert(size >= (strlen(src) + 1));
	strncpy(dst, src, size - 1);
	dst[size - 1] = '\0';
	return errno;
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