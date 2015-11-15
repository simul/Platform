cmake_minimum_required(VERSION 2.8)

include_directories( ../.. )

file(GLOB SOURCES
    "*.cpp"
)

file(GLOB HEADERS
    "*.h"
)

add_library(SimulCrossPlatform ${SOURCES} ${HEADERS})
