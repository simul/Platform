#pragma once

#if defined(UNIX) || defined(__linux__) || defined(__ANDROID__)
	#include <string.h>
	#define _strcpy(d,n,s) (strncpy(d,s,n))
	#define strcpy_s(d, n, s) (strncpy(d,s,n));

	template<typename T, size_t SizeOfArray>
	constexpr size_t _countof(T(&array)[SizeOfArray]) { return SizeOfArray; }
#endif