#pragma once
// Direct3D includes
#include <d3d11.h>
#include <d3dx11.h>
#include <dxerr.h>

#include "Simul/Platform/DirectX11/Direct3D11CallbackInterface.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/GpuSkyGenerator.h"
#include "Simul/Platform/DirectX11/CubemapFramebuffer.h"
#include "Simul/Platform/DirectX11/OceanRenderer.h"
#include "Simul/Platform/CrossPlatform/mixed_resolution_constants.sl"
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace camera
	{
		class Camera;
	}
	namespace clouds
	{
		class Environment;
	}
}
namespace simul
{
	namespace dx11
	{
		enum ViewType
		{
			MAIN_3D_VIEW
			,FADE_EDITING
		};
		struct View
		{
			View();
			~View();
			void RestoreDeviceObjects(ID3D11Device *m_pd3dDevice);
			void InvalidateDeviceObjects();
			int ScreenWidth;
			int ScreenHeight;
			// A framebuffer with depth
			simul::dx11::Framebuffer			hdrFramebuffer;
			// The depth from the HDR framebuffer can be resolved into this texture:
			simul::dx11::Framebuffer			resolvedDepth_fb;
			simul::dx11::TextureStruct			lowResDepthTexture;
			ViewType							viewType;
		};
		class SimulWeatherRendererDX11;
		class SimulHDRRendererDX1x;
		class SimulTerrainRendererDX1x;
		class OceanRenderer;
		class SimulOpticsRendererDX1x;

		//! A renderer for DirectX11. Use this class as a guide to implementing your own rendering in DX11.
		class SIMUL_DIRECTX11_EXPORT Direct3D11Renderer
			:public Direct3D11CallbackInterface
			,public simul::graph::meta::Group
		{
		public:
			//! Constructor - pass a pointer to your Environment, and either an implementation of MemoryInterface, or NULL.
			Direct3D11Renderer(simul::clouds::Environment *env,simul::base::MemoryInterface *m,int w,int h);
			virtual ~Direct3D11Renderer();
			META_BeginProperties
				META_ValueProperty(bool,ShowFlares				,"Whether to draw light flares around the sun and moon.")
				META_ValueProperty(bool,ShowCloudCrossSections	,"Show the cloud textures as an overlay.")
				META_ValueProperty(bool,Show2DCloudTextures		,"Show the 2D cloud textures as an overlay.")
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
				META_ValuePropertyWithSetCall(bool,ReverseDepth,ReverseDepthChanged,"Reverse the direction of the depth (Z) buffer, so that depth 0 is the far plane.")
				META_ValueProperty(bool,ShowOSD					,"Show debug display.")
				META_ValueProperty(float,Exposure				,"A linear multiplier for rendered brightness.")
				META_ValueProperty(int,Antialiasing				,"How many antialiasing samples to use.")
			META_EndProperties
			bool IsEnabled()const{return enabled;}
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
			void SetCamera(simul::camera::Camera *c)
			{
				camera=c;
			}
			void	RecompileShaders();
			void	RenderCubemap(ID3D11DeviceContext* pd3dImmediateContext,D3DXVECTOR3 cam_pos);
			// D3D11CallbackInterface
			virtual D3D_FEATURE_LEVEL	GetMinimumFeatureLevel() const;
			virtual void				OnD3D11CreateDevice	(ID3D11Device* pd3dDevice);
			virtual int					AddView				(const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
			virtual void				Render				(int,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext);
			virtual void				OnD3D11LostDevice	();
			virtual void				OnD3D11DestroyDevice();
			virtual void				RemoveView			(int);
			virtual bool				OnDeviceRemoved		();
			virtual void				OnFrameMove			(double fTime,float fTimeStep);
			virtual const char *		GetDebugText		() const;

			void SetViewType(int view_id,const char *txt);
			void SaveScreenshot(const char *filename_utf8);
		protected:
			int last_created_view_id;
			int cubemap_view_id;
			void DownscaleDepth(int view_id,ID3D11DeviceContext* pContext,const D3DXMATRIX &proj);
			void ReverseDepthChanged();
			bool enabled;
			ID3D11Device				*m_pd3dDevice;
			ID3DX11Effect				*mixedResolutionEffect;
			std::string screenshotFilenameUtf8;
			simul::camera::Camera *camera;
			SimulOpticsRendererDX1x		*simulOpticsRenderer;
			SimulWeatherRendererDX11	*simulWeatherRenderer;
			SimulHDRRendererDX1x		*simulHDRRenderer;
			SimulTerrainRendererDX1x	*simulTerrainRenderer;
			OceanRenderer				*oceanRenderer;
			typedef std::map<int,View*>	ViewMap;
			ViewMap						views;
			simul::dx11::CubemapFramebuffer		cubemapFramebuffer;
			simul::base::MemoryInterface *memoryInterface;
			ConstantBuffer<MixedResolutionConstants> mixedResolutionConstants;
		};
	}
}
#pragma warning(pop)