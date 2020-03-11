#pragma once

#include <vulkan/vulkan.hpp>
#include "Export.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif
typedef struct GLFWwindow GLFWwindow;
namespace simul
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
        SIMUL_VULKAN_EXPORT_CLASS DisplaySurface : public crossplatform::DisplaySurface
        {
        public:
            DisplaySurface();
            ~DisplaySurface();
            void RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt) override;
            void InvalidateDeviceObjects() override;
            void Render(simul::base::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber) override;
			void EndFrame() override;
        private:
            //! Will resize the swap chain only if needed
            void Resize();
			crossplatform::DeviceContext deferredContext;
			crossplatform::PixelFormat pixelFormat;
#ifdef _MSC_VER
			HDC             hDC;
			HGLRC           hRC;
#endif
			vk::Format		vulkanFormat;
			vk::ColorSpaceKHR colour_space;
			vk::SwapchainKHR swapchain;
			std::vector<SwapchainImageResources> swapchain_image_resources;
			vk::Queue graphics_queue,present_queue;
			vk::CommandPool cmd_pool;
			vk::CommandPool present_cmd_pool;
			vk::CommandBuffer cmd;  
			vk::Semaphore image_acquired_semaphores[SIMUL_VULKAN_FRAME_LAG+1];
			vk::Semaphore draw_complete_semaphores[SIMUL_VULKAN_FRAME_LAG+1];
			vk::Semaphore image_ownership_semaphores[SIMUL_VULKAN_FRAME_LAG+1];
			vk::Fence fences[SIMUL_VULKAN_FRAME_LAG+1];
			vk::RenderPass render_pass;

			vk::Pipeline default_pipeline;
			vk::PipelineCache pipelineCache;
			vk::PipelineLayout pipeline_layout;
			vk::DescriptorSetLayout desc_layout;

			vk::SurfaceKHR mSurface;
			void InitSwapChain();
			void GetQueues();
			void CreateRenderPass();
			void CreateFramebuffers();
			void CreateDefaultLayout();
			void CreateDefaultPipeline();
            void Present();
			int frame_index;
			uint32_t current_buffer;
			uint32_t graphics_queue_family_index;
			uint32_t present_queue_family_index;
			vk::ShaderModule prepare_shader_module(const uint32_t *, size_t);
			vk::ShaderModule prepare_vs();
			vk::ShaderModule prepare_fs();
			vk::Instance *GetVulkanInstance();
			vk::Device *GetVulkanDevice();
			vk::PhysicalDevice* GetGPU();
        };
    }
}


#ifdef _MSC_VER
#pragma warning(pop)
#endif