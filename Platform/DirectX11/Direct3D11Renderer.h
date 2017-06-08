#pragma once
// Direct3D includes
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#endif

#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Clouds/TrueSkyRenderer.h"
#include "Simul/Base/Delegate.h"
#include <functional>

#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Direct3D11Renderer
			:public crossplatform::PlatformRendererInterface
		{
			crossplatform::RenderDelegate renderDelegate;
			std::vector<crossplatform::StartupDeviceDelegate> startupDeviceDelegates;
			std::vector<crossplatform::ShutdownDeviceDelegate> shutdownDeviceDelegates;
			void *device;
		public:
			Direct3D11Renderer(crossplatform::RenderPlatform *r,simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m);
			~Direct3D11Renderer();
			simul::crossplatform::RenderPlatform *renderPlatform;
			clouds::TrueSkyRenderer *GetTrueSkyRenderer()
			{
				return &trueSkyRenderer;
			}
			clouds::TrueSkyRenderer trueSkyRenderer;
			virtual void	OnCreateDevice				(void* pd3dDevice);
			virtual int		AddView						(bool external_fb);
			virtual void	RemoveView					(int);
			virtual void	ResizeView					(int view_id,int w,int h);
			virtual void	Render						(int,void* pd3dDevice,void* pd3dImmediateContext);
			virtual void	OnLostDevice				();
			void			SetRenderDelegate			(crossplatform::RenderDelegate d);
			void			RegisterStartupDelegate		(crossplatform::StartupDeviceDelegate d);
			void			RegisterShutdownDelegate	(crossplatform::ShutdownDeviceDelegate d);
		};
	}
}
#pragma warning(pop)