#ifndef SIMUL_MATH_EXPORT_H
#define SIMUL_MATH_EXPORT_H

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

#if defined(_MSC_VER) && !defined(SIMUL_MATH_DLL)
	#ifdef _DEBUG
		#ifdef _DLL
			#pragma comment(lib,"SimulMath_MDd")
		#else
			#pragma comment(lib,"SimulMath_MTd")
		#endif
	#else
		#ifdef _DLL
			#pragma comment(lib,"SimulMath_MD")
		#else
			#pragma comment(lib,"SimulMath_MT")
		#endif
	#endif
#endif

#if defined(SIMUL_DYNAMIC_LINK) && !defined(DOXYGEN)
// In this lib:
	#if !defined(SIMUL_MATH_DLL) 
	// If we're building dll libraries but not in this library IMPORT the classes
        #define SIMUL_MATH_EXPORT SIMUL_IMPORT
	#else
	// In ALL OTHER CASES we EXPORT the classes!
        #define SIMUL_MATH_EXPORT SIMUL_EXPORT
	#endif
#else
	#define SIMUL_MATH_EXPORT
#endif


#if defined(_MSC_VER) && !defined(DOXYGEN)
	#define SIMUL_MATH_EXPORT_FN		SIMUL_MATH_EXPORT __cdecl
#else
	#define SIMUL_MATH_EXPORT_FN		SIMUL_MATH_EXPORT
#endif
#define SIMUL_MATH_EXPORT_CLASS		class SIMUL_MATH_EXPORT
#define SIMUL_MATH_EXPORT_STRUCT	struct SIMUL_MATH_EXPORT

#endif
