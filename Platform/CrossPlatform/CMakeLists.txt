cmake_minimum_required(VERSION 2.8)

include_directories( ../.. )

file(GLOB SOURCES
    "*.cpp"
)

file(GLOB HEADERS
    "*.h"
)

# Setup and build the static library.
add_library(SimulCrossPlatform_MT ${SOURCES} ${HEADERS})
set_target_properties(SimulCrossPlatform_MT PROPERTIES PREFIX "" )
set_target_properties(SimulCrossPlatform_MT PROPERTIES FOLDER Static)

# Setup and build the dynamic library.
add_definitions(-DSIMUL_DYNAMIC_LINK -DSIMUL_CROSSPLATFORM_DLL)
add_library(SimulCrossPlatform_MD SHARED ${SOURCES} ${HEADERS})
set_target_properties(SimulCrossPlatform_MD PROPERTIES PREFIX "" )
set_target_properties(SimulCrossPlatform_MD PROPERTIES FOLDER Dynamic)
target_link_libraries(SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

