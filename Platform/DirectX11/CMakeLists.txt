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

file(GLOB SHADERS
	"${CMAKE_SOURCE_DIR}/Platform/Crossplatform/SFX/*.sfx" 
)


file(GLOB SHADER_INCLUDES
	"${CMAKE_SOURCE_DIR}/Platform/Crossplatform/SL/*.sl"
)

source_group("Shaders" FILES  ${SHADERS} )
source_group("Shader Includes" FILES ${SHADER_INCLUDES} )
#add_custom_target(SimulDirectX11_Shaders DEPENDS ${SHADERS} ${SHADER_INCLUDES} ${CMAKE} SOURCES ${SHADERS} ${SHADER_INCLUDES} ${CMAKE})
#add_fx_shaders(${SHADERS})
add_fx_shader_project( DirectX11Shaders ${SHADERS} ${SHADER_INCLUDES} )

add_static_library(SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${SHADERS} ${CMAKE})
target_compile_definitions(SimulDirectX11_MT PRIVATE SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1)
set_target_properties(SimulDirectX11_MT PROPERTIES FOLDER Static)
target_include_directories(SimulDirectX11_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/Effects11_Win8SDK/inc" )
target_include_directories(SimulDirectX11_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_include_directories(SimulDirectX11_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11" )
LibraryDefaults(SimulDirectX11_MT)
add_dependencies(SimulDirectX11_MT DirectX11Shaders effects11)


link_directories( ${CMAKE_SOURCE_DIR}/lib//x64/v141/${CMAKE_BUILD_TYPE})
add_dynamic_library(SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${SHADERS} ${CMAKE})
target_compile_definitions(SimulDirectX11_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1)
set_target_properties(SimulDirectX11_MD PROPERTIES FOLDER Dynamic
							LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
target_include_directories(SimulDirectX11_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/Effects11_Win8SDK/inc" )
target_include_directories(SimulDirectX11_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_include_directories(SimulDirectX11_MD PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
LibraryDefaults(SimulDirectX11_MD)

target_link_libraries(SimulDirectX11_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)
add_dependencies(SimulDirectX11_MD DirectX11Shaders effects11)
