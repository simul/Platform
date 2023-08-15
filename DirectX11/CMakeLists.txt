cmake_minimum_required(VERSION 3.5)

file(GLOB CMAKE  "*.cmake")

file(GLOB SOURCES	"*.cpp")

file(GLOB HEADERS "*.h")

file(GLOB SHADERS
	"${SIMUL_PLATFORM_DIR}/Shaders/Sfx/*.sfx" 
)

file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl" 
	"${SIMUL_PLATFORM_DIR}/DirectX11/Sfx/*.sl"
)
file(GLOB SHADER_DEFS
	"${SIMUL_PLATFORM_DIR}/DirectX11/Sfx/*.json" 
	)

source_group("Shaders" FILES  ${SHADERS} )
source_group("Shader Includes" FILES ${SHADER_INCLUDES} )

add_static_library( SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${SHADER_DEFS} DEFINITIONS SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1 PLATFORM_D3D11_SFX=1 PLATFORM_TYPED_UAV_FORMATS=1 PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}
	INCLUDES "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" PUBLICINCLUDES "${SIMUL_PLATFORM_DIR}/DirectX11" )

 
if(SIMUL_SOURCE_BUILD)
	if(XBOXONE)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/Sfx/DirectX11.json" )
	elseif(GDK AND NOT PLATFORM_WINGDK)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/Sfx/DirectX11.json" )
	else()
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/DirectX11/Sfx/DirectX11.json" )
	endif()
	
	if(PLATFORM_BUILD_MD_LIBS)
		add_dynamic_library(SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
		target_compile_definitions(SimulDirectX11_MD PRIVATE SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1 PLATFORM_D3D11_SFX=1 PLATFORM_TYPED_UAV_FORMATS=1)
		set_target_properties(SimulDirectX11_MD PROPERTIES LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
		target_include_directories(SimulDirectX11_MD PUBLIC "${SIMUL_PLATFORM_DIR}/DirectX11")
		target_include_directories(SimulDirectX11_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
		LibraryDefaults(SimulDirectX11_MD)

		target_link_libraries(SimulDirectX11_MD SimulCrossPlatform_MD SimulMath_MD Core_MD directxtex_MD)

	endif()
endif()