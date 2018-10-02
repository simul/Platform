cmake_minimum_required(VERSION 2.8)

file(GLOB SOURCES
	"*.cpp" "glad.c"
)

file(GLOB HEADERS
    "*.h"
)
add_library(SimulDirectX12_MT ${SOURCES} ${HEADERS})
target_compile_definitions(SimulDirectX12_MT PRIVATE DSIMUL_DIRECTX12_DLL=1)
set_target_properties(SimulDirectX12_MT PROPERTIES FOLDER Static)
target_include_directories(SimulDirectX12_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/DirectX12")

add_library(SimulDirectX12_MD SHARED ${SOURCES} ${HEADERS})
target_compile_definitions(SimulDirectX12_MD PRIVATE SIMUL_DYNAMIC_LINK=1 DSIMUL_DIRECTX12_DLL=1)
set_target_properties(SimulDirectX12_MD PROPERTIES FOLDER Dynamic)
target_include_directories(SimulDirectX12_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/DirectX12")


target_link_libraries(SimulDirectX12_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

