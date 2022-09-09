cmake_minimum_required(VERSION 2.9)
option(PLATFORM_SUPPORT_VULKAN_MULTIVIEW "" OFF)
option(PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR "" OFF)

file(GLOB SOURCES "*.cpp" )
file(GLOB HEADERS "*.h" )

file(GLOB SHADERS
	"${SIMUL_PLATFORM_DIR}/Shaders/SFX/*.sfx"
)

file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl"
	"${SIMUL_PLATFORM_DIR}/Vulkan/Sfx/*.sl"
)

file(GLOB SHADER_DEFS
	"${SIMUL_PLATFORM_DIR}/Vulkan/Sfx/*.json" 
)

set(VK_DEFS SIMUL_VULKAN_DLL=1 VK_PROTOTYPES)
set(VK_DEFS ${VK_DEFS} PLATFORM_SUPPORT_VULKAN_MULTIVIEW=$<BOOL:${PLATFORM_SUPPORT_VULKAN_MULTIVIEW}>)
set(VK_DEFS ${VK_DEFS} PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR=$<BOOL:${PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR}>)

add_static_library( SimulVulkan SOURCES ${SOURCES} ${HEADERS} DEFINITIONS ${VK_DEFS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

if(SIMUL_SOURCE_BUILD)
target_link_directories(SimulVulkan_MT PUBLIC ${VULKAN_SDK_DIR}/Lib)

if(PLATFORM_WINDOWS)
	target_compile_definitions(SimulVulkan_MT PRIVATE  VK_USE_PLATFORM_WIN32_KHR  )
endif()

target_include_directories(SimulVulkan_MT PUBLIC "${VULKAN_SDK_DIR}/Include")
target_include_directories(SimulVulkan_MT PRIVATE "${SIMUL_PLATFORM_DIR}/External/stb")
target_include_directories(SimulVulkan_MT PUBLIC "${SIMUL_PLATFORM_DIR}/Vulkan")
endif()

add_dynamic_library(SimulVulkan SOURCES ${SOURCES} ${HEADERS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} )
 
if(SIMUL_SOURCE_BUILD)
target_link_directories(SimulVulkan_MD PUBLIC ${VULKAN_SDK_DIR}/Lib)
target_compile_definitions(SimulVulkan_MD PRIVATE SIMUL_DYNAMIC_LINK=1 ${VK_DEFS} )

if(PLATFORM_WINDOWS)
	target_compile_definitions(SimulVulkan_MD PRIVATE  VK_USE_PLATFORM_WIN32_KHR  )
endif()

target_include_directories(SimulVulkan_MD PUBLIC "${VULKAN_SDK_DIR}/Include")
target_include_directories(SimulVulkan_MD PRIVATE "${SIMUL_PLATFORM_DIR}/External/stb")
target_include_directories(SimulVulkan_MD PUBLIC "${SIMUL_PLATFORM_DIR}/Vulkan")

add_sfx_shader_project( VulkanCoreShaders "${CMAKE_CURRENT_SOURCE_DIR}/Sfx/Vulkan.json" INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Sfx" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${SHADER_DEFS} OPTIONS -w DEFINES VULKAN_SDK=${VULKAN_SDK_DIR} OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaderbin" FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders)

if(SIMUL_BUILD_SHADERS)
	#add_dependencies(SimulVulkan_MT VulkanShaders)
	#add_dependencies(SimulVulkan_MD VulkanShaders)
endif()

target_link_libraries(SimulVulkan_MD SimulCrossPlatform_MD SimulMath_MD Core_MD )

if(PLATFORM_WINDOWS)
target_link_libraries(SimulVulkan_MD  vulkan-1)
else()
	target_link_directories( SimulVulkan_MD PRIVATE ${VULKAN_SDK_DIR}/x86_64/lib/ /usr/lib/x86_64-linux-gnu )
	target_link_libraries(SimulVulkan_MD libvulkan.so)
endif()
endif()
