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
add_static_library(SimulCrossPlatform_MT ${SOURCES} ${HEADERS} ${CMAKE})
set_target_properties(SimulCrossPlatform_MT PROPERTIES PREFIX "" )
set_target_properties(SimulCrossPlatform_MT PROPERTIES FOLDER Static)

# Setup and build the dynamic library.
add_library(SimulCrossPlatform_MD SHARED ${SOURCES} ${HEADERS} ${CMAKE})
target_compile_definitions(SimulCrossPlatform_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_CROSSPLATFORM_DLL=1)
set_target_properties(SimulCrossPlatform_MD PROPERTIES PREFIX "" )
set_target_properties(SimulCrossPlatform_MD PROPERTIES FOLDER Dynamic)
target_link_libraries(SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

