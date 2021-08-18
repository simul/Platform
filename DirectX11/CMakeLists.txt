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

add_static_library( SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1 PLATFORM_D3D11_SFX=1 PLATFORM_TYPED_UAV_FORMATS=1 PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}
	INCLUDES "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" PUBLICINCLUDES "${SIMUL_PLATFORM_DIR}/DirectX11" )

add_dynamic_library(SimulDirectX11 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
 
if(SIMUL_SOURCE_BUILD)
	target_compile_definitions(SimulDirectX11_MD PRIVATE SIMUL_DIRECTX11_DLL=1 SIMUL_WIN8_SDK=1 PLATFORM_D3D11_SFX=1 PLATFORM_TYPED_UAV_FORMATS=1)
	set_target_properties(SimulDirectX11_MD PROPERTIES LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
	target_include_directories(SimulDirectX11_MD PUBLIC "${SIMUL_PLATFORM_DIR}/DirectX11")
	target_include_directories(SimulDirectX11_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
	LibraryDefaults(SimulDirectX11_MD)

	if(XBOXONE)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/Sfx/DirectX11.json" )
	elseif(GDK AND NOT PLATFORM_WINGDK)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/Sfx/DirectX11.json" )
	else()
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/DirectX11/Sfx/DirectX11.json" )
	endif()
	
	#message(STATUS JSON_FILE ${JSON_FILE} )
	add_sfx_shader_project( DirectX11CoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Sfx" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${JSON} ${CMAKE} OPTIONS -w OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaderbin" DEFINES SIMUL_XDK_VERSION=${XDK_TOOLCHAIN_VERSION} GDK_BIN_PATH=${GDK_BIN_PATH} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders )

	if(PLATFORM_WINDOWS)
		add_dependencies(SimulDirectX11_MT DirectX11CoreShaders )
		#add_dependencies(DirectX12Shaders Sfx )
	endif()
	add_dependencies(SimulDirectX11_MD DirectX11CoreShaders )

	target_link_libraries(SimulDirectX11_MD SimulCrossPlatform_MD SimulMath_MD Core_MD directxtex_MD)

endif()