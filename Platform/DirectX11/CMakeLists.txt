cmake_minimum_required(VERSION 2.8)

file(GLOB CMAKE
    "*.cmake"
)

file(GLOB SOURCES
	"*.cpp" "glad.c"
)

file(GLOB HEADERS
    "*.h"
)
add_library(SimulDirectX11_MT ${SOURCES} ${HEADERS} ${CMAKE})
target_compile_definitions(SimulDirectX11_MT PRIVATE SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1)
set_target_properties(SimulDirectX11_MT PROPERTIES FOLDER Static)
target_include_directories(SimulDirectX11_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/Effects11_Win8SDK/inc" )
target_include_directories(SimulDirectX11_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_include_directories(SimulDirectX11_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11" )
LibraryDefaults(SimulDirectX11_MT)

link_directories( ${CMAKE_SOURCE_DIR}/lib//x64/v141/${CMAKE_BUILD_TYPE})
add_library(SimulDirectX11_MD SHARED ${SOURCES} ${HEADERS} ${CMAKE})
target_compile_definitions(SimulDirectX11_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1)
set_target_properties(SimulDirectX11_MD PROPERTIES FOLDER Dynamic)
target_include_directories(SimulDirectX11_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/Effects11_Win8SDK/inc" )
target_include_directories(SimulDirectX11_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_include_directories(SimulDirectX11_MD PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
LibraryDefaults(SimulDirectX11_MD)

target_link_libraries(SimulDirectX11_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

