# Copyright 2021 Simul Software Ltd, All rights reserved.

cmake_minimum_required(VERSION 3.12)

if(UNIX)
set(OS_CPP imgui_impl_glfw.cpp)
set(OS_H imgui_impl_glfw.h)
else()
set(OS_CPP imgui_impl_win32.cpp imgui_impl_glfw.cpp)
set(OS_H imgui_impl_win32.h imgui_impl_glfw.h)
endif()

file(GLOB SOURCES 
	"${SIMUL_PLATFORM_DIR}/External/imgui/*.cpp" 
	"${SIMUL_PLATFORM_DIR}/External/imgui/misc/cpp/*.cpp" 
	"imgui_impl_platform.cpp" 
	${OS_CPP} 
	"addons/*/*.cpp"
)
file(GLOB HEADERS 
	"${SIMUL_PLATFORM_DIR}/External/imgui/*.h" 
	"${SIMUL_PLATFORM_DIR}/External/imgui/misc/cpp/*.h" 
	"imgui_impl_platform.h" 
	${OS_H} 
	"addons/*/*.h"
)
 
file(GLOB SHADERS  "Shaders/*.sfx") 
file(GLOB SHADER_INCLUDES "Shaders/*.sl") 
# Setup and build the static library.
add_static_library( ImGui SOURCES ${SOURCES} ${HEADERS} ${SHADERS} ${SHADER_INCLUDES} DEFINITIONS PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} PUBLICINCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui)

message("target is " ${target})
if(SIMUL_SOURCE_BUILD)
	target_include_directories(${target} PUBLIC "${SIMUL_PLATFORM_DIR}/External/glfw/include")
	target_include_directories(${target} PUBLIC "${SIMUL_PLATFORM_DIR}/External")
else()
	target_include_directories(${target} INTERFACE "${SIMUL_PLATFORM_DIR}/External/glfw/include")
	target_include_directories(${target} INTERFACE "${SIMUL_PLATFORM_DIR}/External")
endif()

if (SIMUL_DLL_SDK_LIBRARY)
	AddDLLCompileDefinitions( ${target} )
	target_compile_definitions(${target} PRIVATE "IMGUI_API=__declspec(dllexport)")
endif()

add_multiplatform_sfx_shader_project( ${target} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
								"${SIMUL_PLATFORM_DIR}/.."
							SOURCES ${SHADERS} ${SHADER_INCLUDES} OPTIONS DEFINES VULKAN_SDK=${VULKAN_SDK_DIR}
					 		OUTPUT "${CMAKE_BINARY_DIR}/shaderbin/$PLATFORM_NAME" )
