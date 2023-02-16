# Copyright 2021 Simul Software Ltd, All rights reserved.

cmake_minimum_required( VERSION 3.2 )

if(UNIX)
set(OS_CPP imgui_impl_glfw.cpp)
set(OS_H imgui_impl_glfw.h)
else()
set(OS_CPP imgui_impl_win32.cpp)
set(OS_H imgui_impl_win32.h)
endif()
file(GLOB SOURCES "${SIMUL_PLATFORM_DIR}/External/imgui/*.cpp" imgui_impl_platform.cpp ${OS_CPP})
file(GLOB HEADERS "${SIMUL_PLATFORM_DIR}/External/imgui/*.h" ${OS_H} )

# Setup and build the static library.
add_static_library( ImGui SOURCES ${SOURCES} ${HEADERS} ${SHADERS} DEFINITIONS PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} PUBLICINCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui LINK SimulCrossPlatform_MT)

if(PLATFORM_BUILD_MD_LIBS)
# Setup and build the dynamic library.
add_dynamic_library( ImGui SOURCES ${SOURCES} ${HEADERS} ${SHADERS} DEFINITIONS PLATFORM_IMGUI_DLL=1 PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} "IMGUI_API=__declspec(dllexport)" FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} PUBLICINCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui LINK SimulCrossPlatform_MD)
endif()