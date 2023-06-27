# Copyright 2021 Simul Software Ltd, All rights reserved.

cmake_minimum_required(VERSION 3.5)

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

# Setup and build the static library.
add_static_library( ImGui SOURCES ${SOURCES} ${HEADERS} ${SHADERS} DEFINITIONS PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} PUBLICINCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui LINK SimulCrossPlatform_MT)
target_link_directories( ImGui_MT PUBLIC ${SIMUL_PLATFORM_DIR}/External/glfw/build_mt/lib/${CMAKE_BUILD_TYPE})
target_include_directories(ImGui_MT PUBLIC "${SIMUL_PLATFORM_DIR}/External/glfw/include")

if(PLATFORM_BUILD_MD_LIBS)
# Setup and build the dynamic library.
add_dynamic_library( ImGui SOURCES ${SOURCES} ${HEADERS} ${SHADERS} DEFINITIONS PLATFORM_IMGUI_DLL=1 PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} "IMGUI_API=__declspec(dllexport)" FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} PUBLICINCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui LINK SimulCrossPlatform_MD)
target_link_directories( ImGui_MD PUBLIC ${SIMUL_PLATFORM_DIR}/External/glfw/build_md/lib/${CMAKE_BUILD_TYPE})
target_link_libraries (ImGui_MD glfw3)
target_include_directories(ImGui_MD PUBLIC "${SIMUL_PLATFORM_DIR}/External/glfw/include")
endif()