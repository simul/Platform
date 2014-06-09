#ifndef SIMUL_CROSSPLATFORM_EXPORT_H
#define SIMUL_CROSSPLATFORM_EXPORT_H

#if 1
#if defined(_MSC_VER) && !defined(SIMUL_CROSSPLATFORM_DLL)
	#ifdef _DEBUG
		#ifdef _DLL
			#pragma comment(lib,"SimulCrossPlatform_MDd")
		#else
			#pragma comment(lib,"SimulCrossPlatform_MTd")
		#endif
	#else
		#ifdef _DLL
			#pragma comment(lib,"SimulCrossPlatform_MD")
		#else
			#pragma comment(lib,"SimulCrossPlatform_MT")
		#endif
	#endif
#endif
#endif

#if defined(_DLL) && !defined(DOXYGEN)
// In this lib:
	#if !defined(SIMUL_CROSSPLATFORM_DLL) 
	// If we're building dll libraries but not in this library IMPORT the classes
		#define SIMUL_CROSSPLATFORM_EXPORT __declspec(dllimport)
	#else
	// In ALL OTHER CASES we EXPORT the classes!
		#define SIMUL_CROSSPLATFORM_EXPORT __declspec(dllexport)
	#endif
#else
	#define SIMUL_CROSSPLATFORM_EXPORT
#endif

#ifdef _MSC_VER
	#define SIMUL_CROSSPLATFORM_EXPORT_FN SIMUL_CROSSPLATFORM_EXPORT __cdecl
#else
	#define SIMUL_CROSSPLATFORM_EXPORT_FN SIMUL_CROSSPLATFORM_EXPORT
#endif
#define SIMUL_CROSSPLATFORM_EXPORT_CLASS class SIMUL_CROSSPLATFORM_EXPORT
#define SIMUL_CROSSPLATFORM_EXPORT_STRUCT struct SIMUL_CROSSPLATFORM_EXPORT

#endif
