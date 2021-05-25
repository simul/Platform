cmake_minimum_required(VERSION 2.9)

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
	"${SIMUL_PLATFORM_DIR}/Shaders/Sfx/*.sfx" 
)

file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl"
	"${SIMUL_PLATFORM_DIR}/DirectX11/Sfx/*.sl"
)

source_group("Shaders" FILES  ${SHADERS} )
source_group("Shader Includes" FILES ${SHADER_INCLUDES} )


if(XBOXONE)
	set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/Sfx/DirectX11.json" )
elseif(GDK AND NOT PLATFORM_WINGDK)
	set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/Sfx/DirectX11.json" )
else()
	set( JSON_FILE "${SIMUL_PLATFORM_DIR}/DirectX11/Sfx/DirectX11.json" )
endif()
add_sfx_shader_project( DirectX11CoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Sfx" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${JSON} ${CMAKE} OPTIONS -w OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaderbin" DEFINES SIMUL_XDK_VERSION=${XDK_TOOLCHAIN_VERSION} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders )


add_static_library(SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
if(SIMUL_SOURCE_BUILD AND NOT PLATFORM_WINGDK)
	target_compile_definitions(SimulDirectX11_MT PRIVATE SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1 PLATFORM_D3D11_SFX=1 PLATFORM_TYPED_UAV_FORMATS=1)

	target_include_directories(SimulDirectX11_MT PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
	target_include_directories(SimulDirectX11_MT PRIVATE "${SIMUL_PLATFORM_DIR}/DirectX11" )
	LibraryDefaults(SimulDirectX11_MT)
		add_dependencies(SimulDirectX11_MT directxtex_MT )
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( SimulDirectX11_MT DirectX11CoreShaders )
	endif()
endif()

add_dynamic_library(SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

if(SIMUL_SOURCE_BUILD)
	target_compile_definitions(SimulDirectX11_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1 PLATFORM_D3D11_SFX=1 PLATFORM_TYPED_UAV_FORMATS=1)

	set_target_properties(SimulDirectX11_MD PROPERTIES 
								LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
	
	target_include_directories(SimulDirectX11_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
	target_include_directories(SimulDirectX11_MD PRIVATE "${SIMUL_PLATFORM_DIR}/DirectX11")
	LibraryDefaults(SimulDirectX11_MD)

	target_link_libraries(SimulDirectX11_MD SimulCrossPlatform_MD SimulMath_MD Core_MD directxtex_MD )

	if(SIMUL_BUILD_SHADERS)
		add_dependencies(SimulDirectX11_MD DirectX11CoreShaders )
	endif()
endif()
