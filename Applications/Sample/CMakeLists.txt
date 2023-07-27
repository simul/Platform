
cmake_minimum_required(VERSION 3.5)

file(GLOB CMAKE "*.cmake" )

file(GLOB SOURCES Sample.cpp  )
file(GLOB HEADERS "*.h" )
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")

#set(SAMPLE_API SAMPLE_USE_GLES)  
set(SAMPLE_API SAMPLE_USE_D3D12)  

source_group("Shaders" src FILES ${shaders} )
 
add_static_executable(Sample_MT WIN32 SOURCES ${SOURCES} ${HEADERS} ${shaders} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
set_target_properties(Sample_MT PROPERTIES FOLDER Static/Applications)
target_link_directories(Sample_MT PUBLIC ${CMAKE_SOURCE_DIR}/Platform/External/fmt/build_mt/lib/${CMAKE_BUILD_TYPE})
target_include_directories(Sample_MT PUBLIC "${SIMUL_PLATFORM_DIR}/External/fmt/include" )
target_include_directories(Sample_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_compile_definitions(Sample_MT PRIVATE DO_PROFILING=0 )
set_target_properties(Sample_MT PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
						VS_DEBUGGER_ENVIRONMENT "PATH=${PLATFORM_ANGLE_LIB_DIR};${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}\n${VS_DEBUGGER_ENVIRONMENT}")
target_link_libraries(Sample_MT SimulCrossPlatform_MT SimulMath_MT Core_MT directxtex_MT )
if(PLATFORM_USE_ASSIMP)
	target_link_directories( Sample_MT PUBLIC ${SIMUL_PLATFORM_DIR}/External/assimp/build_mt/lib/${CMAKE_BUILD_TYPE})
	target_link_libraries( Sample_MT ${ASSIMP_LIBNAME} )
endif()

add_multiplatform_sfx_shader_project (Sample_MT SOURCES ${shaders} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"  OPTIONS -w OUTPUT "${CMAKE_BINARY_DIR}/shaderbin/$PLATFORM_NAME")

if(PLATFORM_SUPPORT_ANGLE)
	if(${SAMPLE_API} MATCHES "SAMPLE_USE_GLES")
		target_include_directories(Sample_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/GLES")
		target_link_libraries(Sample_MT SimulGLES_MT libGLESv2.dll.lib libEGL.dll.lib)
	endif() 
endif() 
if(PLATFORM_SUPPORT_D3D11)
	if(${SAMPLE_API} MATCHES "SAMPLE_USE_D3D11")
		target_include_directories(Sample_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
		target_link_libraries(Sample_MT SimulDirectX11_MT )
	endif() 
endif() 

if(PLATFORM_SUPPORT_D3D12)
	if(${SAMPLE_API} MATCHES "SAMPLE_USE_D3D12")
		target_link_libraries(Sample_MT SimulDirectX12_MT)
	endif()
endif()

if(PLATFORM_SUPPORT_VULKAN)
	if(${SAMPLE_API} MATCHES "SAMPLE_USE_VULKAN")
		target_link_libraries(Sample_MT SimulVulkan_MT)
	endif()
endif()
