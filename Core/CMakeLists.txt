# Copyright 2020 Simul Software Ltd, All rights reserved.

cmake_minimum_required( VERSION 3.2 )
# Grab the includes/source files.

file(GLOB SOURCES "*.cpp" )
file(GLOB HEADERS "*.h" )
file(GLOB DOCS "*.rst" )


set(PLATFORM_STD_FILESYSTEM 1)
if(XBOXONE)
	set(PLATFORM_STD_FILESYSTEM 2)
elseif(PLATFORM_PS4)
	set(PLATFORM_STD_FILESYSTEM 0)
elseif(PLATFORM_LINUX)
	set(PLATFORM_STD_FILESYSTEM 2)
endif()

# Setup and build the static library.
add_static_library( Core SOURCES ${SOURCES} ${HEADERS} ${DOCS} DEFINITIONS PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} LINK mt_fmt$<$<CONFIG:DEBUG>:d>)
target_link_directories( Core_MT PUBLIC ${CMAKE_SOURCE_DIR}/Platform/External/fmt/build_mt/lib/${CMAKE_BUILD_TYPE})
target_include_directories( Core_MT PUBLIC "${SIMUL_PLATFORM_DIR}/External/fmt/include" )

if(PLATFORM_BUILD_MD_LIBS)
# Setup and build the dynamic library.
add_dynamic_library( Core SOURCES ${SOURCES} ${HEADERS} ${DOCS} DEFINITIONS PLATFORM_CORE_DLL=1 PLATFORM_STD_FILESYSTEM=${PLATFORM_STD_FILESYSTEM} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} LINK md_fmt$<$<CONFIG:DEBUG>:d>)
target_link_directories( Core_MD PUBLIC ${CMAKE_SOURCE_DIR}/Platform/External/fmt/build_md/lib/${CMAKE_BUILD_TYPE})
target_include_directories( Core_MD PUBLIC "${SIMUL_PLATFORM_DIR}/External/fmt/include" )
endif()

