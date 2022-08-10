cmake_minimum_required(VERSION 2.9)

file(GLOB SOURCES
    "*.cpp"
)

file(GLOB HEADERS
    "*.h"
)

add_static_library( SimulMath SOURCES ${SOURCES} ${HEADERS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

if(PLATFORM_BUILD_MD_LIBS)
	add_dynamic_library(SimulMath SOURCES ${SOURCES} ${HEADERS} DEFINITIONS SIMUL_MATH_DLL=1 LINK Core_MD FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
endif()