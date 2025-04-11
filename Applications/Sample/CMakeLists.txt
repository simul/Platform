
cmake_minimum_required(VERSION 3.12)

file(GLOB CMAKE "*.cmake" )

file(GLOB SOURCES Sample.cpp  )
file(GLOB HEADERS "*.h" )
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")

#set(SAMPLE_API SAMPLE_USE_GLES)  
set(SAMPLE_API SAMPLE_USE_D3D12)  

source_group("Shaders" src FILES ${shaders} )

add_static_executable(Sample${STATIC_LINK_SUFFIX} WIN32 SOURCES ${SOURCES} ${HEADERS} ${shaders} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
target_include_directories(Sample${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_compile_definitions(Sample${STATIC_LINK_SUFFIX} PRIVATE DO_PROFILING=0 )
set_target_properties(Sample${STATIC_LINK_SUFFIX} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
						VS_DEBUGGER_ENVIRONMENT "PATH=${PLATFORM_ANGLE_LIB_DIR};${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}\n${VS_DEBUGGER_ENVIRONMENT}")
target_link_libraries(Sample${STATIC_LINK_SUFFIX} PRIVATE SimulCrossPlatform${STATIC_LINK_SUFFIX} SimulMath${STATIC_LINK_SUFFIX} Core${STATIC_LINK_SUFFIX} directxtex${STATIC_LINK_SUFFIX} fmt::fmt-header-only)
if(PLATFORM_USE_ASSIMP)
	DelayLoadLinkAssimp(Sample${STATIC_LINK_SUFFIX})
endif()

add_multiplatform_sfx_shader_project (Sample${STATIC_LINK_SUFFIX} SOURCES ${shaders} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"  OPTIONS OUTPUT "${CMAKE_BINARY_DIR}/shaderbin/$PLATFORM_NAME")

if(PLATFORM_SUPPORT_ANGLE)
	if(${SAMPLE_API} MATCHES "SAMPLE_USE_GLES")
		target_include_directories(Sample${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/Platform/GLES")
		target_link_libraries(Sample${STATIC_LINK_SUFFIX} PRIVATE SimulGLES${STATIC_LINK_SUFFIX} libGLESv2.dll.lib libEGL.dll.lib)
	endif() 
endif() 
if(PLATFORM_SUPPORT_D3D11)
	if(${SAMPLE_API} MATCHES "SAMPLE_USE_D3D11")
		target_include_directories(Sample${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
		target_link_libraries(Sample${STATIC_LINK_SUFFIX} PRIVATE SimulDirectX11${STATIC_LINK_SUFFIX} )
	endif() 
endif() 

if(PLATFORM_SUPPORT_D3D12)
	if(${SAMPLE_API} MATCHES "SAMPLE_USE_D3D12")
		target_include_directories(Sample${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX12")
		target_link_libraries(Sample${STATIC_LINK_SUFFIX} PRIVATE SimulDirectX12${STATIC_LINK_SUFFIX})
	endif()
endif()

if(PLATFORM_SUPPORT_VULKAN)
	if(${SAMPLE_API} MATCHES "SAMPLE_USE_VULKAN")
		target_include_directories(Sample${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/Platform/External/VulkanMemoryAllocator/include")
		target_link_libraries(Sample${STATIC_LINK_SUFFIX} PRIVATE SimulVulkan${STATIC_LINK_SUFFIX})
	endif()
endif()
