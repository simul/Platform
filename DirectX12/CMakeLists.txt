cmake_minimum_required(VERSION 3.5)
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

file(GLOB SHADER_DEFS
	"${CMAKE_CURRENT_SOURCE_DIR}/Sfx/*.json" 
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

# Get Vulkan Memory Allocator library into [PLATFORM_BINARY]/d3d12ma
FetchContent_Declare(
	d3d12ma
	GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git
	GIT_TAG v2.0.1
	SOURCE_DIR d3d12ma
)
FetchContent_MakeAvailable(d3d12ma)

# Add in D3D12MemAlloc.cpp
set(SOURCES ${SOURCES} "${d3d12ma_SOURCE_DIR}/src/D3D12MemAlloc.cpp")

add_static_library( SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${SHADER_DEFS} DEFINITIONS ${DX12_DEFS} PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}
	INCLUDES "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" PUBLICINCLUDES "${SIMUL_PLATFORM_DIR}/DirectX12" DEPENDENCIES SimulCrossPlatform${STATIC_LINK_SUFFIX})

# For D3D12MA
target_include_directories(SimulDirectX12${STATIC_LINK_SUFFIX} PRIVATE "${d3d12ma_SOURCE_DIR}/include")

option( PLATFORM_SUPPORT_D3D12_RAYTRACING "" OFF)
if(SIMUL_SOURCE_BUILD)
	if(PLATFORM_SPECTRUM)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/Spectrum/Sfx/Spectrum.json" )
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING True)
	elseif(PLATFORM_XBOXONE)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/Sfx/XboxOne.json" )
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING False)
	else()
		set( JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Sfx/DirectX12.json" )
		set( PLATFORM_SUPPORT_D3D12_RAYTRACING True)
	endif()

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