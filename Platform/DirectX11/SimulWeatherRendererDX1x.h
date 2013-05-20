// Copyright (c) 2007-2011 Simul Software Ltd
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
#include "Simul/Platform/DirectX11/FramebufferCubemapDX1x.h"
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include "Simul/Graph/Meta/Group.h"
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
//! An implementation of \link simul::clouds::BaseWeatherRenderer BaseWeatherRenderer\endlink for DirectX 10 and 11
//! The DX10 switch is used
SIMUL_DIRECTX11_EXPORT_CLASS SimulWeatherRendererDX1x : public simul::clouds::BaseWeatherRenderer
{
public:
	SimulWeatherRendererDX1x(simul::clouds::Environment *env,bool usebuffer=true,bool tonemap=false,int w=256,int h=256,bool sky=true,bool clouds3d=true,bool clouds2d=true,bool rain=true);
	virtual ~SimulWeatherRendererDX1x();
	void SetScreenSize(int w,int h);
	//standard d3d object interface functions
	void RestoreDeviceObjects(void*);//ID3D1xDevice* pd3dDevice,IDXGISwapChain *swapChain);
	void RecompileShaders();
	void InvalidateDeviceObjects();
	bool Destroy();
	bool RenderSky(void*,bool buffered,bool is_cubemap);
	void RenderLateCloudLayer(void *context,bool );
	void RenderPrecipitation(void *context);
	void RenderLightning(void *context);
	bool RenderCubemap(void *context);
	void SaveCubemapToFile(const char *filename);
	void *GetCubemap();
	//! Perform the once-per-frame time update.
	void Update();
	//! Apply the view and projection matrices, once per frame.
	void SetMatrices(const D3DXMATRIX &viewmat,const D3DXMATRIX &projmat);
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

protected:
	// Keep copies of these matrices:
	D3DXMATRIX view;
	D3DXMATRIX proj;
	void UpdateSkyAndCloudHookup();
	IDXGISwapChain *pSwapChain;
	//! The size of the 2D buffer the sky is rendered to.
	int BufferWidth,BufferHeight;
	//! The size of the screen:
	int ScreenWidth,ScreenHeight;
	ID3D1xDevice*					m_pd3dDevice;
	
	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	ID3D1xEffect*						m_pTonemapEffect;
	ID3D1xEffectTechnique*				TonemapTechnique;
	ID3D1xEffectTechnique*				SkyOverStarsTechnique;
	ID3D1xEffectMatrixVariable*			worldViewProj;
	ID3D1xEffectShaderResourceVariable*	imageTexture;

	bool CreateBuffers();
	bool RenderBufferToScreen(ID3D1xShaderResourceView* texture,int w,int h,bool do_tonemap);
	simul::base::SmartPtr<class SimulSkyRendererDX1x> simulSkyRenderer;
	simul::base::SmartPtr<class SimulCloudRendererDX1x> simulCloudRenderer;
	simul::base::SmartPtr<class SimulPrecipitationRendererDX1x> simulPrecipitationRenderer;
	simul::base::SmartPtr<class SimulAtmosphericsRendererDX1x> simulAtmosphericsRenderer;
	simul::base::SmartPtr<class Simul2DCloudRendererDX11> simul2DCloudRenderer;
	simul::base::SmartPtr<class SimulLightningRendererDX11> simulLightningRenderer;
	FramebufferDX1x					framebuffer;
	FramebufferCubemapDX1x			framebuffer_cubemap;
	float							exposure;
	float							gamma;
	float exposure_multiplier;
	D3DXVECTOR3 cam_pos;
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif