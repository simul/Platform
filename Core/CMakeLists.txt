# Copyright 2020 Simul Software Ltd, All rights reserved.

cmake_minimum_required(VERSION 3.5)
# Grab the includes/source files.

file(GLOB SOURCES "*.cpp" )
file(GLOB HEADERS "*.h" )
file(GLOB DOCS "*.rst" )

if(PLATFORM_PS4)
	set(PLATFORM_STD_FILESYSTEM 0)
endif()

# Setup and build the static library.
add_static_library( Core SOURCES ${SOURCES} ${HEADERS} ${DOCS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

if(PLATFORM_BUILD_MD_LIBS)
# Setup and build the dynamic library.
add_dynamic_library( Core SOURCES ${SOURCES} ${HEADERS} ${DOCS} DEFINITIONS PLATFORM_CORE_DLL=1 LINK fmt::fmt-header-only FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
add_compile_definitions( Core_MD PRIVATE FMT_HEADER_ONLY)
endif()

