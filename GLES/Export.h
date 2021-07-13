#pragma once

#include "Platform/Core/DebugMemory.h"

#if defined(_MSC_VER)
    //  Microsoft
    #define SIMUL_EXPORT __declspec(dllexport)
    #define SIMUL_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
    //  GCC or Clang
    #define SIMUL_EXPORT __attribute__((visibility("default")))
    #define SIMUL_IMPORT
#else
    //  do nothing and hope for the best?
    #define SIMUL_EXPORT
    #define SIMUL_IMPORT
    #pragma warning Unknown dynamic link import/export semantics.
#endif

#if defined(SIMUL_DYNAMIC_LINK) && !defined(DOXYGEN)
    // In this lib:
	#if !defined(SIMUL_GLES_DLL) 
	    // If we're building dll libraries but not in this library IMPORT the classes
		#define SIMUL_GLES_EXPORT SIMUL_IMPORT
	#else
	    // In ALL OTHER CASES we EXPORT the classes!
		#define SIMUL_GLES_EXPORT SIMUL_EXPORT
	#endif
#else
	#define SIMUL_GLES_EXPORT
#endif

#ifdef _MSC_VER
	#define SIMUL_GLES_EXPORT_FN SIMUL_GLES_EXPORT __cdecl
#else
	#define SIMUL_GLES_EXPORT_FN SIMUL_GLES_EXPORT
#endif

#define SIMUL_GLES_EXPORT_CLASS class SIMUL_GLES_EXPORT
#define SIMUL_GLES_EXPORT_STRUCT struct SIMUL_GLES_EXPORT
