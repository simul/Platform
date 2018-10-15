#pragma once
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/Vulkan/Export.h"
#include <vector>


#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace vk
{
	class Image;
	class SurfaceKHR;
	class SwapchainKHR;
	class PhysicalDevice;
	struct SurfaceFormatKHR;
}

namespace simul
{
	namespace vulkan
	{
		class RenderPlatform;
		class DeviceManagerInternal;
		SIMUL_VULKAN_EXPORT_CLASS DeviceManager
			: public crossplatform::GraphicsDeviceInterface
		{
		public:
			DeviceManager();
			virtual ~DeviceManager();
			// GDI:
			void	Initialize(bool use_debug, bool instrument, bool default_driver) override;
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
			simul::vulkan::RenderPlatform *renderPlatformVulkan;

			void GetQueues(vk::SurfaceKHR *surface);
			std::vector<vk::SurfaceFormatKHR> GetSurfaceFormats(vk::SurfaceKHR *surface);
			std::vector<vk::Image> GetSwapchainImages(vk::SwapchainKHR *swapchain);
			vk::PhysicalDevice *GetGPU();
		protected:
			void CreateDevice();
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h);
			uint32_t enabled_extension_count;
			uint32_t enabled_layer_count;
			uint32_t graphics_queue_family_index;
			uint32_t present_queue_family_index;
			uint32_t current_buffer;
			uint32_t queue_family_count;
			bool device_initialized;
			bool separate_present_queue;
			char const *extension_names[64];
			char const *enabled_layers[64];
			DeviceManagerInternal *deviceManagerInternal;
		};
		extern DeviceManager *deviceManager;
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif