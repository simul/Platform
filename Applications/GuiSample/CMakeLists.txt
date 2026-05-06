cmake_minimum_required(VERSION 3.12)

file(GLOB SOURCES GuiSample.cpp  )
file(GLOB HEADERS "*.h" )
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")

source_group("Shaders" src FILES ${shaders} )
 
add_static_executable(GuiSample${STATIC_LINK_SUFFIX} WIN32 SOURCES ${SOURCES} ${HEADERS} ${shaders} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} INCLUDES ${SIMUL_PLATFORM_DIR}/External/imgui)
if(PLATFORM_SUPPORT_D3D11)
	target_link_libraries(GuiSample${STATIC_LINK_SUFFIX} PRIVATE PlatformDirectX11${STATIC_LINK_SUFFIX})
endif()
if(PLATFORM_SUPPORT_D3D12)
	target_link_libraries(GuiSample${STATIC_LINK_SUFFIX} PRIVATE PlatformDirectX12${STATIC_LINK_SUFFIX})
endif()
if(PLATFORM_SUPPORT_VULKAN)
	target_link_libraries(GuiSample${STATIC_LINK_SUFFIX} PRIVATE PlatformVulkan${STATIC_LINK_SUFFIX} ${Vulkan_LIBRARY})
endif()
target_link_libraries(GuiSample${STATIC_LINK_SUFFIX} PRIVATE ImGui${STATIC_LINK_SUFFIX} PlatformCrossPlatform${STATIC_LINK_SUFFIX} PlatformMath${STATIC_LINK_SUFFIX} Core${STATIC_LINK_SUFFIX} glfw fmt::fmt-header-only)
if(WIN32)
	target_link_libraries(GuiSample${STATIC_LINK_SUFFIX} PRIVATE directxtex${STATIC_LINK_SUFFIX})
endif()

