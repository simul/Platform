cmake_minimum_required(VERSION 3.12)

file(GLOB SOURCES
    "*.cpp"
)

file(GLOB HEADERS
    "*.h"
)

add_static_library( SimulMath SOURCES ${SOURCES} ${HEADERS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})



if (SIMUL_DLL_SDK_LIBRARY)
	AddDLLCompileDefinitions( SimulMath${STATIC_LINK_SUFFIX} )
endif()