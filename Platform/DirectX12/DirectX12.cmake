
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

add_static_library(SimulDirectX12_MT ${SOURCES} ${HEADERS} ${CMAKE} )
target_compile_definitions(SimulDirectX12_MT PRIVATE SIMUL_DIRECTX12_DLL=1)
set_target_properties(SimulDirectX12_MT PROPERTIES FOLDER Static)
target_include_directories(SimulDirectX12_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/DirectX12")
target_include_directories(SimulDirectX12_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
LibraryDefaults(SimulDirectX12_MT)

link_directories( ${CMAKE_SOURCE_DIR}/lib/x64/v141/${CMAKE_BUILD_TYPE})
add_library(SimulDirectX12_MD SHARED ${SOURCES} ${HEADERS} ${CMAKE} )
target_compile_definitions(SimulDirectX12_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_DIRECTX12_DLL=1)
set_target_properties(SimulDirectX12_MD PROPERTIES FOLDER Dynamic
							LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
target_include_directories(SimulDirectX12_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/DirectX12")
target_include_directories(SimulDirectX12_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
LibraryDefaults(SimulDirectX12_MD)

add_sfx_shader_project( DirectX12Shaders "${CMAKE_CURRENT_SOURCE_DIR}/HLSL/HLSL12.json" INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/HLSL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${CMAKE} OPTIONS -w)

add_dependencies(SimulDirectX12_MT DirectX12Shaders)
add_dependencies(SimulDirectX12_MD DirectX12Shaders)

target_link_libraries(SimulDirectX12_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

