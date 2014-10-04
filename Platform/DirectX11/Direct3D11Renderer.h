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
#include "Simul/Platform/DirectX11/CubemapFramebuffer.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/DirectX11/OceanRenderer.h"
#include "Simul/Platform/CrossPlatform/MixedResolutionView.h"
#include "Simul/Platform/CrossPlatform/SL/light_probe_constants.sl"
#pragma warning(push)
#pragma warning(disable:4251)
#if !defined(_XBOX_ONE) && defined(SIMUL_DYNAMIC_LINK)
#define SIMUL_USE_SCENE
#endif
namespace simul
{
	namespace camera
	{
		class CameraOutputInterface;
		class CameraOutputInterface;
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
	namespace scene
	{
		class Scene;
		class BaseSceneRenderer;
	}
	namespace dx11
	{
		enum PerformanceTestLevel
		{
			DEFAULT
			,FORCE_1920_1080
			,FORCE_2560_1600
			,FORCE_3840_2160
		};
		class SimulWeatherRendererDX11;
		class SimulHDRRendererDX1x;
		class TerrainRenderer;
		class OceanRenderer;
		class SimulOpticsRendererDX1x;
		//! A renderer for DirectX11. Use this class as a guide to implementing your own rendering in DX11.
		class SIMUL_DIRECTX11_EXPORT Direct3D11Renderer
			:public Direct3D11CallbackInterface
			,public simul::base::Referenced
		{
		public:
			//! Constructor - pass a pointer to your Environment, and either an implementation of MemoryInterface, or NULL.
			Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m);
			virtual ~Direct3D11Renderer();
			META_BeginProperties
				META_ValueProperty(bool,ShowFlares				,"Whether to draw light flares around the sun and moon.")
				META_ValueProperty(bool,ShowWaterTextures		,"Show the textures generated for water effects as an overlay.")
				META_ValueProperty(bool,ShowTerrain				,"Whether to draw the terrain.")
				META_ValueProperty(bool,ShowMap					,"Show the terrain map as an overlay.")
				META_ValueProperty(clouds::TrueSkyRenderMode,trueSkyRenderMode		,"Whether to use the mixed-resolution renderer.")
				META_ValueProperty(bool,DepthBasedComposite		,"Whether to blend the sky and clouds using depth.")
				META_ValueProperty(bool,UseHdrPostprocessor		,"Whether to apply post-processing for exposure and gamma-correction using a post-processing renderer.")
				META_ValueProperty(bool,UseSkyBuffer			,"Render the sky to a low-res buffer to increase performance.")
				META_ValueProperty(bool,ShowHDRTextures			,"Show the HDR glow textures.")
				META_ValueProperty(bool,ShowLightVolume			,"Show the cloud light volume as a wireframe box.")
				META_ValueProperty(bool,ShowGroundGrid			,"Show a metre-scale grid at ground level.")
				META_ValueProperty(bool,ShowWater				,"Show water surfaces.")
				META_ValueProperty(bool,MakeCubemap				,"Render a cubemap each frame.")
				META_ValueProperty(bool,ShowCubemaps			,"Show any generated cubemaps onscreen.")
				META_ValueRangeProperty(PerformanceTestLevel,PerformanceTest,DEFAULT,DEFAULT,FORCE_3840_2160,"Force rendering at a higher resolution to test performance");
				META_ValuePropertyWithSetCall(bool,ReverseDepth,ReverseDepthChanged,"Reverse the direction of the depth (Z) buffer, so that depth 0 is the far plane.")
				META_ValueProperty(bool,ShowOSD					,"Show debug display.")
				META_ValueProperty(float,SkyBrightness			,"Brightness of the sky (only).")
				META_ValuePropertyWithSetCall(int,Antialiasing	,AntialiasingChanged,"How many antialiasing samples to use.")
				META_ValueProperty(int,SphericalHarmonicsBands	,"How many bands to use for spherical harmonics.")
			META_EndProperties
			bool IsEnabled()const
			{
				return enabled;
			}
			class SimulWeatherRendererDX11 *GetSimulWeatherRenderer()
			{
				return simulWeatherRenderer;
			}
			class SimulHDRRendererDX1x *GetSimulHDRRenderer()
			{
				return simulHDRRenderer;
			}
			OceanRenderer				*GetOceanRenderer()
			{
				return oceanRenderer;
			}
			TerrainRenderer	*GetTerrainRenderer()
			{
				return simulTerrainRenderer;
			}
			void						RecompileShaders();
			void						ReloadTextures();
			void						RenderCubemap(crossplatform::DeviceContext &deviceContext,const float *cam_pos);
			void						RenderEnvmap(crossplatform::DeviceContext &deviceContext);
			// D3D11CallbackInterface
			virtual D3D_FEATURE_LEVEL	GetMinimumFeatureLevel() const;
			virtual void				OnD3D11CreateDevice	(ID3D11Device* pd3dDevice);
			virtual int					AddView				(bool external_fb);
			virtual void				RemoveView			(int);
			virtual void				ResizeView			(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
			virtual void				Render				(int,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext);
			virtual void				OnD3D11LostDevice	();
			virtual void				OnD3D11DestroyDevice();
			virtual bool				OnDeviceRemoved		();
			virtual void				OnFrameMove			(double fTime,float fTimeStep);
			virtual const char *		GetDebugText		() const;
			void SetViewType(int view_id,crossplatform::ViewType vt);
			void SetCamera(int view_id,const simul::camera::CameraOutputInterface *c);
			void SaveScreenshot(const char *filename_utf8,int width=0,int height=0,float exposure=1.0f,float gamma=0.44f);
			simul::dx11::RenderPlatform renderPlatformDx11;
		protected:
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Viewport viewport,int x0,int y0,int w,int h);
			/// Parts of the scene that go into the main buffer with depth active.
			void RenderDepthElements(crossplatform::DeviceContext &deviceContext
				,float exposure
				,float gamma);
			/// Draw the trueSKY elements as background. Use with RenderForegroundSky in place of RenderMixedResolutionSky for faster performance.
			void RenderBackgroundSky(crossplatform::DeviceContext &deviceContext
									 ,float exposure
									 ,float gamma);
			/// Render the sky.
			void RenderMixedResolutionSky(crossplatform::DeviceContext &deviceContext
				,crossplatform::Texture *depthTexture
				,float exposure
				,float gamma);
			void RenderStandard(crossplatform::DeviceContext &deviceContext
				,const camera::CameraViewStruct &cameraViewStruct);
			void RenderToOculus(crossplatform::DeviceContext &deviceContext
				,const camera::CameraViewStruct &cameraViewStruct);
			void RenderOverlays(crossplatform::DeviceContext &deviceContext,const camera::CameraViewStruct &cameraViewStruct);
			// Different kinds of view for Render() to call:
			void RenderOculusView(ID3D11DeviceContext* pd3dImmediateContext);
			void ReverseDepthChanged();
			void AntialiasingChanged();
			void EnsureCorrectBufferSizes(int view_id);

			int											cubemap_view_id;
			bool										enabled;
			std::string									screenshotFilenameUtf8;
			ID3D11Device								*m_pd3dDevice;
			ID3DX11Effect								*lightProbesEffect;
			crossplatform::Effect						*linearizeDepthEffect;
			SimulOpticsRendererDX1x						*simulOpticsRenderer;
			SimulWeatherRendererDX11					*simulWeatherRenderer;
			SimulHDRRendererDX1x						*simulHDRRenderer;
			TerrainRenderer								*simulTerrainRenderer;
			OceanRenderer								*oceanRenderer;
			simul::scene::BaseSceneRenderer				*sceneRenderer;
			crossplatform::MixedResolutionViewManager	viewManager;
			simul::dx11::CubemapFramebuffer				cubemapFramebuffer;
			simul::dx11::CubemapFramebuffer				envmapFramebuffer;
			simul::dx11::Framebuffer					msaaFramebuffer;
			ConstantBuffer<LightProbeConstants>			lightProbeConstants;
			simul::base::MemoryInterface				*memoryInterface;
			ID3D11RenderTargetView	*mainRenderTarget;
			ID3D11DepthStencilView	*mainDepthSurface;
			simul::crossplatform::Texture				*linearDepthTexture;
			std::map<int,const simul::camera::CameraOutputInterface *> cameras;
			bool AllOsds;
			crossplatform::DemoOverlay *demoOverlay;
		};
	}
}
#pragma warning(pop)