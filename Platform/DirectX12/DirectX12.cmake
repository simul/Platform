
file(GLOB CMAKE
    "*.cmake"
)

file(GLOB SOURCES
	"*.cpp" "glad.c"
)

file(GLOB HEADERS
    "*.h"
)
add_library(SimulDirectX12_MT ${SOURCES} ${HEADERS} ${CMAKE} )
target_compile_definitions(SimulDirectX12_MT PRIVATE SIMUL_DIRECTX12_DLL=1)
set_target_properties(SimulDirectX12_MT PROPERTIES FOLDER Static)
target_include_directories(SimulDirectX12_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/DirectX12")
target_include_directories(SimulDirectX12_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
LibraryDefaults(SimulDirectX12_MT)

add_library(SimulDirectX12_MD SHARED ${SOURCES} ${HEADERS} ${CMAKE} )
target_compile_definitions(SimulDirectX12_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_DIRECTX12_DLL=1)
set_target_properties(SimulDirectX12_MD PROPERTIES FOLDER Dynamic)
target_include_directories(SimulDirectX12_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/DirectX12")
target_include_directories(SimulDirectX12_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
LibraryDefaults(SimulDirectX12_MD)


target_link_libraries(SimulDirectX12_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

