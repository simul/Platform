#pragma once

#include <vulkan/vulkan.hpp>
#include "Export.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif
typedef struct GLFWwindow GLFWwindow;
namespace platform
{
	namespace vulkan
	{
		struct SwapchainImageResources
		{
			vk::Image image;
			vk::CommandBuffer cmd;
			vk::CommandBuffer graphics_to_present_cmd;
			vk::ImageView view;
			vk::Framebuffer framebuffer;
		} ;
		class SIMUL_VULKAN_EXPORT DisplaySurface: public crossplatform::DisplaySurface
		{
		public:
			DisplaySurface(int view_id);
			virtual ~DisplaySurface();
			virtual void RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, crossplatform::PixelFormat outFmt) override;
			virtual void InvalidateDeviceObjects() override;
			virtual void Render(platform::core::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber) override;
			virtual void EndFrame() override;
		protected:
			//! Will resize the swap chain only if needed
			void Resize();
			crossplatform::DeviceContext deferredContext;
			crossplatform::PixelFormat pixelFormat = crossplatform::PixelFormat::UNDEFINED;
			// The format requested - may not be available.
			crossplatform::PixelFormat requestedPixelFormat = crossplatform::PixelFormat::UNDEFINED;

			vk::Format		vulkanFormat;
			vk::ColorSpaceKHR colour_space;
			vk::SwapchainKHR swapchain;
			std::vector<SwapchainImageResources> swapchain_image_resources;
			vk::Queue graphics_queue,present_queue;
			vk::CommandPool cmd_pool;
			vk::CommandPool present_cmd_pool;
			vk::Semaphore image_acquired_semaphores[SIMUL_VULKAN_FRAME_LAG+1];
			vk::Semaphore draw_complete_semaphores[SIMUL_VULKAN_FRAME_LAG+1];
			vk::Semaphore image_ownership_semaphores[SIMUL_VULKAN_FRAME_LAG+1];
			vk::Fence fences[SIMUL_VULKAN_FRAME_LAG+1];
			vk::RenderPass render_pass;

			vk::SurfaceKHR mSurface;
			void InitSwapChain();
			void GetQueues();
			void CreateRenderPass();
			void CreateFramebuffers();
			void Present();
			uint32_t current_buffer;
			uint32_t graphics_queue_family_index;
			uint32_t present_queue_family_index;
			vk::Instance *GetVulkanInstance();
			vk::Device *GetVulkanDevice();
			vk::PhysicalDevice* GetGPU();
			void EnsureImageLayout();
			void EnsureImagePresentLayout();
		};
	}
}


#ifdef _MSC_VER
#pragma warning(pop)
#endif