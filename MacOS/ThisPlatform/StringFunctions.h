#pragma once

// The macOS counterpart of Linux/ThisPlatform/StringFunctions.h. Apple clang defines
// __APPLE__ rather than __linux__, which the Linux header's guard does not admit.
#if defined(UNIX) || defined(__APPLE__) || defined(__linux__) || defined(__ANDROID__)
	#include <string.h>
	#define _strcpy(d,n,s) (strncpy(d,s,n))
	#define strcpy_s(d, n, s) (strncpy(d,s,n));

	template<typename T, size_t SizeOfArray>
	constexpr size_t _countof(T(&array)[SizeOfArray]) { return SizeOfArray; }
#endif
