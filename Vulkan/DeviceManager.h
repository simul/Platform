#pragma once
#include <vulkan/vulkan.hpp>
#include "Platform/Vulkan/Export.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include <vector>


#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace platform
{
	namespace crossplatform
	{
		class RenderPlatform;
	}
	namespace vulkan
	{
		extern void InitQueueProperties(const vk::PhysicalDevice& gpu, std::vector<vk::QueueFamilyProperties>& queue_props);
		class RenderPlatform;
		class DeviceManagerInternal;
		class SIMUL_VULKAN_EXPORT DeviceManager
			: public crossplatform::GraphicsDeviceInterface
		{
		public:
			DeviceManager();
			virtual ~DeviceManager();
			// GDI:
			void	Initialize(bool use_debug, bool instrument, bool default_driver) override;
			bool	IsActive() const override;
			void	Shutdown() override;
			void*	GetDevice() override;
			void*	GetDeviceContext() override;
			int		GetNumOutputs() override;
			crossplatform::Output	GetOutput(int i) override;

			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();

			void Activate() ;

			void ReloadTextures();
			// called late to start debug output.
			void InitDebugging();
		//	platform::vulkan::RenderPlatform *renderPlatformVulkan;

			std::vector<vk::SurfaceFormatKHR> GetSurfaceFormats(vk::SurfaceKHR *surface);
			std::vector<vk::Image> GetSwapchainImages(vk::SwapchainKHR *swapchain);
			vk::PhysicalDevice *GetGPU();
			vk::Device *GetVulkanDevice();
			vk::Instance *GetVulkanInstance();
			uint32_t QueueFamilyCount() const
			{
				return queue_family_count;
			}
			const std::vector<vk::QueueFamilyProperties> &GetQueueProperties() const
			{
				return queue_props;
			}
		protected:
			void CreateDevice();
			void SetupDebugCallback();
			void RenderDepthBuffers(crossplatform::GraphicsDeviceContext &deviceContext,int x0,int y0,int w,int h);
			uint32_t enabled_extension_count;
			uint32_t enabled_layer_count;
			bool device_initialized;
			std::vector<const char *> extension_names;
			char const *enabled_layers[64];
			DeviceManagerInternal *deviceManagerInternal;
			bool separate_present_queue;
			uint32_t queue_family_count;
			std::vector<vk::QueueFamilyProperties> queue_props;
		};
		extern DeviceManager *deviceManager;
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif