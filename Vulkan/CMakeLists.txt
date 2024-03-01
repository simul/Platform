cmake_minimum_required(VERSION 3.5)
option(PLATFORM_SUPPORT_VULKAN_MULTIVIEW "" OFF)
option(PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR "" OFF)
option(PLATFORM_VULKAN_ENABLE_DEBUG_UTILS_MARKERS "" OFF)

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
	"${CMAKE_CURRENT_SOURCE_DIR}/Sfx/*.json" 
)

set(VK_DEFS SIMUL_VULKAN_DLL=1 VK_PROTOTYPES)
set(VK_DEFS ${VK_DEFS} PLATFORM_SUPPORT_VULKAN_MULTIVIEW=$<BOOL:${PLATFORM_SUPPORT_VULKAN_MULTIVIEW}>)
set(VK_DEFS ${VK_DEFS} PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR=$<BOOL:${PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR}>)
set(VK_DEFS ${VK_DEFS} PLATFORM_VULKAN_ENABLE_DEBUG_UTILS_MARKERS=$<BOOL:${PLATFORM_VULKAN_ENABLE_DEBUG_UTILS_MARKERS}>)

# Add in vk_mem_alloc.h
set(HEADERS ${HEADERS} "${SIMUL_PLATFORM_DIR}/External/VulkanMemoryAllocator/include/vk_mem_alloc.h")

if(SIMUL_SOURCE_BUILD)
	add_static_library( SimulVulkan SOURCES ${SOURCES} ${HEADERS} ${SHADER_DEFS} ${SHADER_INCLUDES} DEFINITIONS ${VK_DEFS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

	if(PLATFORM_WINDOWS)
		target_compile_definitions(SimulVulkan${STATIC_LINK_SUFFIX} PRIVATE  VK_USE_PLATFORM_WIN32_KHR  )
	endif()
	# Support for X11
	if(PLATFORM_LINUX)
		target_compile_definitions(SimulVulkan${STATIC_LINK_SUFFIX} PRIVATE  VK_USE_PLATFORM_XCB_KHR  )
	endif()
	
	target_include_directories(SimulVulkan${STATIC_LINK_SUFFIX} PUBLIC "${Vulkan_INCLUDE_DIR}")
	target_include_directories(SimulVulkan${STATIC_LINK_SUFFIX} PRIVATE "${SIMUL_PLATFORM_DIR}/External/stb")
	# For parallel_hashmap
	target_include_directories(SimulVulkan${STATIC_LINK_SUFFIX} PUBLIC "${SIMUL_PLATFORM_DIR}/External")
	target_include_directories(SimulVulkan${STATIC_LINK_SUFFIX} PUBLIC "${SIMUL_PLATFORM_DIR}/Vulkan")
	# For VMA
	target_include_directories(SimulVulkan${STATIC_LINK_SUFFIX} PUBLIC "${SIMUL_PLATFORM_DIR}/External/VulkanMemoryAllocator/include")

endif()
