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
#include "Simul/Platform/DirectX11/SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
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
		struct TwoResFramebuffer:public simul::crossplatform::TwoResFramebuffer
		{
			TwoResFramebuffer();
			~TwoResFramebuffer();
			crossplatform::BaseFramebuffer *GetLowResFarFramebuffer()
			{
				return lowResFarFramebufferDx11;
			}
			crossplatform::BaseFramebuffer *GetLowResNearFramebuffer()
			{
				return lowResNearFramebufferDx11;
			}
			crossplatform::BaseFramebuffer *GetHiResFarFramebuffer()
			{
				return hiResFarFramebufferDx11;
			}
			crossplatform::BaseFramebuffer *GetHiResNearFramebuffer()
			{
				return hiResNearFramebufferDx11;
			}
			void ActivateHiRes(crossplatform::DeviceContext &deviceContext);
			void DeactivateHiRes(crossplatform::DeviceContext &deviceContext);
			void ActivateLowRes(crossplatform::DeviceContext &deviceContext);
			void DeactivateLowRes(crossplatform::DeviceContext &deviceContext);
			crossplatform::BaseFramebuffer	*lowResFarFramebufferDx11;
			crossplatform::BaseFramebuffer	*lowResNearFramebufferDx11;
			crossplatform::BaseFramebuffer	*hiResFarFramebufferDx11;
			crossplatform::BaseFramebuffer	*hiResNearFramebufferDx11;
			void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			void InvalidateDeviceObjects();
			void SetDimensions(int w,int h,int downscale,int hiResDownscale);
			void GetDimensions(int &w,int &h,int &downscale,int &hiResDownscale);
		protected:
			void SaveOldRTs(crossplatform::DeviceContext &deviceContext);
			void RestoreRTs(crossplatform::DeviceContext &deviceContext);
			unsigned numOldViewports;
			ID3D11RenderTargetView*				m_pOldRenderTargets[2];
			ID3D11DepthStencilView*				m_pOldDepthSurface;
			D3D11_VIEWPORT						m_OldViewports[16];
		};
		/// An implementation of \link simul::clouds::BaseWeatherRenderer BaseWeatherRenderer\endlink for DirectX 11
		SIMUL_DIRECTX11_EXPORT_CLASS SimulWeatherRendererDX11 : public simul::clouds::BaseWeatherRenderer
		{
		public:
			SimulWeatherRendererDX11(simul::clouds::Environment *env,simul::base::MemoryInterface *mem);
			virtual ~SimulWeatherRendererDX11();
			void SetScreenSize(int view_id,int w,int h);
			//standard d3d object interface functions
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void RecompileShaders();
			void InvalidateDeviceObjects();
			bool Destroy();
	
			void RenderLightning(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depth_tex,simul::sky::float4 depthViewportXYWH,crossplatform::Texture *low_res_depth_tex);
			void SaveCubemapToFile(crossplatform::RenderPlatform *renderPlatform,const char *filename,float exposure,float gamma);
			//! Set the exposure, if we're using an hdr shader to render the sky buffer.
			void SetExposure(float ex){exposure=ex;}

			//! Get a pointer to the sky renderer owned by this class instance.
			class SimulSkyRendererDX1x *GetSkyRenderer();
			//! Get a pointer to the 3d cloud renderer owned by this class instance.
			class SimulCloudRendererDX1x *GetCloudRenderer();
			//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
			void SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb);
			crossplatform::Texture *GetCloudDepthTexture(int view_id);

		protected:
			simul::base::MemoryInterface	*memoryInterface;
			// Keep copies of these matrices:
		//	simul::math::Matrix4x4 view;
		//	simul::math::Matrix4x4 proj;
			IDXGISwapChain								*pSwapChain;
			ID3D11Device								*m_pd3dDevice;

			bool CreateBuffers();
			bool RenderBufferToScreen(ID3D11ShaderResourceView* texture,int w,int h,bool do_tonemap);
			class SimulSkyRendererDX1x					*simulSkyRenderer;
			class SimulCloudRendererDX1x				*simulCloudRenderer;
			simul::crossplatform::TwoResFramebuffer *	GetFramebuffer(int view_id);
			float										exposure;
			float										gamma;
			float										exposure_multiplier;
			//simul::math::Vector3						cam_pos;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
