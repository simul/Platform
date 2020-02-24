cmake_minimum_required (VERSION 3.8)

set(SOURCES d3dxGlobal.cpp
            EffectAPI.cpp
            EffectLoad.cpp
            EffectNonRuntime.cpp
            EffectReflection.cpp
            EffectRuntime.cpp)

add_static_library(effects11 SOURCES ${SOURCES} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Binary" PUBLICINCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/inc" RUNTIME MD FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
add_static_library(effects11 SOURCES ${SOURCES} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Binary" PUBLICINCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/inc" FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})


