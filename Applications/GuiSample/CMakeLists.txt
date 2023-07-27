cmake_minimum_required(VERSION 3.5)

file(GLOB SOURCES GuiSample.cpp  )
file(GLOB HEADERS "*.h" )
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")

source_group("Shaders" src FILES ${shaders} )
 
add_static_executable(GuiSample_MT WIN32 SOURCES ${SOURCES} ${HEADERS} ${shaders} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} INCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui)
target_link_libraries(GuiSample_MT ImGui_MT SimulCrossPlatform_MT SimulMath_MT Core_MT directxtex_MT )
if(PLATFORM_SUPPORT_D3D11)
	target_link_libraries(GuiSample_MT SimulDirectX11_MT )
endif()
if(PLATFORM_SUPPORT_D3D12)
	target_link_libraries(GuiSample_MT SimulDirectX12_MT )
endif()
if(PLATFORM_SUPPORT_VULKAN)
	target_link_libraries(GuiSample_MT SimulVulkan_MT ${Vulkan_LIBRARY} )
endif()
