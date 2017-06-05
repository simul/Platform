#pragma once
// Direct3D includes
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#endif

#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Platform/DirectX11on12/Export.h"
#include "Simul/Platform/DirectX11on12/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11on12/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/SL/light_probe_constants.sl"
#include "Simul/Clouds/TrueSkyRenderer.h"
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace crossplatform
	{
		class CameraOutputInterface;
		class BaseOpticsRenderer;
	}
	namespace clouds
	{
		class Environment;
		class BaseWeatherRenderer;
		enum class TrueSkyRenderMode;
	}
	namespace crossplatform
	{
		struct DeviceContext;
		class DemoOverlay;
	}
	namespace terrain
	{
		class BaseTerrainRenderer;
		class BaseSeaRenderer;
	}
	namespace dx11on12
	{
		//! A renderer for DirectX11. Use this class as a guide to implementing your own rendering in DX11.
		class SIMUL_DIRECTX12_EXPORT TrueSkyRenderer:public clouds::TrueSkyRenderer
		{
		public:
			//! Constructor - pass a pointer to your Environment, and either an implementation of MemoryInterface, or NULL.
			TrueSkyRenderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m);
			virtual ~TrueSkyRenderer();
			void RestoreDeviceObjects	(crossplatform::RenderPlatform *r);
		protected:
			// Different kinds of view for Render() to call:
			void RenderOculusView(ID3D11DeviceContext* pd3dImmediateContext);
		};
		class SIMUL_DIRECTX12_EXPORT Direct3D11Renderer
			:public crossplatform::PlatformRendererInterface
		{
		public:
			Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m);
			~Direct3D11Renderer();
			simul::dx11on12::RenderPlatform renderPlatformDx11;
			TrueSkyRenderer *GetTrueSkyRenderer()
			{
				return &trueSkyRenderer;
			}
			TrueSkyRenderer trueSkyRenderer;
			virtual D3D_FEATURE_LEVEL	GetMinimumFeatureLevel() const;
			virtual void				OnD3D11CreateDevice	(ID3D11Device* pd3dDevice);
			virtual int					AddView				(bool external_fb);
			virtual void				RemoveView			(int);
			virtual void				ResizeView			(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
			virtual void				Render				(int,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext);
			virtual void				OnD3D11LostDevice	();
		};
	}
}
#pragma warning(pop)