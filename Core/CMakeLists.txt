# Copyright 2020 Simul Software Ltd, All rights reserved.

cmake_minimum_required(VERSION 3.5)
# Grab the includes/source files.

file(GLOB SOURCES "*.cpp" )
file(GLOB HEADERS "*.h" )
file(GLOB DOCS "*.rst" )

# Setup and build the static library.
add_static_library( Core SOURCES ${SOURCES} ${HEADERS} ${DOCS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

if(SIMUL_SOURCE_BUILD)
	target_compile_definitions( Core${STATIC_LINK_SUFFIX} PUBLIC FMT_HEADER_ONLY )
else()
	target_compile_definitions( Core${STATIC_LINK_SUFFIX} INTERFACE FMT_HEADER_ONLY )
endif()

if (SIMUL_DLL_SDK_LIBRARY)
	AddDLLCompileDefinitions( Core${STATIC_LINK_SUFFIX} )
endif()
