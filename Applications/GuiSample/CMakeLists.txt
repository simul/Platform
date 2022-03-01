cmake_minimum_required(VERSION 2.9)

file(GLOB SOURCES GuiSample.cpp  )
file(GLOB HEADERS "*.h" )
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")

source_group("Shaders" src FILES ${shaders} )
 
add_static_executable(GuiSample_MT WIN32 SOURCES ${SOURCES} ${HEADERS} ${shaders} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} INCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui)
target_link_libraries(GuiSample_MT ImGui_MT SimulCrossPlatform_MT SimulMath_MT Core_MT directxtex_MT )

add_dynamic_executable(GuiSample_MD WIN32 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} ${shaders} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} INCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui LINK SimulCrossPlatform_MD)

target_link_libraries(GuiSample_MD ImGui_MD)

if(PLATFORM_SUPPORT_D3D12)
	add_dependencies( GuiSample_MD DirectX12CoreShaders )
	add_dependencies( GuiSample_MT DirectX12CoreShaders )
	target_link_libraries(GuiSample_MD SimulDirectX12_MD )
	target_link_libraries(GuiSample_MT SimulDirectX12_MT )
endif()
if(PLATFORM_SUPPORT_VULKAN)
	add_dependencies( GuiSample_MD VulkanCoreShaders )
	add_dependencies( GuiSample_MT VulkanCoreShaders )
	target_link_libraries(GuiSample_MD SimulVulkan_MD )
	target_link_libraries(GuiSample_MT SimulVulkan_MT )
endif()