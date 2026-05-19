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
		static constexpr size_t FrameCount = SIMUL_VULKAN_FRAME_LAG + 1;

		struct CommandBufferResources
		{
			vk::CommandBuffer cmdBuffer;
			vk::CommandBuffer graphicsToPresentCmdBuffer;
		};
		
		struct SwapchainImageResources
		{
			vk::Image image;
			vk::ImageView view;
			vk::Framebuffer framebuffer;
		};

		class SIMUL_VULKAN_EXPORT DisplaySurface : public crossplatform::DisplaySurface
		{
		public:
			DisplaySurface(int view_id);
			virtual ~DisplaySurface();
			virtual void RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, crossplatform::PixelFormat outFmt) override;
			virtual void InvalidateDeviceObjects() override;
			virtual void Render(platform::core::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber) override;
			virtual void EndFrame() override;
			//! Push in an externally-known framebuffer extent for surfaces whose
			//! VkSurfaceCapabilitiesKHR::currentExtent is the "undefined" sentinel
			//! (Wayland, some XWayland setups). Picked up on the next Resize().
			void SetRequestedExtent(uint32_t w, uint32_t h);

		protected:
			void InitSwapChain();
			void GetQueues();
			void CreateSyncObjects();
			void CreateCommandPoolsAndBuffers();
			void CreateRenderPass();
			void CreateFramebuffers();
			void Present();

			//! Will resize the swap chain only if needed
			void Resize();
			void EnsureImageLayout();
			void EnsureImagePresentLayout();

			vk::Instance* GetVulkanInstance();
			vk::Device* GetVulkanDevice();
			vk::PhysicalDevice* GetGPU();

			// The format being used.
			crossplatform::PixelFormat pixelFormat = crossplatform::PixelFormat::UNDEFINED;
			// The format requested - may not be available.
			crossplatform::PixelFormat requestedPixelFormat = crossplatform::PixelFormat::UNDEFINED;

			vk::Format vulkanFormat;
			vk::ColorSpaceKHR colourSpace;
			vk::SwapchainKHR swapchain;
			vk::SurfaceKHR surface;

			std::vector<SwapchainImageResources> swapchainImageResources;
			std::vector<CommandBufferResources> cmdBufferResources;

			vk::Queue graphicsQueue;
			vk::Queue presentQueue;
			vk::CommandPool cmdPool;
			vk::CommandPool presentCmdPool;

			vk::Semaphore imageAcquiredSemaphores[FrameCount];
			vk::Semaphore drawCompleteSemaphores[FrameCount];
			vk::Semaphore imageOwnershipSemaphores[FrameCount];
			vk::Fence fences[FrameCount];
			vk::RenderPass renderPass;

			uint32_t frameIndex = 0; // Our internal frame index counter.
			uint32_t imageIndex;	 // Returned from Vulkan each frame.
			uint32_t graphicsQueueFamilyIndex;
			uint32_t presentQueueFamilyIndex;

			uint32_t pendingW = 0;
			uint32_t pendingH = 0;
		};
	}
}


#ifdef _MSC_VER
#pragma warning(pop)
#endif