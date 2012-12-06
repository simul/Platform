// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <tchar.h>
#include "Simul/Platform/DirectX1x/MacrosDX1x.h"
#include "Simul/Platform/DirectX1x/Export.h"
#include "Simul/Platform/DirectX1x/FramebufferDX1x.h"
#include "Simul/Platform/DirectX1x/FramebufferCubemapDX1x.h"
#include <d3dx9.h>
#ifdef DX10
	#include <D3D10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
#endif
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
SIMUL_DIRECTX1x_EXPORT_CLASS SimulWeatherRendererDX1x : public simul::clouds::BaseWeatherRenderer
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
	bool RenderSky(bool buffered,bool is_cubemap);
	bool RenderLateCloudLayer(bool );
	bool RenderCubemap();
	void *GetCubemap();
	//! Perform the once-per-frame time update.
	void Update(float dt);
	//! Apply the view and projection matrices, once per frame.
	void SetMatrices(const D3DXMATRIX &viewmat,const D3DXMATRIX &projmat);
	//! Set the exposure, if we're using an hdr shader to render the sky buffer.
	void SetExposure(float ex){exposure=ex;}

	//! Get a pointer to the sky renderer owned by this class instance.
	class SimulSkyRendererDX1x *GetSkyRenderer();
	//! Get a pointer to the 3d cloud renderer owned by this class instance.
	class SimulCloudRendererDX1x *GetCloudRenderer();
	//! Get a pointer to the 2d cloud renderer owned by this class instance.
	class Simul2DCloudRenderer *Get2DCloudRenderer();
	//! Get the current debug text as a c-string pointer.
	const char *GetDebugText() const;
	//! Get timing value.
	float GetTiming() const;
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
	ID3D1xDeviceContext *			m_pImmediateContext;

	bool CreateBuffers();
	bool RenderBufferToScreen(ID3D1xShaderResourceView* texture,int w,int h,bool do_tonemap);
	simul::base::SmartPtr<class SimulSkyRendererDX1x> simulSkyRenderer;
	simul::base::SmartPtr<class SimulCloudRendererDX1x> simulCloudRenderer;
	simul::base::SmartPtr<class SimulAtmosphericsRendererDX1x> simulAtmosphericsRenderer;
	//simul::base::SmartPtr<class Simul2DCloudRenderer> *simul2DCloudRenderer;
	//simul::base::SmartPtr<class SimulPrecipitationRenderer> *simulPrecipitationRenderer;
	//simul::base::SmartPtr<class SimulAtmosphericsRenderer> *simulAtmosphericsRenderer;
	FramebufferDX1x					framebuffer;
	FramebufferCubemapDX1x			framebuffer_cubemap;
	float							exposure;
	float							gamma;
	float timing;
	float exposure_multiplier;
	void ConnectInterfaces();
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif