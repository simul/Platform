// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <tchar.h>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/CubemapFramebuffer.h"
#include <d3d11.h>
#if WINVER<0x0602
#include <d3dx9.h>
#include <d3dx11.h>
#endif
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"

#ifndef RENDERDEPTHBUFFERCALLBACK
#define RENDERDEPTHBUFFERCALLBACK
class RenderDepthBufferCallback
{
public:
	virtual void Render()=0;
};
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif
namespace simul
{
	//! The namespace for the DirectX 11 platform library and its rendering classes.
	namespace dx11
	{
		struct TwoResFramebuffer:public simul::clouds::TwoResFramebuffer
		{
			TwoResFramebuffer();
			BaseFramebuffer *GetLowResFarFramebuffer()
			{
				return &lowResFarFramebufferDx11;
			}
			BaseFramebuffer *GetLowResNearFramebuffer()
			{
				return &lowResNearFramebufferDx11;
			}
			BaseFramebuffer *GetHiResFarFramebuffer()
			{
				return &hiResFarFramebufferDx11;
			}
			BaseFramebuffer *GetHiResNearFramebuffer()
			{
				return &hiResNearFramebufferDx11;
			}
			dx11::Framebuffer	lowResFarFramebufferDx11;
			dx11::Framebuffer	lowResNearFramebufferDx11;
			dx11::Framebuffer	hiResFarFramebufferDx11;
			dx11::Framebuffer	hiResNearFramebufferDx11;
			void RestoreDeviceObjects(void *);
			void InvalidateDeviceObjects();
			void SetDimensions(int w,int h,int downscale);
			int Width,Height,Downscale;
			ID3D11Device*	m_pd3dDevice;
		};
		//! An implementation of \link simul::clouds::BaseWeatherRenderer BaseWeatherRenderer\endlink for DirectX 10 and 11
		//! The DX10 switch is used
		SIMUL_DIRECTX11_EXPORT_CLASS SimulWeatherRendererDX11 : public simul::clouds::BaseWeatherRenderer
		{
		public:
			SimulWeatherRendererDX11(simul::clouds::Environment *env,simul::base::MemoryInterface *mem);
			virtual ~SimulWeatherRendererDX11();
			void SetScreenSize(int view_id,int w,int h);
			//standard d3d object interface functions
			void RestoreDeviceObjects(void*);
			void RecompileShaders();
			void InvalidateDeviceObjects();
			bool Destroy();
			//! Apply the view and projection matrices, once per frame.
			void SetMatrices(const simul::math::Matrix4x4 &viewmat,const simul::math::Matrix4x4 &projmat);
			void RenderSkyAsOverlay(void *context
											,int view_id											
											,const math::Matrix4x4 &viewmat
											,const math::Matrix4x4 &projmat
											,bool is_cubemap
											,float exposure
											,bool buffered
											,const void* mainDepthTexture
											,const void* lowResDepthTexture
											,const sky::float4& depthViewportXYWH
											,bool doFinalCloudBufferToScreenComposite);
			void RenderMixedResolution(	void *context
										,int view_id
										,const math::Matrix4x4 &viewmat
										,const math::Matrix4x4 &projmat
										,bool is_cubemap
										,float exposure
										,const void* mainDepthTextureMS	
										,const void* hiResDepthTexture	
										,const void* lowResDepthTexture 
										,const sky::float4& depthViewportXYWH
										);
			// This composites the clouds and other buffers to the screen.
			void CompositeCloudsToScreen(void *context
												,int view_id
												,bool depth_blend
												,const void* mainDepthTexture
												,const void* lowResDepthTexture
												,const simul::sky::float4& viewportRegionXYWH);
			void RenderFramebufferDepth(void *context,int view_id,int x0,int y0,int w,int h);
			void RenderCompositingTextures(void *context,int view_id,int x0,int y0,int w,int h);
			void RenderPrecipitation(void *context,const void *depth_tex,simul::sky::float4 depthViewportXYWH,const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p);
			void RenderLightning(void *context,int viewport_id,const void *depth_tex,simul::sky::float4 depthViewportXYWH,const void *low_res_depth_tex);
			void SaveCubemapToFile(const char *filename,float exposure,float gamma);
			//! Set the exposure, if we're using an hdr shader to render the sky buffer.
			void SetExposure(float ex){exposure=ex;}

			//! Get a pointer to the sky renderer owned by this class instance.
			class SimulSkyRendererDX1x *GetSkyRenderer();
			//! Get a pointer to the 3d cloud renderer owned by this class instance.
			class SimulCloudRendererDX1x *GetCloudRenderer();
			//! Get a pointer to the 2d cloud renderer owned by this class instance.
			class Simul2DCloudRendererDX11 *Get2DCloudRenderer();
			//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
			void SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb);
			void *GetCloudDepthTexture();

		protected:
			simul::base::MemoryInterface	*memoryInterface;
			// Keep copies of these matrices:
			simul::math::Matrix4x4 view;
			simul::math::Matrix4x4 proj;
			IDXGISwapChain *pSwapChain;
			ID3D11Device*							m_pd3dDevice;
			
			//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
			ID3DX11Effect							*m_pTonemapEffect;
			ID3DX11EffectTechnique					*simpleCloudBlendTechnique;
			ID3DX11EffectTechnique					*farNearDepthBlendTechnique;
			ID3DX11EffectTechnique					*showDepthTechnique;
			ID3DX11EffectShaderResourceVariable		*imageTexture;

			bool CreateBuffers();
			bool RenderBufferToScreen(ID3D11ShaderResourceView* texture,int w,int h,bool do_tonemap);
			class SimulSkyRendererDX1x				*simulSkyRenderer;
			class SimulCloudRendererDX1x			*simulCloudRenderer;
			class PrecipitationRenderer	*simulPrecipitationRenderer;
			class SimulAtmosphericsRendererDX1x		*simulAtmosphericsRenderer;
			class Simul2DCloudRendererDX11			*simul2DCloudRenderer;
			class LightningRenderer		*simulLightningRenderer;
			typedef std::map<int,simul::dx11::TwoResFramebuffer*> FramebufferMapDx11;
			// Map from view_id to framebuffer.
			TwoResFramebuffer *						GetFramebuffer(int view_id);
			FramebufferMapDx11						framebuffersDx11;
			simul::dx11::ConstantBuffer<HdrConstants> hdrConstants;
			float									exposure;
			float									gamma;
			float									exposure_multiplier;
			simul::math::Vector3					cam_pos;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
