#ifndef SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#define SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#include "BaseRenderer.h"
#include <functional>
struct ID3D11DeviceContext;
struct IDirect3DDevice9;
namespace sce
{
	namespace Gnmx
	{
		class LightweightGfxContext;
	}
}
namespace simul
{
	namespace crossplatform
	{
		class EffectTechnique;
		class RenderPlatform;
		//! The base class for Device contexts. The actual context pointer is only applicable in DirectX - in OpenGL, it will be null.
		//! The DeviceContext also carries a pointer to the platform-specific RenderPlatform.
		//! DeviceContext is context in the DirectX11 sense, encompassing a platform-specific deviceContext pointer
		struct DeviceContext
		{
			void *platform_context;
			RenderPlatform *renderPlatform;
			EffectTechnique *activeTechnique;
			long long frame_number;
			bool initialized;
			DeviceContext():
				platform_context(0)
				,renderPlatform(0)
				,activeTechnique(0)
				,frame_number(0)
			{
				viewStruct.depthTextureStyle=crossplatform::PROJECTION;
			}
			inline ID3D11DeviceContext *asD3D11DeviceContext()
			{
				return (ID3D11DeviceContext*)platform_context;
			}
			inline IDirect3DDevice9 *asD3D9Device()
			{
				return (IDirect3DDevice9*)platform_context;
			}
			inline sce::Gnmx::LightweightGfxContext *asGfxContext()
			{
				return (sce::Gnmx::LightweightGfxContext*)platform_context;
			}
			ViewStruct viewStruct;
		};

		struct DeviceContext;
	
		// A simple render delegate, it will usually be a function partially bound with std::bind.
		typedef std::function<void(crossplatform::DeviceContext&)> RenderDelegate;
		typedef std::function<void(void*)> StartupDeviceDelegate;
		typedef std::function<void()> ShutdownDeviceDelegate;
	}
}
#endif