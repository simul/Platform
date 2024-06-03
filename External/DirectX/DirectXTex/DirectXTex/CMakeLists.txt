# DirectX Texture Library
#
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
#
# http://go.microsoft.com/fwlink/?LinkId=248926

cmake_minimum_required (VERSION 3.8)
project (DirectXTex_CMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DIRECTX_TEX_SRC 
    BC.h
    BCDirectCompute.h
    d3dx12.h
    DDS.h
    DirectXTex.h
    DirectXTexP.h
    filters.h
    scoped.h
    BC.cpp
    BC4BC5.cpp
    BC6HBC7.cpp
    BCDirectCompute.cpp
    DirectXTexCompress.cpp
    DirectXTexCompressGPU.cpp
    DirectXTexConvert.cpp
    DirectXTexD3D11.cpp
    DirectXTexD3D12.cpp
    DirectXTexDDS.cpp
    DirectXTexFlipRotate.cpp
    DirectXTexHDR.cpp
    DirectXTexImage.cpp
    DirectXTexMipmaps.cpp
    DirectXTexMisc.cpp
    DirectXTexNormalMaps.cpp
    DirectXTexPMAlpha.cpp
    DirectXTexResize.cpp
    DirectXTexTGA.cpp
    DirectXTexUtil.cpp
    DirectXTexWIC.cpp)


if(PLATFORM_WINDOWS )
	add_static_library (directxtex SOURCES ${DIRECTX_TEX_SRC} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
endif()
add_static_library(directxtex SOURCES ${DIRECTX_TEX_SRC} RUNTIME MD FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

#target_compile_options( directxtex PRIVATE /fp:fast )

if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
elseif ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    target_compile_options( directxtex PRIVATE -Wall -Wpedantic -Wextra )
    if (${CMAKE_SIZEOF_VOID_P} EQUAL "4")
        target_compile_options( directxtex PRIVATE /arch:SSE2 )
    endif()
endif()

# Windows 10 is used here to build the DirectX 12 code paths as well as 11
add_compile_definitions(_UNICODE UNICODE _WIN32_WINNT=0x0A00)
if (PLATFORM_SUPPORT_WIN7)
    add_compile_definitions(SUPPORT_WINDOWS_7=1)
else()
    add_compile_definitions(SUPPORT_WINDOWS_7=0)
endif()