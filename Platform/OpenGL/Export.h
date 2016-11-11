#ifndef SIMUL_OPENGL_EXPORT_H
#define SIMUL_OPENGL_EXPORT_H

#if defined(_MSC_VER) && !defined(SIMUL_OPENGL_DLL)
	#ifdef _DEBUG
		#ifdef _DLL
			#pragma comment(lib,"SimulOpenGL_MDd")
		#else
			#pragma comment(lib,"SimulOpenGL_MTd")
		#endif
	#else
		#ifdef _DLL
			#pragma comment(lib,"SimulOpenGL_MD")
		#else
			#pragma comment(lib,"SimulOpenGL_MT")
		#endif
	#endif
#endif

#if defined(SIMUL_DYNAMIC_LINK) && !defined(DOXYGEN)
// In this lib:
	#if !defined(SIMUL_OPENGL_DLL) 
	// If we're building dll libraries but not in this library IMPORT the classes
		#define SIMUL_OPENGL_EXPORT __declspec(dllimport)
	#else
	// In ALL OTHER CASES we EXPORT the classes!
		#define SIMUL_OPENGL_EXPORT __declspec(dllexport)
	#endif
#else
	#define SIMUL_OPENGL_EXPORT
#endif

#ifdef _MSC_VER
	#define SIMUL_OPENGL_EXPORT_FN SIMUL_OPENGL_EXPORT __cdecl
#else
	#define SIMUL_OPENGL_EXPORT_FN SIMUL_OPENGL_EXPORT
#endif

#define SIMUL_OPENGL_EXPORT_CLASS class SIMUL_OPENGL_EXPORT
#define SIMUL_OPENGL_EXPORT_STRUCT struct SIMUL_OPENGL_EXPORT

#endif
