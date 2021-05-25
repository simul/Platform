cmake_minimum_required(VERSION 2.9)
option(SIMUL_D3D12_ENABLE_PIX "" OFF)
option(PLATFORM_SUPPORT_D3D12_RAYTRACING "" OFF)

file(GLOB CMAKE
	"*.cmake"
)

file(GLOB SOURCES
	"*.cpp" "**/*.cpp"
)

file(GLOB HEADERS
	"*.h" "**/*.h"

)

file(GLOB JSON
	"${CMAKE_CURRENT_SOURCE_DIR}/HLSL/*.json"
	"${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/*.json"
)

file(GLOB SHADERS
	"${SIMUL_PLATFORM_DIR}/Shaders/Sfx/*.sfx" 
)

file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl"
	"${SIMUL_PLATFORM_DIR}/DirectX12/HLSL/*.sl"
)
source_group("NV" src FILES "**/*.cpp" "**/*.h" )

set(in_RootSignature "${CMAKE_CURRENT_SOURCE_DIR}/Sfx/GFX.hlsl")

set(DX12_DEFS SIMUL_DIRECTX12_DLL=1 PLATFORM_SUPPORT_D3D12_RAYTRACING=$<BOOL:${PLATFORM_SUPPORT_D3D12_RAYTRACING}> )

if(${SIMUL_D3D12_ENABLE_PIX})
	set(DX12_DEFS ${DX12_DEFS} SIMUL_ENABLE_PIX=1 USE_PIX)
endif()

add_static_library( SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS ${DX12_DEFS} PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}
	INCLUDES "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" PUBLICINCLUDES "${SIMUL_PLATFORM_DIR}/DirectX12" )

add_dynamic_library(SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS ${DX12_DEFS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
 
if(SIMUL_SOURCE_BUILD)
	target_compile_definitions(SimulDirectX12_MD PRIVATE ${DX12_DEFS})
	set_target_properties(SimulDirectX12_MD PROPERTIES LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
	target_include_directories(SimulDirectX12_MD PUBLIC "${SIMUL_PLATFORM_DIR}/DirectX12")
	target_include_directories(SimulDirectX12_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
	LibraryDefaults(SimulDirectX12_MD)

	if(XBOXONE)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/HLSL12X.json" )
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING False)
	elseif(PLATFORM_SPECTRUM)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/Spectrum/Sfx/Spectrum.json" )
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING True)
	elseif(GDK AND NOT PLATFORM_WINGDK)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/HLSL12GDK.json" )
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING False)
	else()
		set( JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Sfx/DirectX12.json" )
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING True)
	endif()
	#message(STATUS JSON_FILE ${JSON_FILE} )
	add_sfx_shader_project( DirectX12CoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Sfx" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${JSON} ${CMAKE} OPTIONS -w OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaderbin" DEFINES SIMUL_XDK_VERSION=${XDK_TOOLCHAIN_VERSION} GDK_BIN_PATH=${GDK_BIN_PATH} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders )

	if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	add_dependencies(SimulDirectX12_MT DirectX12CoreShaders )
	#add_dependencies(DirectX12Shaders Sfx )
	endif()
	add_dependencies(SimulDirectX12_MD DirectX12CoreShaders )

	target_link_libraries(SimulDirectX12_MD SimulCrossPlatform_MD SimulMath_MD Core_MD directxtex_MD)

endif()