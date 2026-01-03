cmake_minimum_required(VERSION 3.12)

file(GLOB CMAKE "*.cmake" )

file(GLOB SOURCES Test.cpp  )
file(GLOB HEADERS "*.h" )
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")
 
source_group("Shaders" src FILES ${shaders} )
 
set(SAMPLE_USE_APIS SAMPLE_USE_D3D11=$<BOOL:${PLATFORM_SUPPORT_D3D11}> SAMPLE_USE_D3D12=$<BOOL:${PLATFORM_SUPPORT_D3D12}> SAMPLE_USE_OPENGL=$<BOOL:${PLATFORM_SUPPORT_OPENGL}> SAMPLE_USE_VULKAN=$<BOOL:${PLATFORM_SUPPORT_VULKAN}> )

add_static_executable( Test${STATIC_LINK_SUFFIX} WIN32 SOURCES ${SOURCES} ${HEADERS} ${shaders} DEFINITIONS ${SAMPLE_USE_APIS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
target_include_directories( Test${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_compile_definitions( Test${STATIC_LINK_SUFFIX} PRIVATE DO_PROFILING=0 )
set_target_properties( Test${STATIC_LINK_SUFFIX} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
						VS_DEBUGGER_ENVIRONMENT "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}\n${VS_DEBUGGER_ENVIRONMENT}")
target_link_libraries( Test${STATIC_LINK_SUFFIX} PRIVATE SimulCrossPlatform${STATIC_LINK_SUFFIX} SimulMath${STATIC_LINK_SUFFIX} Core${STATIC_LINK_SUFFIX} directxtex${STATIC_LINK_SUFFIX} fmt::fmt-header-only )

if(PLATFORM_USE_ASSIMP)
	DelayLoadLinkAssimp(Test${STATIC_LINK_SUFFIX})
endif()

if(PLATFORM_SUPPORT_D3D11)
	target_include_directories(Test${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
	target_link_libraries(Test${STATIC_LINK_SUFFIX} PRIVATE SimulDirectX11${STATIC_LINK_SUFFIX} )
endif() 

if(PLATFORM_SUPPORT_D3D12)
	target_include_directories(Test${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX12")
	target_link_libraries(Test${STATIC_LINK_SUFFIX} PRIVATE SimulDirectX12${STATIC_LINK_SUFFIX})
endif()

if(PLATFORM_SUPPORT_VULKAN)
	target_include_directories(Test${STATIC_LINK_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}/Platform/External/VulkanMemoryAllocator/include")
	target_include_directories(Test${STATIC_LINK_SUFFIX} PUBLIC "${Vulkan_INCLUDE_DIR}")
	target_link_libraries(Test${STATIC_LINK_SUFFIX} PRIVATE SimulVulkan${STATIC_LINK_SUFFIX} glfw ${Vulkan_LIBRARY})
endif()

if(PLATFORM_SUPPORT_OPENGL)
	target_link_libraries(Test${STATIC_LINK_SUFFIX} PRIVATE SimulOpenGL${STATIC_LINK_SUFFIX} glfw)
endif()
