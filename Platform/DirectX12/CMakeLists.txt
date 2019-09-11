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

file(GLOB JSON
    "${CMAKE_CURRENT_SOURCE_DIR}/HLSL/*.json"
    "${CMAKE_SOURCE_DIR}/Platform/XboxOne/HLSL/*.json"
)

file(GLOB SHADERS
	"${CMAKE_SOURCE_DIR}/Platform/Crossplatform/Sfx/*.sfx" 
)


file(GLOB SHADER_INCLUDES
	"${CMAKE_SOURCE_DIR}/Platform/Crossplatform/SL/*.sl"
)
set(in_rs "${CMAKE_CURRENT_SOURCE_DIR}/HLSL/GFX.hlsl")

add_static_library( SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS SIMUL_DIRECTX12_DLL=1 PROPERTIES FOLDER Static
	INCLUDES "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" PUBLICINCLUDES "${CMAKE_SOURCE_DIR}/Platform/DirectX12" )

add_dynamic_library(SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} )

target_compile_definitions(SimulDirectX12_MD PRIVATE SIMUL_DIRECTX12_DLL=1)
set_target_properties(SimulDirectX12_MD PROPERTIES FOLDER Dynamic
							LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
target_include_directories(SimulDirectX12_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/DirectX12")
target_include_directories(SimulDirectX12_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
LibraryDefaults(SimulDirectX12_MD)

if(XBOXONE)
set( JSON_FILE "${CMAKE_SOURCE_DIR}/Platform/XboxOne/HLSL/HLSL12X.json" )
else()
set( JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/HLSL/HLSL12.json" )
endif()
#message(STATUS JSON_FILE ${JSON_FILE} )
add_sfx_shader_project( DirectX12Shaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/HLSL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${JSON} ${CMAKE} ${in_rs} OPTIONS -w -ESIMUL_XDK_VERSION=${XDK_TOOLCHAIN_VERSION} )

# ROOT SIG BLOB!
set(out_rs "${CMAKE_CURRENT_BINARY_DIR}/shaderbin/GFX.cso")
add_custom_command(OUTPUT ${out_rs}
				COMMAND ${SIMUL_FX_EXECUTABLE} /T rootsig_1_0 /E"GFXRS" /I"HLSL" /Fo"${out_rs}" /nologo ${in_rs}
				MAIN_DEPENDENCY ${in_rs}
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				COMMENT "${SIMUL_FX_EXECUTABLE} /E\"GFXRS\" /I\"HLSL\" /Fo\"${out_rs}\" /nologo ${in_rs}"
				)


if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
add_dependencies(SimulDirectX12_MT DirectX12Shaders )
add_dependencies(DirectX12Shaders Sfx )
endif()
add_dependencies(SimulDirectX12_MD DirectX12Shaders )

target_link_libraries(SimulDirectX12_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD directxtex_MD)

