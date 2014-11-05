#pragma once
// Direct3D includes
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#endif

#include "Simul/Platform/DirectX11/Direct3D11CallbackInterface.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Sky/BaseGpuSkyGenerator.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
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
		enum TrueSkyRenderMode;
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
	namespace dx11
	{
		//! A renderer for DirectX11. Use this class as a guide to implementing your own rendering in DX11.
		class SIMUL_DIRECTX11_EXPORT TrueSkyRenderer:public clouds::TrueSkyRenderer
		{
		public:
			//! Constructor - pass a pointer to your Environment, and either an implementation of MemoryInterface, or NULL.
			TrueSkyRenderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m);
			virtual ~TrueSkyRenderer();
			META_BeginProperties
				META_ValueProperty(bool,ShowWater				,"Show water surfaces.")
				META_ValueProperty(bool,ShowWaterTextures		,"Show the textures generated for water effects as an overlay.")
			META_EndProperties
			void RestoreDeviceObjects	(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void Render(int view_id,ID3D11DeviceContext* pContext);
			///////////////////////////
			crossplatform::HdrRenderer *GetSimulHDRRenderer()
			{
				return simulHDRRenderer;
			}
			terrain::BaseSeaRenderer *GetOceanRenderer()
			{
				return oceanRenderer;
			}
			void	RecompileShaders();
			void	ReloadTextures();
			void	RenderCubemap(crossplatform::DeviceContext &deviceContext,const float *cam_pos);
			void	RenderEnvmap(crossplatform::DeviceContext &deviceContext);
			void	SetViewType(int view_id,crossplatform::ViewType vt);
			void	SetCamera(int view_id,const simul::crossplatform::CameraOutputInterface *c);
			void	SaveScreenshot(const char *filename_utf8,int width=0,int height=0,float exposure=1.0f,float gamma=0.44f);
		protected:
			void	RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Viewport viewport,int x0,int y0,int w,int h);
			/// Parts of the scene that go into the main buffer with depth active.
			void	RenderDepthElements(crossplatform::DeviceContext &deviceContext
									,float exposure
									,float gamma);
			/// Render the sky.
			void	RenderMixedResolutionSky(crossplatform::DeviceContext &deviceContext
									,crossplatform::Texture *depthTexture
									,float exposure
									,float gamma);
			void RenderStandard(crossplatform::DeviceContext &deviceContext,const crossplatform::CameraViewStruct &cameraViewStruct);
			void RenderToOculus(crossplatform::DeviceContext &deviceContext,const crossplatform::CameraViewStruct &cameraViewStruct);
			void RenderOverlays(crossplatform::DeviceContext &deviceContext,const crossplatform::CameraViewStruct &cameraViewStruct);
			// Different kinds of view for Render() to call:
			void RenderOculusView(ID3D11DeviceContext* pd3dImmediateContext);
			void ReverseDepthChanged();
			void AntialiasingChanged();
			void EnsureCorrectBufferSizes(int view_id);
			
			terrain::BaseSeaRenderer					*oceanRenderer;
		};
		class SIMUL_DIRECTX11_EXPORT Direct3D11Renderer
			:public Direct3D11CallbackInterface
		{
		public:
			Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m);
			~Direct3D11Renderer();
			simul::dx11::RenderPlatform renderPlatformDx11;
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