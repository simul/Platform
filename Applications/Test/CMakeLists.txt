cmake_minimum_required(VERSION 3.5)

file(GLOB CMAKE "*.cmake" )

file(GLOB SOURCES Test.cpp  )
file(GLOB HEADERS "*.h" )
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")
 
source_group("Shaders" src FILES ${shaders} )
 
set(SAMPLE_USE_APIS SAMPLE_USE_D3D11=$<BOOL:${PLATFORM_SUPPORT_D3D11}> SAMPLE_USE_D3D12=$<BOOL:${PLATFORM_SUPPORT_D3D12}> SAMPLE_USE_OPENGL=$<BOOL:${PLATFORM_SUPPORT_OPENGL}> SAMPLE_USE_VULKAN=$<BOOL:${PLATFORM_SUPPORT_VULKAN}> )

add_static_executable( Test_MT WIN32 SOURCES ${SOURCES} ${HEADERS} ${shaders} DEFINITIONS ${SAMPLE_USE_APIS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
set_target_properties( Test_MT PROPERTIES FOLDER Static/Applications)
target_include_directories( Test_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_compile_definitions( Test_MT PRIVATE DO_PROFILING=0 )
set_target_properties( Test_MT PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
						VS_DEBUGGER_ENVIRONMENT "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}\n${VS_DEBUGGER_ENVIRONMENT}")
target_link_libraries( Test_MT SimulCrossPlatform_MT SimulMath_MT Core_MT directxtex_MT )

if(PLATFORM_USE_ASSIMP)
	target_link_directories( Test_MT PUBLIC ${SIMUL_PLATFORM_DIR}/External/assimp/build_mt/lib/${CMAKE_BUILD_TYPE})
	target_link_libraries( Test_MT ${ASSIMP_LIBNAME} )
endif()

if(PLATFORM_SUPPORT_D3D11)
	target_include_directories(Test_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
	target_link_libraries(Test_MT SimulDirectX11_MT )
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( Test_MT CoreShaders )
	endif()
endif() 

if(PLATFORM_SUPPORT_D3D12)
	target_link_libraries(Test_MT SimulDirectX12_MT)
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( Test_MT CoreShaders )
	endif()
endif()

if(PLATFORM_SUPPORT_VULKAN)
	target_include_directories(Test_MT PUBLIC "${VULKAN_SDK_DIR}/Include")
	target_link_libraries(Test_MT SimulVulkan_MT glfw)
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( Test_MT CoreShaders ) 
	endif()
endif()

if(PLATFORM_SUPPORT_OPENGL)
	target_link_libraries(Test_MT SimulOpenGL_MT glfw)
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( Test_MT CoreShaders ) 
	endif()
endif()

set( JSON_FILE_D3D12 "${SIMUL_PLATFORM_DIR}/DirectX12/Sfx/DirectX12.json" )
set( JSON_FILE_VULKAN "${SIMUL_PLATFORM_DIR}/Vulkan/Sfx/Vulkan.json" )

if(PLATFORM_BUILD_MD_LIBS)
	add_dynamic_executable(Test_MD WIN32 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} ${shaders} DEFINITIONS ${SAMPLE_USE_APIS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
	target_compile_definitions(Test_MD PRIVATE SIMUL_DYNAMIC_LINK=1 DO_PROFILING=0)
	target_include_directories(Test_MD PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
	target_include_directories(Test_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
	set_target_properties(Test_MD PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
								VS_DEBUGGER_ENVIRONMENT       "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}\n${VS_DEBUGGER_ENVIRONMENT}")
	target_link_libraries(Test_MD SimulCrossPlatform_MD SimulMath_MD Core_MD)

	add_sfx_shader_project ( Test_MD ${JSON_FILE_VULKAN} SOURCES ${shaders} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Shaders" "${SIMUL_PLATFORM_DIR}/Shaders/SL" "${SIMUL_PLATFORM_DIR}/Vulkan/Sfx" OPTIONS -w OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/shaderbin/Vulkan)
	add_sfx_shader_project ( Test_MD ${JSON_FILE_D3D12} SOURCES ${shaders} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Shaders" "${SIMUL_PLATFORM_DIR}/Shaders/SL" "${SIMUL_PLATFORM_DIR}/DirectX12/Sfx" OPTIONS -w OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/shaderbin/DirectX12)

	if(PLATFORM_SUPPORT_D3D11)
		target_link_libraries(Test_MD SimulDirectX11_MD)
		if(SIMUL_BUILD_SHADERS)
			add_dependencies( Test_MD CoreShaders )
		endif()
	endif()

	if(PLATFORM_SUPPORT_D3D12)
		target_link_libraries(Test_MD SimulDirectX12_MD)
		if(SIMUL_BUILD_SHADERS)
			add_dependencies( Test_MD CoreShaders )
		endif()
	endif()

	if(PLATFORM_SUPPORT_VULKAN)
		target_include_directories(Test_MD PUBLIC "${VULKAN_SDK_DIR}/Include")
		target_link_libraries(Test_MD SimulVulkan_MD)
		if(SIMUL_BUILD_SHADERS)
			add_dependencies( Test_MD CoreShaders )
		endif()
	endif()

	if(PLATFORM_SUPPORT_OPENGL)
		target_link_libraries(Test_MD SimulOpenGL_MD)
		if(SIMUL_BUILD_SHADERS)
			add_dependencies( Test_MD CoreShaders ) 
		endif()
	endif()
endif()