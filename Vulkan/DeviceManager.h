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
			void	Initialize(bool use_debug, bool instrument, bool default_driver,std::vector<std::string> required_device_extensions,std::vector<std::string> required_instance_extensions) ;
			bool	IsActive() const override;
			void	Shutdown() override;
			void*	GetDevice() override;
			void*	GetDeviceContext() override;
			int		GetNumOutputs() override;
			crossplatform::Output	GetOutput(int i) override;

			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();

			void Activate();

			void ReloadTextures();
			
			// called late to start debug output.
			void InitDebugging();

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
			
			std::vector<vk::LayerProperties> instance_layers;
			std::vector<vk::ExtensionProperties> instance_extensions;
			std::vector<vk::LayerProperties> device_layers;
			std::vector<vk::ExtensionProperties> device_extensions;
			
			std::vector<const char*> instance_layer_names;
			std::vector<const char*> instance_extension_names;
			std::vector<const char*> device_layer_names;
			std::vector<const char*> device_extension_names;

			bool device_initialized;
			DeviceManagerInternal *deviceManagerInternal;
			bool separate_present_queue;
			uint32_t queue_family_count;
			std::vector<vk::QueueFamilyProperties> queue_props;
			vk::DebugUtilsMessengerEXT debugUtilsMessenger;
			vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI;

			vk::PhysicalDeviceSamplerYcbcrConversionFeatures physicalDeviceSamplerYcbcrConversionFeatures;

			vk::PhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures;
			vk::PhysicalDeviceMultiviewProperties physicalDeviceMultiviewProperties;
		};
		extern DeviceManager *deviceManager;
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif