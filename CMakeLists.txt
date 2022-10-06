cmake_minimum_required(VERSION 3.14)

if(SIMUL_PLATFORM_DIR)
	set(SIMUL_PLATFORM_FOLDER_PREFIX Platform)
else()
	project(Platform)
	set(SIMUL_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

set(PLATFORM_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PLATFORM_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(SIMUL_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})  
# copy to scope above:
set(SIMUL_PLATFORM_DIR ${SIMUL_PLATFORM_DIR} PARENT_SCOPE)
set(PLATFORM_SOURCE_DIR ${PLATFORM_SOURCE_DIR} PARENT_SCOPE)
set(PLATFORM_BUILD_DIR ${PLATFORM_BUILD_DIR} PARENT_SCOPE)


if(PLATFORM_USE_ASSIMP)
	set( ASSIMP_LIBNAME assimp-vc142-mt$<$<CONFIG:Debug>:d> )
	if(NOT PLATFORM_WINDOWS )
		add_subdirectory(External/assimp)
	endif()
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

if(PLATFORM_IMGUI)
	add_subdirectory(Imgui)
	add_subdirectory(Applications/GuiSample)
endif()

set(FMT_TEST false)  
set(FMT_DOC false) 
set(FMT_INSTALL false) 
add_subdirectory(External/fmt)
if(PLATFORM_WINDOWS)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	target_compile_options(fmt PRIVATE "$<$<CONFIG:RELEASE>:/MT>")
	target_compile_options(fmt PRIVATE "$<$<CONFIG:DEBUG>:/MTd>")
endif()