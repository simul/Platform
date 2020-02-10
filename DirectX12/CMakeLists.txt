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
    "${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/*.json"
)

file(GLOB SHADERS
	"${SIMUL_PLATFORM_DIR}/Shaders/Sfx/*.sfx" 
)

file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl"
)

set(in_RootSignature "${CMAKE_CURRENT_SOURCE_DIR}/HLSL/GFX.hlsl")

add_static_library( SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS SIMUL_DIRECTX12_DLL=1 PROPERTIES FOLDER Static
	INCLUDES "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" PUBLICINCLUDES "${SIMUL_PLATFORM_DIR}/DirectX12" )

add_dynamic_library(SimulDirectX12 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} )

if(SIMUL_SOURCE_BUILD)
	target_compile_definitions(SimulDirectX12_MD PRIVATE SIMUL_DIRECTX12_DLL=1)
	set_target_properties(SimulDirectX12_MD PROPERTIES FOLDER Dynamic
								LINK_FLAGS "/DELAYLOAD:d3dcompiler_47.dll")
	target_include_directories(SimulDirectX12_MD PUBLIC "${SIMUL_PLATFORM_DIR}/DirectX12")
	target_include_directories(SimulDirectX12_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/DirectX/DirectXTex/DirectXTex" )
	LibraryDefaults(SimulDirectX12_MD)

	if(XBOXONE)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/XboxOne/HLSL/HLSL12X.json" )
	elseif(GDK)
		set( JSON_FILE "${SIMUL_PLATFORM_DIR}/Platfm/XboxOne/HLSL/HLSL12X.json" )
	else()
		set( JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/HLSL/HLSL12.json" )
	endif()
	#message(STATUS JSON_FILE ${JSON_FILE} )
	add_sfx_shader_project( DirectX12CoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/HLSL" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${JSON} ${CMAKE} OPTIONS -w OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaderbin" -ESIMUL_XDK_VERSION=${XDK_TOOLCHAIN_VERSION} )
	#Note: Due to a CMake bug, we can't include the in_RootSignature file in SOURCES, or it will be default-compiled as an hlsl shader instead of the custom command below.

	# ROOT SIG BLOB!
	if(XBOXONE OR GDK)
	set(out_RootSignature "${CMAKE_CURRENT_BINARY_DIR}/../XboxOne/shaderbin/GFX.cso")
	else()
	set(out_RootSignature "${CMAKE_CURRENT_BINARY_DIR}/shaderbin/GFX.cso")
	endif()
	add_custom_command(TARGET DirectX12CoreShaders 
					COMMAND ${SIMUL_FX_EXECUTABLE} /T rootsig_1_0 /E"GFXRS" /I"HLSL" /Fo"${out_RootSignature}" /nologo ${in_RootSignature}
					MAIN_DEPENDENCY ${in_RootSignature}
					WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
					COMMENT "${SIMUL_FX_EXECUTABLE} /E\"GFXRS\" /I\"HLSL\" /Fo\"${out_RootSignature}\" /nologo ${in_RootSignature}"
					)


	if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	#add_dependencies(SimulDirectX12_MT DirectX12Shaders )
	#add_dependencies(DirectX12Shaders Sfx )
	endif()
	#add_dependencies(SimulDirectX12_MD DirectX12Shaders )

	target_link_libraries(SimulDirectX12_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD directxtex_MD)

endif()