cmake_minimum_required(VERSION 2.8)

file(GLOB SOURCES
	"*.cpp" "glad.c"
)

file(GLOB HEADERS
    "*.h"
)
add_library(SimulDirectX11_MT ${SOURCES} ${HEADERS})
set_target_properties(SimulDirectX11_MT PROPERTIES FOLDER Static)
target_include_directories(SimulDirectX11_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/Effects11/inc" )
target_include_directories(SimulDirectX11_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11" )

add_library(SimulDirectX11_MD SHARED ${SOURCES} ${HEADERS})
target_compile_definitions(SimulDirectX11_MD PRIVATE SIMUL_DYNAMIC_LINK=1 DSIMUL_DIRECTX11_DLL=1)
set_target_properties(SimulDirectX11_MD PROPERTIES FOLDER Dynamic)
target_include_directories(SimulDirectX11_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/Effects11/inc" )
target_include_directories(SimulDirectX11_MD PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")


target_link_libraries(SimulDirectX11_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

