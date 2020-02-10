cmake_minimum_required(VERSION 2.8)

file(GLOB CMAKE
    "*.cmake"
)

file(GLOB SOURCES
    "*.cpp"
)

file(GLOB HEADERS
    "*.h"
)

add_static_library( SimulMath SOURCES ${SOURCES} ${HEADERS} ${CMAKE})

add_dynamic_library(SimulMath SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS SIMUL_MATH_DLL=1 LINK SimulBase_MD)

