// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Platform/Windows/DirectX 1x/MacrosDx1x.h"
#include <d3dx9.h>
#ifdef X10
	#include <D3D1x.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
#endif
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
typedef long HRESULT;
#ifdef DX10
#define SimulWeatherRendererDX1x SimulWeatherRendererDX10
#else
#define SimulWeatherRendererDX1x SimulWeatherRendererDX11
#endif
class RenderDepthBufferCallback
{
public:
	virtual void Render()=0;
};

class SimulWeatherRendererDX1x : public simul::clouds::BaseWeatherRenderer, public simul::graph::meta::Group
{
public:
	SimulWeatherRendererDX1x(bool usebuffer=true,bool tonemap=false,int w=256,int h=256,bool sky=true,bool clouds3d=true,bool clouds2d=true,bool rain=true);
	virtual ~SimulWeatherRendererDX1x();
	//standard d3d object interface functions
	HRESULT RestoreDeviceObjects(ID3D1xDevice* pd3dDevice,IDXGISwapChain *swapChain);
	HRESULT InvalidateDeviceObjects();
	HRESULT Destroy();
	HRESULT Render();

	//! Enable or disable the 3d and 2d cloud layers.
	void Enable(bool sky,bool clouds3d,bool clouds2d,bool rain);
	//! Enable or disable the sky.
	void SetShowSky(bool s)
	{
		show_sky=s;
	}
	//! Enable or disable the clouds.
	void SetShowClouds(bool s)
	{
		show_3d_clouds=s;
	}
	//! Perform the once-per-frame time update.
	void Update(float dt);
	//! Apply the view and projection matrices, once per frame.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
	//! Set the exposure, if we're using an hdr shader to render the sky buffer.
	void SetExposure(float ex){exposure=ex;}

	//! Get a pointer to the sky renderer owned by this class instance.
	class SimulSkyRendererDX1x *GetSkyRenderer();
	//! Get a pointer to the 3d cloud renderer owned by this class instance.
	class SimulCloudRendererDX1x *GetCloudRenderer();
	//! Get a pointer to the 2d cloud renderer owned by this class instance.
	class Simul2DCloudRenderer *Get2DCloudRenderer();
	//! Get a pointer to the rain renderer owned by this class instance.
	class SimulPrecipitationRenderer *GetPrecipitationRenderer();
	//! Get a pointer to the atmospherics renderer owned by this class instance.
	class SimulAtmosphericsRenderer *GetAtmosphericsRenderer();
	//! Get the current debug text as a c-string pointer.
	const char *GetDebugText() const;
	const wchar_t *GetWDebugText() const;
	//! Get timing value.
	float GetTiming() const;
	//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
	void SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb);
protected:
	IDXGISwapChain *pSwapChain;
	//! The size of the 2D buffer the sky is rendered to.
	int BufferWidth,BufferHeight;
	ID3D1xDevice*					m_pd3dDevice;
	HRESULT CreateBuffers();
	HRESULT RenderBufferToScreen(ID3D1xShaderResourceView* texture,int w,int h,bool do_tonemap);
	simul::base::SmartPtr<class SimulSkyRendererDX1x> simulSkyRenderer;
	simul::base::SmartPtr<class SimulCloudRendererDX1x> simulCloudRenderer;
	//simul::base::SmartPtr<class Simul2DCloudRenderer> *simul2DCloudRenderer;
	//simul::base::SmartPtr<class SimulPrecipitationRenderer> *simulPrecipitationRenderer;
	//simul::base::SmartPtr<class SimulAtmosphericsRenderer> *simulAtmosphericsRenderer;
	float							exposure;
	float							gamma;
	bool show_3d_clouds,layer2;
	bool show_sky;
	bool show_rain;
	float timing;
	float exposure_multiplier;
	void ConnectInterfaces();
};