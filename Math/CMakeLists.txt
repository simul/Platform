cmake_minimum_required(VERSION 3.14)

file(GLOB SOURCES
    "*.cpp"
)

file(GLOB HEADERS
    "*.h"
)

add_static_library( SimulMath SOURCES ${SOURCES} ${HEADERS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

add_dynamic_library(SimulMath SOURCES ${SOURCES} ${HEADERS} DEFINITIONS SIMUL_MATH_DLL=1 LINK Core_MD FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

