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
endif() 

if(PLATFORM_SUPPORT_D3D12)
	target_link_libraries(Test_MT SimulDirectX12_MT)
endif()

if(PLATFORM_SUPPORT_VULKAN)
	target_include_directories(Test_MT PUBLIC "${Vulkan_INCLUDE_DIR}")
	target_link_libraries(Test_MT SimulVulkan_MT glfw)
endif()

if(PLATFORM_SUPPORT_OPENGL)
	target_link_libraries(Test_MT SimulOpenGL_MT glfw)
endif()
