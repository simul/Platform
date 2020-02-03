cmake_minimum_required(VERSION 3.14)
project(Platform)

include("CMake/Variables.cmake")
include("CMake/Include.cmake")
include("CMake/Shader.cmake")

add_subdirectory(CrossPlatform)
add_subdirectory(Applications/Sfx)

if(SIMUL_BUILD_SAMPLES)
	if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" )
		add_subdirectory(External/glfw)
	endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_subdirectory(Vulkan)
endif()
 
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	add_subdirectory(OpenGL)
	add_subdirectory(DirectX11)
endif()

if(XBOXONE)
	add_subdirectory(DirectX12)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" )
	add_subdirectory(DirectX12)
endif()

if(XBOXONE)
	add_subdirectory(External/DirectX/DirectXTex/DirectXTex)
	set( ENV{SIMUL_XDK_VERSION}  ${REQUIRED_XB1_TOOLCHAIN_VERSION})
endif()

if(GDK)
	add_subdirectory(Platform/DirectX12)
	add_subdirectory(External/DirectX/DirectXTex/DirectXTex)
	add_subdirectory(External/DirectX/DirectXTK12)
	set( ENV{SIMUL_XDK_VERSION}  ${REQUIRED_XB1_TOOLCHAIN_VERSION})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" )
	add_subdirectory(External/DirectX/DirectXTex/DirectXTex)
	add_subdirectory(External/DirectX/Effects11_Win8SDK)
endif()

if(SIMUL_SUPPORT_PS4)
	add_subdirectory(PS4/Render)
endif()