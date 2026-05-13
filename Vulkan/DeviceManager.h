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
		class RenderPlatform;
		class DeviceManagerInternal;

		class SIMUL_VULKAN_EXPORT DeviceManager : public crossplatform::GraphicsDeviceInterface
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
			crossplatform::GPUInfo	GetGPUInfo() override;

			vk::PhysicalDevice *GetGPU();
			vk::Device *GetVulkanDevice();
			vk::Instance *GetVulkanInstance();

		protected:
			void CreateInstance(bool use_debug, std::vector<std::string> required_instance_extensions);
			void CreateDevice(bool use_debug, std::vector<std::string> required_device_extensions);
			void SetupDebugCallback(bool debugUtils, bool debugReport, bool debugMarker);

			uint32_t apiVersion;

			std::vector<vk::LayerProperties> instanceLayers;
			std::vector<vk::ExtensionProperties> instanceExtensions;
			std::vector<vk::LayerProperties> deviceLayers;
			std::vector<vk::ExtensionProperties> deviceExtensions;

			std::vector<std::string> instanceLayerNames;
			std::vector<std::string> instanceExtensionNames;
			std::vector<std::string> deviceLayerNames;
			std::vector<std::string> deviceExtensionNames;

			bool deviceInitialized;
			std::unique_ptr<DeviceManagerInternal> deviceManagerInternal;

			vk::DebugReportCallbackEXT debugReportCallback;
			vk::DebugReportCallbackCreateInfoEXT debugReportCallbackCI;
			vk::DebugUtilsMessengerEXT debugUtilsMessenger;
			vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI;

			vk::PhysicalDeviceSamplerYcbcrConversionFeatures physicalDeviceSamplerYcbcrConversionFeatures;

			vk::PhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures;
			vk::PhysicalDeviceMultiviewProperties physicalDeviceMultiviewProperties;

			vk::PhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeatures;
			vk::PhysicalDeviceMeshShaderPropertiesEXT physicalDeviceMeshShaderProperties;

			vk::PhysicalDeviceVulkan14Features physicalDeviceVulkan14Features;
			vk::PhysicalDeviceVulkan14Properties physicalDeviceVulkan14Properties;

			vk::PhysicalDeviceVulkan13Features physicalDeviceVulkan13Features;
			vk::PhysicalDeviceVulkan13Properties physicalDeviceVulkan13Properties;

			vk::PhysicalDeviceVulkan12Features physicalDeviceVulkan12Features;
			vk::PhysicalDeviceVulkan12Properties physicalDeviceVulkan12Properties;

			vk::PhysicalDeviceVulkan11Features physicalDeviceVulkan11Features;
			vk::PhysicalDeviceVulkan11Properties physicalDeviceVulkan11Properties;


			friend vulkan::RenderPlatform;
		};
		extern DeviceManager* deviceManager;
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif