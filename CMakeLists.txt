cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)
if(SIMUL_PLATFORM_DIR)
	set(SIMUL_PLATFORM_FOLDER_PREFIX Platform)
else()
	project(Platform)
	set(SIMUL_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

set(PLATFORM_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PLATFORM_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(SIMUL_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})

# copy to scope above:
if (NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(SIMUL_PLATFORM_DIR ${SIMUL_PLATFORM_DIR} PARENT_SCOPE)
	set(PLATFORM_SOURCE_DIR ${PLATFORM_SOURCE_DIR} PARENT_SCOPE)
	set(PLATFORM_BUILD_DIR ${PLATFORM_BUILD_DIR} PARENT_SCOPE)
endif()

if(PLATFORM_USE_ASSIMP)
	set(ASSIMP_LIBNAME assimp-vc${MSVC_TOOLSET_VERSION}-mt$<$<CONFIG:Debug>:d> )
	if(NOT PLATFORM_WINDOWS)
		add_subdirectory(External/assimp)
	endif()
endif()

include("CMake/Variables.cmake")
include("CMake/Include.cmake")
include("CMake/Shader.cmake")

if(PLATFORM_USE_FMT)
	set(FMT_DOC false)
	set(FMT_INSTALL false)
	set(FMT_TEST false)
	add_subdirectory(External/fmt)
	add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
	if( PLATFORM_WINDOWS )
		if(${PLATFORM_WINDOWS_RUNTIME} MATCHES "static")
			set_property(TARGET fmt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		endif()
	endif()
	set_target_properties( fmt PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/External)
	set_target_properties( fmt PROPERTIES PREFIX "" )
	if (GDK)
		set_target_runtime(fmt dynamic)
		set_target_properties( fmt PROPERTIES SUFFIX ".lib" )
		set_property(TARGET fmt PROPERTY PROJECT_LABEL fmt_GDK )
		set_target_properties(fmt PROPERTIES VS_GLOBAL_XdkEditionTarget ${GDK_TOOLCHAIN_VERSION})
	endif()
endif()


# FetchContent download timestamp policy:
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0077 NEW)
include(FetchContent)

# Get the "safe" header library for lockable resources
set(BUILD_TESTING OFF)
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
FetchContent_Declare(
  safe
  GIT_REPOSITORY https://github.com/LouisCharlesC/safe.git
  GIT_TAG        v1.1.0
  SOURCE_DIR safe
)
FetchContent_MakeAvailable(safe)

# fetch magic_enum
FetchContent_Declare(
  magic_enum
  URL https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.5.zip
  SOURCE_DIR magic_enum
)
FetchContent_MakeAvailable(magic_enum)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake" ${CMAKE_MODULE_PATH})

# GLFW
#if(PLATFORM_BUILD_SAMPLES)
	option(GLFW_INSTALL  "" OFF)
	add_subdirectory(External/glfw)
	set_target_properties( glfw PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/External)
	set_target_properties(update_mappings PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/External)
	if( PLATFORM_WINDOWS )
		set_property(TARGET glfw PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
#endif()

function(AddDLLCompileDefinitions TargetName)
	# Export functions for dynamic linking
	target_compile_definitions(${TargetName} PRIVATE SIMUL_DYNAMIC_LINK=1)
	# Platform
	target_compile_definitions(${TargetName} PRIVATE PLATFORM_CORE_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE PLATFORM_IMGUI_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_CROSSPLATFORM_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_DIRECTX11_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_DIRECTX12_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_MATH_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_OPENGL_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_VULKAN_DLL=1)
	# trueSKY
	target_compile_definitions(${TargetName} PRIVATE SIMUL_BASE_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_CLOUDS_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_SKY_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE SIMUL_TERRAIN_DLL=1)
	target_compile_definitions(${TargetName} PRIVATE UI_DLL=1)
endfunction()

# Core subdirectories
add_subdirectory(Core)
add_subdirectory(Math)
add_subdirectory(CrossPlatform)
add_subdirectory(Applications/Sfx)

# Platform and API subdirectories
if(PLATFORM_SUPPORT_VULKAN)
	if(PLATFORM_WINDOWS OR PLATFORM_LINUX)
		add_subdirectory(Vulkan)
	endif()
endif()
 
if(PLATFORM_WINDOWS)
	if( PLATFORM_SUPPORT_OPENGL )
		add_subdirectory(OpenGL)
	endif()
	if( PLATFORM_SUPPORT_D3D11 )
		add_subdirectory(DirectX11)
	endif()
	if( PLATFORM_SUPPORT_D3D12 )
		add_subdirectory(DirectX12)
	endif()
endif()

if(PLATFORM_SUPPORT_GLES)
	add_subdirectory(GLES)
endif()

if(PLATFORM_BUILD_SAMPLES AND ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten" )
	add_subdirectory(Applications/EmscriptenSample)
endif()

if(GDK)
	add_subdirectory(DirectX12)
	add_subdirectory(External/DirectX/DirectXTex/DirectXTex)
endif()

if(PLATFORM_WINGDK)
	add_subdirectory(DirectX11)
endif()

if(PLATFORM_WINDOWS)
	add_subdirectory(External/DirectX/DirectXTex/DirectXTex)
endif()

if(PLATFORM_BUILD_DOCS)
	add_subdirectory(Docs)
endif()

if(PLATFORM_PS4)
	add_subdirectory(PS4/Render)
endif()

if(PLATFORM_COMMODORE)
	add_subdirectory(Commodore)
endif()

if(PLATFORM_IMGUI)
	add_subdirectory(ImGui)
	if(PLATFORM_BUILD_SAMPLES)
		add_subdirectory(Applications/GuiSample)
	endif()
endif()

# Samples
if(PLATFORM_WINDOWS)
	if(PLATFORM_BUILD_SAMPLES)
		add_subdirectory(Applications/Sample)
		add_subdirectory(Applications/Test)
	endif()
endif()