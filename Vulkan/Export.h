#ifndef SIMUL_VULKAN_EXPORT_H
#define SIMUL_VULKAN_EXPORT_H

#include "Platform/Core/DebugMemory.h"

#define SIMUL_VULKAN_FRAME_LAG 2

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

#if defined(_MSC_VER) && !defined(SIMUL_VULKAN_DLL)
	#ifdef _DEBUG
		#ifdef _DLL
			#pragma comment(lib,"SimulVulkan_MDd")
		#else
			#pragma comment(lib,"SimulVulkan_MTd")
		#endif
	#else
		#ifdef _DLL
			#pragma comment(lib,"SimulVulkan_MD")
		#else
			#pragma comment(lib,"SimulVulkan_MT")
		#endif
	#endif
#endif

#if defined(SIMUL_DYNAMIC_LINK) && !defined(DOXYGEN)
    // In this lib:
	#if !defined(SIMUL_VULKAN_DLL) 
	    // If we're building dll libraries but not in this library IMPORT the classes
		#define SIMUL_VULKAN_EXPORT SIMUL_IMPORT
	#else
	    // In ALL OTHER CASES we EXPORT the classes!
		#define SIMUL_VULKAN_EXPORT SIMUL_EXPORT
	#endif
#else
	#define SIMUL_VULKAN_EXPORT
#endif

#ifdef _MSC_VER
	#define SIMUL_VULKAN_EXPORT_FN SIMUL_VULKAN_EXPORT __cdecl
#else
	#define SIMUL_VULKAN_EXPORT_FN SIMUL_VULKAN_EXPORT
#endif

#define SIMUL_VULKAN_EXPORT_CLASS class SIMUL_VULKAN_EXPORT
#define SIMUL_VULKAN_EXPORT_STRUCT struct SIMUL_VULKAN_EXPORT

#define SIMUL_VK_CHECK(result) {vk::Result res=result;SIMUL_ASSERT((res) == vk::Result::eSuccess);}

#define RETURN_FALSE_IF_FAILED(v) {if((v)!=vk::Result::eSuccess) {SIMUL_BREAK("failed in Vulkan."); return false;}}

#define vulkanRenderPlatform ((platform::vulkan::RenderPlatform*)renderPlatform)
#endif
