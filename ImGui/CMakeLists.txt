# Copyright 2021 Simul Software Ltd, All rights reserved.

cmake_minimum_required( VERSION 3.2 )

file(GLOB SOURCES "${SIMUL_PLATFORM_DIR}/External/imgui/*.cpp" *.cpp)
file(GLOB HEADERS "${SIMUL_PLATFORM_DIR}/External/imgui/*.h" "*.h" )

# Setup and build the static library.
add_static_library( ImGui SOURCES ${SOURCES} ${HEADERS} ${SHADERS} DEFINITIONS PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} PUBLICINCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui LINK SimulCrossPlatform_MT)

if(PLATFORM_BUILD_MD_LIBS)
# Setup and build the dynamic library.
add_dynamic_library( ImGui SOURCES ${SOURCES} ${HEADERS} ${SHADERS} DEFINITIONS PLATFORM_IMGUI_DLL=1 PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} "IMGUI_API=__declspec(dllexport)" FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} PUBLICINCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui LINK SimulCrossPlatform_MD)
endif()