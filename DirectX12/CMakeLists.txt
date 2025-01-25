cmake_minimum_required(VERSION 3.12)
option(PLATFORM_D3D12_ENABLE_PIX "" OFF)
option(PLATFORM_SUPPORT_D3D12_RAYTRACING "" OFF)
option(PLATFORM_SUPPORT_D3D12_VIEWINSTANCING "" OFF)
option(PLATFORM_DEBUG_BARRIERS "Whether to output debug info for resource layout transition barriers. Slow." OFF)

if(!SIMUL_INTERNAL_CHECKS)
	set(PLATFORM_DEBUG_BARRIERS OFF)
endif()

file(GLOB CMAKE
	"*.cmake"
)

file(GLOB SOURCES
	"*.cpp" "**/*.cpp"
)

file(GLOB HEADERS
	"*.h" "**/*.h"
)

if(PLATFORM_SPECTRUM)
	set( FOLDER "Spectrum" )
elseif(PLATFORM_XBOXONE)
	set( FOLDER "XboxOne" )
else()
	set( FOLDER "DirectX12" )
endif()

file(GLOB SHADERS
	"${SIMUL_PLATFORM_DIR}/${FOLDER}/Sfx/*.sfx"
)
file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/${FOLDER}/Sfx/*.sl"
)
file(GLOB SHADER_DEFS
	"${SIMUL_PLATFORM_DIR}/${FOLDER}/Sfx/*.json"
)

source_group("NV" src FILES "**/*.cpp" "**/*.h" )
source_group("Shaders" FILES  ${SHADERS} )
source_group("Shader Includes" FILES ${SHADER_INCLUDES} )

set(DX12_DEFS SIMUL_DIRECTX12_DLL=1 
	PLATFORM_SUPPORT_D3D12_RAYTRACING=$<BOOL:${PLATFORM_SUPPORT_D3D12_RAYTRACING}> 
	PLATFORM_SUPPORT_D3D12_VIEWINSTANCING=$<BOOL:${PLATFORM_SUPPORT_D3D12_VIEWINSTANCING}> 
	PLATFORM_DEBUG_BARRIERS=$<BOOL:${PLATFORM_DEBUG_BARRIERS}> )

if(${PLATFORM_D3D12_ENABLE_PIX})
	set(DX12_DEFS ${DX12_DEFS} PLATFORM_ENABLE_PIX=1 USE_PIX)
else()
	set(DX12_DEFS ${DX12_DEFS} PLATFORM_ENABLE_PIX=0)
endif()

if(PLATFORM_WINDOWS AND NOT PLATFORM_WINGDK)
	set(DX12_DEFS ${DX12_DEFS} SIMUL_D3D12_VIDEO_SUPPORTED=1)
else()
	set(DX12_DEFS ${DX12_DEFS} SIMUL_D3D12_VIDEO_SUPPORTED=0)
	list(FILTER SOURCES EXCLUDE REGEX "Video*")
	list(FILTER HEADERS EXCLUDE REGEX "Video*")
endif()

# Add in D3D12MemAlloc.cpp/.h
set(HEADERS ${HEADERS} "${SIMUL_PLATFORM_DIR}/External/D3D12MemoryAllocator/include/D3D12MemAlloc.h")
set(SOURCES ${SOURCES} "${SIMUL_PLATFORM_DIR}/External/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp")

add_static_library( SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${SHADERS} ${SHADER_INCLUDES} ${SHADER_DEFS} DEFINITIONS ${DX12_DEFS} PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}
	INCLUDES "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" PUBLICINCLUDES "${SIMUL_PLATFORM_DIR}/DirectX12")
add_dependencies(SimulDirectX12${STATIC_LINK_SUFFIX} SimulCrossPlatform${STATIC_LINK_SUFFIX})

# For D3D12MA
if(SIMUL_SOURCE_BUILD)
	target_include_directories(SimulDirectX12${STATIC_LINK_SUFFIX} PUBLIC "${SIMUL_PLATFORM_DIR}/External/D3D12MemoryAllocator/include")
else()
	target_include_directories(SimulDirectX12${STATIC_LINK_SUFFIX} INTERFACE "${SIMUL_PLATFORM_DIR}/External/D3D12MemoryAllocator/include")
endif()

if (SIMUL_DLL_SDK_LIBRARY)
	AddDLLCompileDefinitions( SimulDirectX12${STATIC_LINK_SUFFIX} )
endif()

option( PLATFORM_SUPPORT_D3D12_RAYTRACING "" OFF)
if(SIMUL_SOURCE_BUILD)
	if(PLATFORM_SPECTRUM)
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING True)
	elseif(PLATFORM_XBOXONE)
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING False)
	else()
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING True)
	endif()

	set( JSON_FILE "${SIMUL_PLATFORM_DIR}/${FOLDER}/Sfx/${FOLDER}.json" )

	#Note: Due to a CMake bug, we can't include the in_RootSignature file in SOURCES, or it will be default-compiled as an hlsl shader instead of the custom command below.

	option(PLATFORM_DX12_LOAD_GFX "" OFF)
	if(PLATFORM_DX12_LOAD_GFX)
		if(GDK AND NOT PLATFORM_WINGDK)
			set(in_RootSignature "${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/GFX.hlsl")
		# ROOT SIG BLOB!
			set(out_RootSignatureFolder "${CMAKE_BINARY_DIR}/shaderbin/$PLATFORM_NAME")
		# Create the target directory for the cso file:
			file(MAKE_DIRECTORY ${out_RootSignatureFolder})
			add_custom_command(TARGET SimulDirectX12${STATIC_LINK_SUFFIX}
						COMMAND ${GDK_BIN_PATH}/dxc.exe /T rootsig_1_0 /E"GFXRS" /I"Sfx" /Fo"${out_RootSignatureFolder}/GFX.cso" /nologo ${in_RootSignature}
						MAIN_DEPENDENCY ${in_RootSignature}
						WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
						COMMENT "${GDK_BIN_PATH}/dxc.exe /T rootsig_1_0 /E\"GFXRS\" /I\"Sfx\" /Fo\"${out_RootSignatureFolder}/GFX.cso\" /nologo ${in_RootSignature}"
						)
		endif()
	endif()
endif()