# This module is shared; use include blocker.
if( _XB1_TOOLCHAIN_ )
	return()
endif()
set(_XB1_TOOLCHAIN_ 1)

# GDK version requirement
set(REQUIRED_GDK_TOOLCHAIN_VERSION "221001" CACHE STRING "Choose the GDK version" )

# Get GDK environment
if( EXISTS "$ENV{GameDK}" AND IS_DIRECTORY "$ENV{GameDK}" )
	string(REGEX REPLACE "\\\\" "/" GDK_ROOT $ENV{GameDK})
	string(REGEX REPLACE "//" "/" GDK_ROOT ${GDK_ROOT})
endif()

# Fail if GDK not found
if( NOT GDK_ROOT )
	if( PLATFORM_TOOLCHAIN_ENVIRONMENT_ONLY )
		return()
	endif()
	message(FATAL_ERROR "Requires GDK to be installed in order to build platform.")
endif()

# Get toolchain version
message("[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\GDK\\${REQUIRED_GDK_TOOLCHAIN_VERSION}\\GXDK\\;EditionVersion]")
get_filename_component(GDK_TOOLCHAIN_VERSION "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\GDK\\${REQUIRED_GDK_TOOLCHAIN_VERSION}\\GXDK\\;EditionVersion]" NAME)

if( GDK_TOOLCHAIN_VERSION STREQUAL REQUIRED_GDK_TOOLCHAIN_VERSION )
	message(STATUS "Found required GDK toolchain version (${GDK_TOOLCHAIN_VERSION})")
else()
	get_filename_component(GDK_TOOLCHAIN_VERSION "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\GDK;Latest]" NAME)
	message(WARNING "Could not find required GDK toolchain version (${REQUIRED_GDK_TOOLCHAIN_VERSION}), using latest version instead (${GDK_TOOLCHAIN_VERSION})")
endif()

# If we only want the environment values, exit now
if( PLATFORM_TOOLCHAIN_ENVIRONMENT_ONLY )
	return()
endif()

# Find GDK compiler directory
if( CMAKE_GENERATOR STREQUAL "Visual Studio 15 2017" )
	set(GDK_COMPILER_DIR "C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional" )
elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 16 2019" )
	set(GDK_COMPILER_DIR "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community" )
elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 17 2022" )
	set(GDK_COMPILER_DIR "C:/Program Files/Microsoft Visual Studio/2022/Community" )
else()
	message(FATAL_ERROR "Unsupported Visual Studio version!")
endif()

#set(GDK_TOOLS_DIR "${GDK_COMPILER_DIR}/VC/Tools/MSVC/14.25.28610/bin/Hostx64/x64")
file(GLOB_RECURSE GDK_TOOLS_CL "${GDK_COMPILER_DIR}/VC/Tools/MSVC/**/bin/Hostx64/x64/cl.exe" )
list(REVERSE GDK_TOOLS_CL)
list(GET GDK_TOOLS_CL 0 GDK_TOOLS_CL)
message("get_filename_component(GDK_TOOLS_CL ${GDK_TOOLS_CL} DIRECTORY)")
get_filename_component(GDK_TOOLS_DIR "${GDK_TOOLS_CL}" DIRECTORY)
message("GDK_TOOLS_DIR ${GDK_TOOLS_DIR}")
#find_path (GDK_TOOLS_DIR bin/Hostx64/x64/cl.exe PATHS "${GDK_COMPILER_DIR}/VC/Tools/MSVC")
set(CMAKE_C_COMPILER "${GDK_TOOLS_DIR}/cl.exe")
set(CMAKE_CXX_COMPILER "${GDK_TOOLS_DIR}/cl.exe")
set(CMAKE_ASM_COMPILER "${GDK_TOOLS_DIR}/ml64.exe")

# Tell CMake we are cross-compiling to GDK
set(CMAKE_SYSTEM_NAME WinGDK)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_VS_PLATFORM_NAME Gaming.Desktop.x64)
set(CMAKE_GENERATOR_PLATFORM Gaming.Desktop.x64)

set(PLATFORM_WINGDK True)
set(GDK True)

# Set the compilers to the ones found in GDK directory
# Set CMake system root search paet(CMAKE_SYSROOT "${GDK_COMPILER_DIR}")

# Force compilers to skip detecting compiler ABI info and compile features
set(CMAKE_C_COMPILER_FORCED True)
set(CMAKE_CXX_COMPILER_FORCED True)
set(CMAKE_ASM_COMPILER_FORCED True)

# Only search the GDK, not the remainder of the host file system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

if(SIMUL_PLATFORM_DIR)
include_directories( ${SIMUL_PLATFORM_DIR}/Windows )
else()
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/Windows )
endif()

# Global variables
set(DYNAMIC_LINK_SUFFIX "_GDK")
set(STATIC_LINK_SUFFIX "_GDK")
set(MSVC 1)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od /Ob2 /DNDEBUG /Zi /EHsc")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /DDEBUG /D_DEBUG /RTC1 /EHsc")

link_directories("${GDK_ROOT}/${GDK_TOOLCHAIN_VERSION}/GRDK/gameKit/Lib/amd64/"
"$(Console_WindowsIncludeRoot)/um/x64"
"$(Console_WindowsIncludeRoot)/ucrt/x64")

#set(CMAKE_EXE_LINKER_FLAGS /NODEFAULTLIB)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od /Ob2 /DNDEBUG /Zi /EHsc")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /DDEBUG /D_DEBUG /RTC1 /EHsc")
set(CMAKE_LINK_LIBRARY_FLAG "")

set(PLATFORM_BUILD_MD_LIBS true)

set(PLATFORM_USE_FMT true)
set(FMT_COMPILER_FEATURE_CHECKS OFF)