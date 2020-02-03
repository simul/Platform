cmake_minimum_required(VERSION 2.8)

file(GLOB CMAKE "*.cmake" )
file(GLOB SOURCES "*.cpp" )
file(GLOB HEADERS "*.h" )

file(GLOB SHADERS
	"${CMAKE_SOURCE_DIR}/Platform/Shaders/SFX/*.sfx"
)

file(GLOB SHADER_INCLUDES
	"${CMAKE_SOURCE_DIR}/Platform/Shaders/SL/*.sl"
)

file(GLOB SHADER_DEFS
	"${CMAKE_SOURCE_DIR}/Platform/Vulkan/GLSL/*.*" 
)

add_static_library( SimulVulkan SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS SIMUL_VULKAN_DLL=1  VK_PROTOTYPES )

if(SIMUL_SOURCE_BUILD)
target_link_directories(SimulVulkan_MT PUBLIC ${VULKAN_SDK_DIR}/Lib)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_compile_definitions(SimulVulkan_MT PRIVATE  VK_USE_PLATFORM_WIN32_KHR  )
endif()

target_include_directories(SimulVulkan_MT PRIVATE "${VULKAN_SDK_DIR}/Include")
target_include_directories(SimulVulkan_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/External/stb")
target_include_directories(SimulVulkan_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/Vulkan")
endif()

add_dynamic_library(SimulVulkan SOURCES ${SOURCES} ${HEADERS} ${CMAKE})

if(SIMUL_SOURCE_BUILD)
target_link_directories(SimulVulkan_MD PUBLIC ${VULKAN_SDK_DIR}/Lib)
target_compile_definitions(SimulVulkan_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_VULKAN_DLL=1  VK_PROTOTYPES )

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_compile_definitions(SimulVulkan_MD PRIVATE  VK_USE_PLATFORM_WIN32_KHR  )
endif()

set_target_properties(SimulVulkan_MD PROPERTIES FOLDER Dynamic)
target_include_directories(SimulVulkan_MD PRIVATE "${VULKAN_SDK_DIR}/Include")
target_include_directories(SimulVulkan_MD PRIVATE "${CMAKE_SOURCE_DIR}/Platform/External/stb")
target_include_directories(SimulVulkan_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/Vulkan")

add_sfx_shader_project( VulkanCoreShaders "${CMAKE_CURRENT_SOURCE_DIR}/GLSL/GLSL.json" INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/GLSL" "${CMAKE_SOURCE_DIR}/Platform/Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${CMAKE} ${SHADER_DEFS} OPTIONS -w OUTPUT "${CMAKE_BINARY_DIR}/Platform/Vulkan/shaderbin")

if(SIMUL_BUILD_SHADERS)
	#add_dependencies(SimulVulkan_MT VulkanShaders)
	#add_dependencies(SimulVulkan_MD VulkanShaders)
endif()

target_link_libraries(SimulVulkan_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD )

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
target_link_libraries(SimulVulkan_MD  vulkan-1)
else()
	target_link_directories( SimulVulkan_MD PRIVATE ${VULKAN_SDK_DIR}/x86_64/lib/ /usr/lib/x86_64-linux-gnu )
	target_link_libraries(SimulVulkan_MD libvulkan.so)
endif()
endif()
