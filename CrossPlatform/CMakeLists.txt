cmake_minimum_required(VERSION 2.8)

file(GLOB SOURCES "*.cpp" )

file(GLOB HEADERS "*.h" )

if(PLATFORM_LOAD_RENDERDOC)
	add_definitions(-DPLATFORM_LOAD_RENDERDOC=1)
endif()

# Setup and build the static library.
add_static_library( SimulCrossPlatform SOURCES ${SOURCES} ${HEADERS}
	DEFINITIONS PLATFORM_LOAD_RENDERDOC=$<BOOL:${PLATFORM_LOAD_RENDERDOC}>
	FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

# Setup and build the dynamic library.
add_dynamic_library(SimulCrossPlatform SOURCES ${SOURCES} ${HEADERS} 
	DEFINITIONS SIMUL_CROSSPLATFORM_DLL=1 PLATFORM_LOAD_RENDERDOC=$<BOOL:${PLATFORM_LOAD_RENDERDOC}>
	LINK SimulMath_MD Core_MD FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

