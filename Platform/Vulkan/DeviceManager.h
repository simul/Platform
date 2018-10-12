#pragma once
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/Vulkan/Export.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
		class RenderPlatform;
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
		protected:
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h);
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif