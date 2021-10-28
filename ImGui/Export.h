#ifndef PLATFORM_IMGUI_EXPORT_H
#define PLATFORM_IMGUI_EXPORT_H

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

#if defined(_MSC_VER) && !defined(PLATFORM_IMGUI_DLL)
	#ifdef _DEBUG
		#ifdef _DLL
			#pragma comment(lib,"ImGui_MDd")
		#else
			#pragma comment(lib,"ImGui_MTd")
		#endif
	#else
		#ifdef _DLL
			#pragma comment(lib,"ImGui_MD")
		#else
			#pragma comment(lib,"ImGui_MT")
		#endif
	#endif
#endif

#if defined(SIMUL_DYNAMIC_LINK) && !defined(DOXYGEN)
    // In this lib:
    #if !defined(PLATFORM_IMGUI_DLL)
    // If we're building dll libraries but not in this library IMPORT the classes
        #define PLATFORM_IMGUI_EXPORT SIMUL_IMPORT
    #else
    // In ALL OTHER CASES we EXPORT the classes!
        #define PLATFORM_IMGUI_EXPORT SIMUL_EXPORT
    #endif
#else
	#define PLATFORM_IMGUI_EXPORT
#endif

#ifdef _MSC_VER
	#define PLATFORM_IMGUI_EXPORT_FN PLATFORM_IMGUI_EXPORT __cdecl
#else
	#define PLATFORM_IMGUI_EXPORT_FN PLATFORM_IMGUI_EXPORT
#endif
#define PLATFORM_IMGUI_EXPORT_CLASS class PLATFORM_IMGUI_EXPORT
#define PLATFORM_IMGUI_EXPORT_STRUCT struct PLATFORM_IMGUI_EXPORT

#endif
