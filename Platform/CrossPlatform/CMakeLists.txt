cmake_minimum_required(VERSION 2.8)

include_directories( ../.. )

file(GLOB SOURCES
    "*.cpp"
)

file(GLOB HEADERS
    "*.h"
)

add_library(SimulCrossPlatform_MT ${SOURCES} ${HEADERS})

add_definitions(-DSIMUL_DYNAMIC_LINK -DSIMUL_CROSSPLATFORM_DLL)
add_library(SimulCrossPlatform_MD SHARED ${SOURCES} ${HEADERS})
target_link_libraries(SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

