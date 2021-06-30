# Copyright 2020 Simul Software Ltd, All rights reserved.

cmake_minimum_required( VERSION 3.2 )
# Grab the includes/source files.

file(GLOB SOURCES "*.cpp" )
file(GLOB HEADERS "*.h" )
file(GLOB DOCS "*.rst" )

# Setup and build the static library.
add_static_library( Core SOURCES ${SOURCES} ${HEADERS} ${DOCS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

# Setup and build the dynamic library.
add_dynamic_library( Core SOURCES ${SOURCES} ${HEADERS} ${DOCS} DEFINITIONS PLATFORM_CORE_DLL=1 FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
  