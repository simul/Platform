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
	"${SIMUL_PLATFORM_DIR}/Shaders/Sfx/*.sfx" 
)

file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl"
)

source_group("Shaders" FILES  ${SHADERS} )
source_group("Shader Includes" FILES ${SHADER_INCLUDES} )
#add_custom_target(SimulDirectX11_Shaders DEPENDS ${SHADERS} ${SHADER_INCLUDES} ${CMAKE} SOURCES ${SHADERS} ${SHADER_INCLUDES} ${CMAKE})
#add_fx_shaders(${SHADERS})
#add_fx_shader_project( DirectX11CoreShaders INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/HLSL" "${SIMUL_PLATFORM_DIR}/Shaders/SL" "${SIMUL_PLATFORM_DIR}/CrossPlatform/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaderbin" FOLDER "${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders" )

	if(XBOXONE)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/HLSL11.json" )
	elseif(GDK)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/HLSL11X.json" )
	else()
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/DirectX11/HLSL/HLSL11.json" )
	endif() 

add_sfx_shader_project( DirectX11CoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/HLSL" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${JSON} ${CMAKE} OPTIONS -w OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaderbin" -ESIMUL_XDK_VERSION=${XDK_TOOLCHAIN_VERSION} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders )

add_static_library(SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
if(SIMUL_SOURCE_BUILD)
	target_compile_definitions(SimulDirectX11_MT PRIVATE SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1)
	
	target_include_directories(SimulDirectX11_MT PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/Effects11_Win8SDK/inc" )
	target_include_directories(SimulDirectX11_MT PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
	target_include_directories(SimulDirectX11_MT PRIVATE "${SIMUL_PLATFORM_DIR}/DirectX11" )
	LibraryDefaults(SimulDirectX11_MT)
	add_dependencies(SimulDirectX11_MT effects11_MT directxtex_MT )
	if(SIMUL_BUILD_SHADERS)
		#add_dependencies( SimulDirectX11_MT DirectX11Shaders )
	endif()
endif()

add_dynamic_library(SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

if(SIMUL_SOURCE_BUILD)
	target_compile_definitions(SimulDirectX11_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1)
	set_target_properties(SimulDirectX11_MD PROPERTIES 
								LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
	target_include_directories(SimulDirectX11_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/Effects11_Win8SDK/inc" )
	target_include_directories(SimulDirectX11_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
	target_include_directories(SimulDirectX11_MD PRIVATE "${SIMUL_PLATFORM_DIR}/DirectX11")
	LibraryDefaults(SimulDirectX11_MD)

	target_link_libraries(SimulDirectX11_MD SimulCrossPlatform_MD SimulMath_MD Core_MD effects11_MD directxtex_MD )
	if(SIMUL_BUILD_SHADERS)
	#add_dependencies(SimulDirectX11_MD DirectX11Shaders )
	endif()
endif()
