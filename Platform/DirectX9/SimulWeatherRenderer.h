// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
#include "Simul/Platform/DirectX9/SimulLightningRenderer.h"
#include "Simul/Platform/DirectX9/SimulPrecipitationRenderer.h"
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Platform/DirectX9/Framebuffer.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace clouds
	{
		class Environment;
	}
	namespace dx9
	{
		class SimulSkyRenderer;
		class SimulCloudRenderer;
	}
}
class SimulLightningRenderer;
class Simul2DCloudRenderer;
class SimulPrecipitationRenderer;
class SimulAtmosphericsRenderer;

namespace simul
{
	//! The namespace for the DirectX 9 platform library and its rendering classes.

	//! This library is deprecated in favour of the \ref  DirectX 11 library.
	namespace dx9
	{
		struct TwoResFramebuffer:public simul::clouds::TwoResFramebuffer
		{
			TwoResFramebuffer();
			BaseFramebuffer *GetLowResFarFramebuffer()
			{
				return &lowResFarFramebuffer;
			}
			BaseFramebuffer *GetLowResNearFramebuffer()
			{
				return &lowResNearFramebuffer;
			}
			BaseFramebuffer *GetHiResFarFramebuffer()
			{
				return &hiResFarFramebuffer;
			}
			BaseFramebuffer *GetHiResNearFramebuffer()
			{
				return &hiResNearFramebuffer;
			}
			dx9::Framebuffer	lowResFarFramebuffer;
			dx9::Framebuffer	lowResNearFramebuffer;
			dx9::Framebuffer	hiResFarFramebuffer;
			dx9::Framebuffer	hiResNearFramebuffer;
			void RestoreDeviceObjects(void *);
			void InvalidateDeviceObjects();
			void SetDimensions(int w,int h,int downscale);
			int Width,Height,Downscale;
			LPDIRECT3DDEVICE9	m_pd3dDevice;
		};
		//! A rendering class that encapsulates Simul skies and clouds.
		//! Create an instance of this class within a DirectX 9 program.
		//! You can take this entire class and use it as source in your project.
		//! Make appropriate modifications where required.
		SIMUL_DIRECTX9_EXPORT_CLASS SimulWeatherRenderer:public simul::clouds::BaseWeatherRenderer
		{
		public:
			SimulWeatherRenderer(simul::clouds::Environment *env
									,simul::base::MemoryInterface *mem
									,bool usebuffer=true
									,int width=320,
				int height=240,bool sky=true,
				bool rain=true);
			virtual ~SimulWeatherRenderer();
			void SetScreenSize(int view_id,int w,int h);
			//standard d3d object interface functions
			bool Create( LPDIRECT3DDEVICE9 pd3dDevice);
			void RecompileShaders();
			//! Call this when the device has been created
			void RestoreDeviceObjects(void *pd3dDevice);
			//! Call this when the 3D device has been lost.
			void InvalidateDeviceObjects();
			//! Call this to draw the sky and clouds.
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
											,bool doFinalCloudBufferToScreenComposite
									);
			void RenderMixedResolution(	void *
										,int 
										,const math::Matrix4x4 &
										,const math::Matrix4x4 &
										,bool 
										,float 
										,const void* 	
										,const void* 		
										,const void*  
										,const sky::float4& 
										){}
			//! Call this to draw the clouds after the main scene.
			void RenderLateCloudLayer(void *context,float exposure,bool buf,int viewport_id,const simul::sky::float4 &relativeViewportTextureRegionXYWH);
			//! Call this to draw lightning.
			void RenderLightning(void *context,int viewport_id);
			//! Call this to draw rain etc.
			void RenderPrecipitation(void *context);
			//! Perform the once-per-frame time update.
			void PreRenderUpdate(void *context,float dt);
		#if defined(XBOX) || defined(DOXYGEN)
			//! Call this once per frame to set the matrices (X360 only).
			void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
		#endif
			//! Set the exposure, if we're using an hdr shader to render the sky buffer.
			void SetExposure(float ex){exposure=ex;}
			//! Get a pointer to the sky renderer owned by this class instance.
			SimulSkyRenderer *GetSkyRenderer();
			//! Get a pointer to the 3d cloud renderer owned by this class instance.
			SimulCloudRenderer *GetCloudRenderer();
			//! Get a pointer to the 2d cloud renderer owned by this class instance.
			Simul2DCloudRenderer *Get2DCloudRenderer();
			//! Get a pointer to the rain renderer owned by this class instance.
			SimulPrecipitationRenderer *GetPrecipitationRenderer();
			//! Get a pointer to the atmospherics renderer owned by this class instance.
			SimulAtmosphericsRenderer *GetAtmosphericsRenderer();
			//! Get the current debug text as a c-string pointer. We don't use TCHAR here because Qt does not support using wchar_t as a built-in type.
			const char *GetDebugText() const;
			void EnableRain(bool e=true);
			float GetTotalBrightness() const;
			// Connect-up sky, clouds - now in base
			//void ConnectInterfaces();
		protected:
			 void ReverseDepthChanged()
			 {
				 ReverseDepth=false;
			 }
			bool Restore3DCloudObjects();
			bool Restore2DCloudObjects();
			LPDIRECT3DDEVICE9				m_pd3dDevice;
			LPDIRECT3DTEXTURE9 temp_depth_texture;
			//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
			LPD3DXEFFECT					m_pBufferToScreenEffect;
			D3DXHANDLE						SkyOverStarsTechnique;
			D3DXHANDLE						CloudBlendTechnique;

			D3DXHANDLE						bufferTexture;

			bool CreateBuffers();
			clouds::TwoResFramebuffer *		GetFramebuffer(int view_id);
			typedef std::map<int,TwoResFramebuffer*> FramebufferMap;
			FramebufferMap framebuffers;
			SimulSkyRenderer				*simulSkyRenderer;
			SimulCloudRenderer				*simulCloudRenderer;
			SimulLightningRenderer			*simulLightningRenderer;
			Simul2DCloudRenderer			*simul2DCloudRenderer;
			SimulPrecipitationRenderer		*simulPrecipitationRenderer;
			SimulAtmosphericsRenderer		*simulAtmosphericsRenderer;
			float							exposure;
			float							gamma;
			bool							use_buffer;
			float							exposure_multiplier;
			bool							show_rain;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif