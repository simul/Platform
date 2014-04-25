#pragma once
// Direct3D includes
#include <d3d11.h>
#include <d3dx11.h>
#include <dxerr.h>

#include "Simul/Platform/DirectX11/Direct3D11CallbackInterface.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Scene/BaseSceneRenderer.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/GpuSkyGenerator.h"
#include "Simul/Platform/DirectX11/CubemapFramebuffer.h"
#include "Simul/Platform/DirectX11/OceanRenderer.h"
#include "Simul/Platform/DirectX11/View.h"
#include "Simul/Platform/CrossPlatform/SL/mixed_resolution_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/light_probe_constants.sl"
#pragma warning(push)
#pragma warning(disable:4251)

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
	}
	namespace crossplatform
	{
		struct DeviceContext;
	}
	namespace scene
	{
		class Scene;
	}
	namespace dx11
	{
		class SimulWeatherRendererDX11;
		class SimulHDRRendererDX1x;
		class TerrainRenderer;
		class OceanRenderer;
		class SimulOpticsRendererDX1x;
		class SIMUL_DIRECTX11_EXPORT MixedResolutionRenderer
		{
		public:
			MixedResolutionRenderer();
			~MixedResolutionRenderer();
			void RestoreDeviceObjects(ID3D11Device* pd3dDevice);
			void InvalidateDeviceObjects();
			void RecompileShaders(const std::map<std::string,std::string> &defines);
			void DownscaleDepth(ID3D11DeviceContext* pContext,View *view,ID3D11ShaderResourceView *depth_SRV,int s,vec3 depthToLinFadeDistParams);
		protected:
			ID3D11Device								*m_pd3dDevice;
			ID3DX11Effect								*mixedResolutionEffect;
			ConstantBuffer<MixedResolutionConstants>	mixedResolutionConstants;
		};
		//! A renderer for DirectX11. Use this class as a guide to implementing your own rendering in DX11.
		class SIMUL_DIRECTX11_EXPORT Direct3D11Renderer
			:public Direct3D11CallbackInterface
			,public simul::base::Referenced
		{
		public:
			//! Constructor - pass a pointer to your Environment, and either an implementation of MemoryInterface, or NULL.
			Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m,int w,int h);
			virtual ~Direct3D11Renderer();
			META_BeginProperties
				META_ValueProperty(bool,ShowFlares				,"Whether to draw light flares around the sun and moon.")
				META_ValueProperty(bool,ShowCloudCrossSections	,"Show the cloud textures as an overlay.")
				META_ValueProperty(bool,Show2DCloudTextures		,"Show the 2D cloud textures as an overlay.")
				META_ValueProperty(bool,ShowWaterTextures		,"Show the textures generated for water effects as an overlay.")
				META_ValueProperty(bool,ShowFades				,"Show the fade textures as an overlay.")
				META_ValueProperty(bool,ShowTerrain				,"Whether to draw the terrain.")
				META_ValueProperty(bool,ShowMap					,"Show the terrain map as an overlay.")
				META_ValueProperty(bool,UseHdrPostprocessor		,"Whether to apply post-processing for exposure and gamma-correction using a post-processing renderer.")
				META_ValueProperty(bool,UseSkyBuffer			,"Render the sky to a low-res buffer to increase performance.")
				META_ValueProperty(bool,ShowDepthBuffers		,"Show the depth buffers .")
				META_ValueProperty(bool,ShowLightVolume			,"Show the cloud light volume as a wireframe box.")
				META_ValueProperty(bool,CelestialDisplay		,"Show geographical and sidereal overlay.")
				META_ValueProperty(bool,ShowWater				,"Show water surfaces.")
				META_ValueProperty(bool,MakeCubemap				,"Render a cubemap each frame.")
				META_ValueProperty(bool,ShowCubemaps			,"Show any generated cubemaps onscreen.")
				META_ValueProperty(bool,ShowRainTextures		,"Show rain textures onscreen.")
				META_ValuePropertyWithSetCall(bool,ReverseDepth,ReverseDepthChanged,"Reverse the direction of the depth (Z) buffer, so that depth 0 is the far plane.")
				META_ValueProperty(bool,ShowOSD					,"Show debug display.")
				META_ValueProperty(float,Exposure				,"A linear multiplier for rendered brightness.")
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
			void						RenderCubemap(ID3D11DeviceContext* pContext,D3DXVECTOR3 cam_pos);
			void						RenderEnvmap(ID3D11DeviceContext* pContext);
			// D3D11CallbackInterface
			virtual D3D_FEATURE_LEVEL	GetMinimumFeatureLevel() const;
			virtual void				OnD3D11CreateDevice	(ID3D11Device* pd3dDevice);
			virtual int					AddView				();
			virtual void				RemoveView			(int);
			virtual void				ResizeView			(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
			virtual void				Render				(int,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext);
			virtual void				OnD3D11LostDevice	();
			virtual void				OnD3D11DestroyDevice();
			virtual bool				OnDeviceRemoved		();
			virtual void				OnFrameMove			(double fTime,float fTimeStep);
			virtual const char *		GetDebugText		() const;
			void SetViewType(int view_id,ViewType vt);
			void SetCamera(int view_id,const simul::camera::CameraOutputInterface *c);
			void SaveScreenshot(const char *filename_utf8);
		protected:
			void RenderDepthBuffers(void *context,int view_id,int x0,int y0,int w,int h);
			// Encompasses drawing the actual scene and putting the hdr buffer to screen.
			void RenderScene(int view_id,crossplatform::DeviceContext &deviceContext,clouds::BaseWeatherRenderer *w,D3DXMATRIX v,D3DXMATRIX proj);
			// Different kinds of view for Render() to call:
			void RenderFadeEditView(ID3D11DeviceContext* pd3dImmediateContext);
			void RenderOculusView(ID3D11DeviceContext* pd3dImmediateContext);
			void DownscaleDepth(int view_id,ID3D11DeviceContext* pContext,const D3DXMATRIX &proj);
			void ReverseDepthChanged();
			void AntialiasingChanged();
			void EnsureCorrectBufferSizes(int view_id);

			int											cubemap_view_id;
			bool										enabled;
			std::string									screenshotFilenameUtf8;
			ID3D11Device								*m_pd3dDevice;
			ID3DX11Effect								*lightProbesEffect;
			SimulOpticsRendererDX1x						*simulOpticsRenderer;
			SimulWeatherRendererDX11					*simulWeatherRenderer;
			SimulHDRRendererDX1x						*simulHDRRenderer;
			TerrainRenderer								*simulTerrainRenderer;
			OceanRenderer								*oceanRenderer;
			simul::scene::BaseSceneRenderer				*sceneRenderer;
			ViewManager									viewManager;
			simul::dx11::CubemapFramebuffer				cubemapFramebuffer;
			simul::dx11::CubemapFramebuffer				envmapFramebuffer;
			ConstantBuffer<LightProbeConstants>			lightProbeConstants;
			simul::base::MemoryInterface				*memoryInterface;
			MixedResolutionRenderer						mixedResolutionRenderer;
		};
	}
}
#pragma warning(pop)