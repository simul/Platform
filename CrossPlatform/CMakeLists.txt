cmake_minimum_required(VERSION 2.8)

file(GLOB SOURCES "*.cpp" )

file(GLOB HEADERS "*.h" )

# Setup and build the static library.
add_static_library( SimulCrossPlatform SOURCES ${SOURCES} ${HEADERS}  FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

# Setup and build the dynamic library.
add_dynamic_library(SimulCrossPlatform SOURCES ${SOURCES} ${HEADERS} 
	DEFINITIONS SIMUL_CROSSPLATFORM_DLL=1
	LINK SimulMath_MD SimulBase_MD FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
	 