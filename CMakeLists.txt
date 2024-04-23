cmake_minimum_required(VERSION 3.14)

if(SIMUL_PLATFORM_DIR)
	set(PLATFORM_SOURCE_DIR ${SIMUL_PLATFORM_DIR})
	set(PLATFORM_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
	set(SIMUL_PLATFORM_FOLDER_PREFIX Platform)
else()
project(Platform)
	set(SIMUL_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR})
	set(PLATFORM_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
	set(PLATFORM_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

set(SIMUL_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})  

if(PLATFORM_USE_FMT)
	add_subdirectory(External/fmt)
endif()

if(PLATFORM_USE_ASSIMP)
	set(ASSIMP_LIBNAME assimp-vc${MSVC_TOOLSET_VERSION}-mt$<$<CONFIG:Debug>:d>)
	set(ASSIMP_INSTALL OFF CACHE INTERNAL  "")
	set(ASSIMP_INSTALL_PDB OFF CACHE INTERNAL  "")
	set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL  "")
	set(ASSIMP_BUILD_ZLIB ON CACHE INTERNAL  "")
	SET ( BUILD_SHARED_LIBS ON CACHE BOOL "BUILD_SHARED_LIB" FORCE ) # We want a DLL here.
	add_subdirectory(External/assimp)
	SET ( BUILD_SHARED_LIBS OFF CACHE BOOL "BUILD_SHARED_LIB" FORCE ) # Restore off state.

	set_target_properties(assimp PROPERTIES FOLDER "Platform/assimp")
	set_target_properties(zlibstatic PROPERTIES FOLDER "Platform/assimp")
	set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER "Platform/assimp")

	function(DelayLoadLinkAssimp target)
		target_link_libraries(${target} PRIVATE assimp)
		target_link_options(${target} PRIVATE "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:assimp>.dll")
	endfunction()
endif()

include("CMake/Variables.cmake")
include("CMake/Include.cmake")
include("CMake/Shader.cmake")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake" ${CMAKE_MODULE_PATH})

add_subdirectory(Core)
add_subdirectory(Math)
add_subdirectory(CrossPlatform)
add_subdirectory(Applications/Sfx)

if(PLATFORM_SUPPORT_GLES)
	add_subdirectory(GLES)
endif()

if(PLATFORM_BUILD_SAMPLES)
	#add_subdirectory(Examples)
endif()

if(SIMUL_BUILD_SAMPLES AND ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten" )
	add_subdirectory(Applications/EmscriptenSample)
endif()

if(PLATFORM_SUPPORT_VULKAN)
	if(SIMUL_BUILD_SAMPLES AND PLATFORM_LINUX)
		add_subdirectory(External/glfw)
	endif()
	if(PLATFORM_WINDOWS OR PLATFORM_LINUX)
		add_subdirectory(Vulkan)
	endif()
endif()
 
if(PLATFORM_WINDOWS)
	if(SIMUL_SOURCE_BUILD)
		add_subdirectory(Applications/Sample)
		add_subdirectory(Applications/Test)
	endif()
	if( PLATFORM_SUPPORT_OPENGL )
		add_subdirectory(OpenGL)
	endif()
	if( PLATFORM_SUPPORT_D3D11 )
		add_subdirectory(DirectX11)
	endif()
endif()

if( PLATFORM_SUPPORT_ANGLE )
	add_subdirectory(GLES)
endif()

if( PLATFORM_EMSCRIPTEN )
	add_subdirectory(Applications/EmscriptenSample)
endif()

if(XBOXONE AND PLATFORM_SUPPORT_D3D11)
	add_subdirectory(DirectX11)
endif()

if(PLATFORM_WINDOWS AND PLATFORM_SUPPORT_D3D12)
	add_subdirectory(DirectX12)
endif()

if(XBOXONE AND PLATFORM_SUPPORT_D3D12)
	add_subdirectory(DirectX12)
endif()

if(XBOXONE)
	add_subdirectory(External/DirectX/DirectXTex/DirectXTex)
	set( ENV{SIMUL_XDK_VERSION} ${REQUIRED_XB1_TOOLCHAIN_VERSION})
endif()

if(GDK)
	add_subdirectory(DirectX12)
	add_subdirectory(External/DirectX/DirectXTex/DirectXTex)
	set( ENV{SIMUL_XDK_VERSION} ${REQUIRED_XB1_TOOLCHAIN_VERSION})
endif()

if(PLATFORM_WINGDK)
	add_subdirectory(DirectX11)
endif()

if(PLATFORM_WINDOWS)
	add_subdirectory(External/DirectX/DirectXTex/DirectXTex)
endif()

if(PLATFORM_BUILD_DOCS)
	add_subdirectory(Docs)
endif()

if(PLATFORM_PS4)
	add_subdirectory(PS4/Render)
endif()

if(PLATFORM_COMMODORE)
	add_subdirectory(Commodore)
endif()

if(PLATFORM_SWITCH)
	add_subdirectory(Switch)
endif()

if(PLATFORM_IMGUI)
	add_subdirectory(Imgui)
	add_subdirectory(Applications/GuiSample)
endif()