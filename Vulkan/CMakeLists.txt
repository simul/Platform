cmake_minimum_required(VERSION 3.5)
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
	"${CMAKE_CURRENT_SOURCE_DIR}/Sfx/*.json" 
)

set(VK_DEFS SIMUL_VULKAN_DLL=1 VK_PROTOTYPES)
set(VK_DEFS ${VK_DEFS} PLATFORM_SUPPORT_VULKAN_MULTIVIEW=$<BOOL:${PLATFORM_SUPPORT_VULKAN_MULTIVIEW}>)
set(VK_DEFS ${VK_DEFS} PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR=$<BOOL:${PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR}>)

if(SIMUL_SOURCE_BUILD)
	add_static_library( SimulVulkan SOURCES ${SOURCES} ${HEADERS} ${SHADER_DEFS} DEFINITIONS ${VK_DEFS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
	#target_link_directories(SimulVulkan_MT PUBLIC ${VULKAN_SDK_DIR}/Lib)

	if(PLATFORM_WINDOWS)
		target_compile_definitions(SimulVulkan_MT PRIVATE  VK_USE_PLATFORM_WIN32_KHR  )
	endif()
	# Support for X11
	if(PLATFORM_LINUX)
		target_compile_definitions(SimulVulkan_MT PRIVATE  VK_USE_PLATFORM_XCB_KHR  )
	endif()
	
	target_include_directories(SimulVulkan_MT PUBLIC "${Vulkan_INCLUDE_DIR}")
	target_include_directories(SimulVulkan_MT PRIVATE "${SIMUL_PLATFORM_DIR}/External/stb")
	target_include_directories(SimulVulkan_MT PUBLIC "${SIMUL_PLATFORM_DIR}/Vulkan")


endif()
