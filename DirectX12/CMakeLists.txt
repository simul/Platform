cmake_minimum_required(VERSION 2.9)
option(SIMUL_D3D12_ENABLE_PIX "" OFF)
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

if(${SIMUL_D3D12_ENABLE_PIX})
	set(DX12_DEFS ${DX12_DEFS} SIMUL_ENABLE_PIX=1 USE_PIX)
else()
	set(DX12_DEFS ${DX12_DEFS} SIMUL_ENABLE_PIX=0)
endif()

if(PLATFORM_WINDOWS AND NOT PLATFORM_WINGDK)
	set(DX12_DEFS ${DX12_DEFS} SIMUL_D3D12_VIDEO_SUPPORTED=1)
else()
	set(DX12_DEFS ${DX12_DEFS} SIMUL_D3D12_VIDEO_SUPPORTED=0)
	list(FILTER SOURCES EXCLUDE REGEX "Video*")
	list(FILTER HEADERS EXCLUDE REGEX "Video*")
endif()

add_static_library( SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS ${DX12_DEFS} PROPERTIES FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}
	INCLUDES "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" PUBLICINCLUDES "${SIMUL_PLATFORM_DIR}/DirectX12" )
	
if(PLATFORM_BUILD_MD_LIBS)
	add_dynamic_library(SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS ${DX12_DEFS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
 endif()

option( PLATFORM_SUPPORT_D3D12_RAYTRACING "" OFF)
if(SIMUL_SOURCE_BUILD)
	if(PLATFORM_BUILD_MD_LIBS)
		target_compile_definitions(SimulDirectX12_MD PRIVATE ${DX12_DEFS})
		set_target_properties(SimulDirectX12_MD PROPERTIES LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
		target_include_directories(SimulDirectX12_MD PUBLIC "${SIMUL_PLATFORM_DIR}/DirectX12")
		target_include_directories(SimulDirectX12_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
		target_include_directories(SimulDirectX12_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/assimp/build_md/include" )
		LibraryDefaults(SimulDirectX12_MD)
	endif()
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
	add_sfx_shader_project( DirectX12CoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Sfx" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${JSON} ${CMAKE} OPTIONS -w OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaderbin" DEFINES SIMUL_XDK_VERSION=${XDK_TOOLCHAIN_VERSION} GDK_BIN_PATH=${GDK_BIN_PATH} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders )
	#Note: Due to a CMake bug, we can't include the in_RootSignature file in SOURCES, or it will be default-compiled as an hlsl shader instead of the custom command below.

	option(PLATFORM_DX12_LOAD_GFX "" OFF)
	if(PLATFORM_DX12_LOAD_GFX)
		if(GDK AND NOT PLATFORM_WINGDK)
			set(in_RootSignature "${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/GFX.hlsl")
		# ROOT SIG BLOB!
			set(out_RootSignatureFolder "${CMAKE_CURRENT_BINARY_DIR}/shaderbin")
		# Create the target directory for the cso file:
			file(MAKE_DIRECTORY ${out_RootSignatureFolder})
			add_custom_command(TARGET DirectX12CoreShaders
						COMMAND ${GDK_BIN_PATH}/dxc.exe /T rootsig_1_0 /E"GFXRS" /I"Sfx" /Fo"${out_RootSignatureFolder}/GFX.cso" /nologo ${in_RootSignature}
						MAIN_DEPENDENCY ${in_RootSignature}
						WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
						COMMENT "${GDK_BIN_PATH}/dxc.exe /T rootsig_1_0 /E\"GFXRS\" /I\"Sfx\" /Fo\"${out_RootSignatureFolder}/GFX.cso\" /nologo ${in_RootSignature}"
						)
		endif()
	endif()
	if(PLATFORM_WINDOWS)
		add_dependencies(SimulDirectX12_MT DirectX12CoreShaders )
		#add_dependencies(DirectX12Shaders Sfx )
	endif()
	if(PLATFORM_BUILD_MD_LIBS)
		add_dependencies(SimulDirectX12_MD DirectX12CoreShaders )

		target_link_libraries(SimulDirectX12_MD SimulCrossPlatform_MD SimulMath_MD Core_MD directxtex_MD)
	endif()

endif()