cmake_minimum_required(VERSION 2.8)

include_directories( ../.. )

file(GLOB CMAKE
    "*.cmake"
)

file(GLOB SOURCES
    "*.cpp"
)

file(GLOB HEADERS
    "*.h"
)

# Setup and build the static library.
add_static_library( SimulCrossPlatform SOURCES ${SOURCES} ${HEADERS} ${CMAKE})

# Setup and build the dynamic library.
add_dynamic_library(SimulCrossPlatform SOURCES ${SOURCES} ${HEADERS} ${CMAKE}
	DEFINITIONS SIMUL_CROSSPLATFORM_DLL=1
	LINK SimulGeometry_MD SimulMath_MD SimulBase_MD)

