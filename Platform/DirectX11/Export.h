#ifndef SIMUL_DIRECTX11_EXPORT_H
#define SIMUL_DIRECTX11_EXPORT_H
#if defined(SIMUL_DIRECTX11_DLL)  || defined(SIMUL_DIRECTX10_DLL)
	#define SIMUL_DIRECTX1x_DLL
#endif
#if defined(_MSC_VER) && !defined(SIMUL_DIRECTX1x_DLL)
	#ifdef _DEBUG
		#ifdef _DLL
			#pragma comment(lib,"SimulDirectX11_MDd")
		#else
			#pragma comment(lib,"SimulDirectX11_MTd")
		#endif
	#else
		#ifdef _DLL
			#pragma comment(lib,"SimulDirectX11_MD")
		#else
			#pragma comment(lib,"SimulDirectX11_MT")
		#endif
	#endif
#endif

#if defined(SIMUL_DYNAMIC_LINK) && !defined(DOXYGEN)
// In this lib:
	#if !defined(SIMUL_DIRECTX1x_DLL) 
	// If we're building dll libraries but not in this library IMPORT the classes
		#define SIMUL_DIRECTX11_EXPORT __declspec(dllimport)
	#else
	// In ALL OTHER CASES we EXPORT the classes!
		#define SIMUL_DIRECTX11_EXPORT __declspec(dllexport)
	#endif
#else
	#define SIMUL_DIRECTX11_EXPORT
#endif

#ifdef _MSC_VER
	#define SIMUL_DIRECTX11_EXPORT_FN SIMUL_DIRECTX11_EXPORT __cdecl
#else
	#define SIMUL_DIRECTX11_EXPORT_FN SIMUL_DIRECTX11_EXPORT
#endif

#define SIMUL_DIRECTX11_EXPORT_CLASS class SIMUL_DIRECTX11_EXPORT
#define SIMUL_DIRECTX11_EXPORT_STRUCT struct SIMUL_DIRECTX11_EXPORT

#endif
