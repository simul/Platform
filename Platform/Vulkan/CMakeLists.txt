cmake_minimum_required(VERSION 2.8)

file(GLOB CMAKE
    "*.cmake"
)

file(GLOB SOURCES
	"*.cpp"
)

file(GLOB HEADERS
    "*.h"
)
link_directories(${VULKAN_SDK_DIR}/Lib)
add_library(SimulVulkan_MT ${SOURCES} ${HEADERS} ${CMAKE})
target_compile_definitions(SimulVulkan_MT PRIVATE SIMUL_VULKAN_DLL=1 VK_USE_PLATFORM_WIN32_KHR VK_PROTOTYPES )
set_target_properties(SimulVulkan_MT PROPERTIES FOLDER Static)
target_include_directories(SimulVulkan_MT PRIVATE "${VULKAN_SDK_DIR}/Include")
target_include_directories(SimulVulkan_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/stb")
target_include_directories(SimulVulkan_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/Vulkan")

add_library(SimulVulkan_MD SHARED ${SOURCES} ${HEADERS} ${CMAKE})
target_compile_definitions(SimulVulkan_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_VULKAN_DLL=1 VK_USE_PLATFORM_WIN32_KHR VK_PROTOTYPES )
set_target_properties(SimulVulkan_MD PROPERTIES FOLDER Dynamic)
target_include_directories(SimulVulkan_MD PRIVATE "${VULKAN_SDK_DIR}/Include")
target_include_directories(SimulVulkan_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/stb")
target_include_directories(SimulVulkan_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/Vulkan")

target_link_libraries(SimulVulkan_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

