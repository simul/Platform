#ifndef SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#define SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#include "BaseRenderer.h"
#include "Texture.h"
#include "Export.h"
#include <functional>
#include <stack>
struct ID3D11DeviceContext;
struct IDirect3DDevice9;
struct ID3D12GraphicsCommandList;
namespace sce
{
	namespace Gnmx
	{
		class LightweightGfxContext;
	}
}
namespace nvn
{
	class CommandBuffer;
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
		struct SIMUL_CROSSPLATFORM_EXPORT DeviceContext
		{
			void *platform_context;
			RenderPlatform *renderPlatform;
			EffectTechnique *activeTechnique;
			long long frame_number;
			bool initialized;
			DeviceContext();
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
			inline nvn::CommandBuffer* asNVNContext()
			{
				return (nvn::CommandBuffer*)platform_context;
			}
			inline ID3D12GraphicsCommandList* asD3D12Context()
			{
				return (ID3D12GraphicsCommandList*)platform_context;
			}
			ViewStruct viewStruct;
			uint cur_backbuffer;
		public:
			std::stack<crossplatform::TargetsAndViewport*>& GetFrameBufferStack();
			crossplatform::TargetsAndViewport defaultTargetsAndViewport;
			//! Set the RT's to restore to, once all Simul Framebuffers are deactivated. This must be called at least once,
			//! as 
			void setDefaultRenderTargets(const ApiRenderTarget*,
				const ApiDepthRenderTarget*,
				uint32_t viewportLeft,
				uint32_t viewportTop,
				uint32_t viewportRight,
				uint32_t viewportBottom
			);
			std::stack<crossplatform::TargetsAndViewport*> targetStack;
		};

		struct DeviceContext;
	
		// A simple render delegate, it will usually be a function partially bound with std::bind.
		typedef std::function<void(crossplatform::DeviceContext&)> RenderDelegate;
		typedef std::function<void(void*)> StartupDeviceDelegate;
		typedef std::function<void()> ShutdownDeviceDelegate;
	}
}
#endif