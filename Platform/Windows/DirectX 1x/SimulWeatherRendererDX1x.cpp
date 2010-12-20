// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRendererDX1x.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRendererDX1x.h"

#include <tchar.h>
#include <dxerr.h>
#include <string>

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Base/StringToWString.h"
#include "SimulSkyRendererDX1x.h"

#include "SimulCloudRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"

SimulWeatherRendererDX1x::SimulWeatherRendererDX1x(
		bool usebuffer,bool tonemap,int w,int h,bool sky,bool clouds3d,bool clouds2d,bool rain) :
	m_pd3dDevice(NULL),
	simulSkyRenderer(NULL),
	simulCloudRenderer(NULL),
	show_3d_clouds(clouds3d),
	layer2(clouds2d),
	BufferWidth(w),
	BufferHeight(h),
	timing(0.f),
	exposure_multiplier(1.f),
	show_sky(sky),
	show_rain(rain)
{
	if(show_sky)
	{
		simulSkyRenderer=new SimulSkyRendererDX1x();
		baseSkyRenderer=simulSkyRenderer.get();
		AddChild(simulSkyRenderer.get());
	}
	if(show_3d_clouds)
	{
		simulCloudRenderer=new SimulCloudRendererDX1x();
		baseCloudRenderer=simulCloudRenderer.get();
		AddChild(simulCloudRenderer.get());
	}
/*	if(clouds2d)
		simul2DCloudRenderer=new Simul2DCloudRenderer();
	if(rain)
		simulPrecipitationRenderer=new SimulPrecipitationRenderer();
	
	simulAtmosphericsRenderer=new SimulAtmosphericsRenderer;*/
	ConnectInterfaces();
}
void SimulWeatherRendererDX1x::ConnectInterfaces()
{
	if(simulCloudRenderer.get()&&simulSkyRenderer.get())
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
		simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
		simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
	}
/*	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
		simul2DCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
	}*/
	if(simulSkyRenderer)
	{
	//	if(simulCloudRenderer)
//			simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	//if(simulAtmosphericsRenderer&&simulSkyRenderer)
	//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	if(simulCloudRenderer)
	{
		if(simulSkyRenderer)
			simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
	}
	/*if(simul2DCloudRenderer)
		V_RETURN(simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	if(simulPrecipitationRenderer)
		V_RETURN(simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice));
	
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(dev);*/
}

HRESULT SimulWeatherRendererDX1x::RestoreDeviceObjects(ID3D1xDevice* dev,IDXGISwapChain *swapChain)
{
	m_pd3dDevice=dev;
	pSwapChain=swapChain;
	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
		simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
	}
/*	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
		simul2DCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
	}*/
	if(simulSkyRenderer)
	{
	//	if(simulCloudRenderer)
//			simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	//if(simulAtmosphericsRenderer&&simulSkyRenderer)
	//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	HRESULT hr=S_OK;
	//V_RETURN(CreateBuffers());
	if(simulSkyRenderer)
		V_RETURN(simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice));
	if(simulCloudRenderer)
	{
		if(simulSkyRenderer)
			simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
		V_RETURN(simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	}
	/*if(simul2DCloudRenderer)
		V_RETURN(simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	if(simulPrecipitationRenderer)
		V_RETURN(simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice));
	
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(dev);*/
	return hr;
}

HRESULT SimulWeatherRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	/*if(simul2DCloudRenderer)
		simul2DCloudRenderer->InvalidateDeviceObjects();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();*/
//	if(m_pTonemapEffect)
//        hr=m_pTonemapEffect->OnLostDevice();
	return hr;
}

HRESULT SimulWeatherRendererDX1x::Destroy()
{
	HRESULT hr=S_OK;
	InvalidateDeviceObjects();
	if(simulSkyRenderer.get())
		simulSkyRenderer->Destroy();
	if(simulCloudRenderer.get())
		simulCloudRenderer->Destroy();
	RemoveChild(simulSkyRenderer.get());
	simulSkyRenderer=NULL;
	RemoveChild(simulCloudRenderer.get());
	simulCloudRenderer=NULL;
	/*if(simul2DCloudRenderer)
		simul2DCloudRenderer->Destroy();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->Destroy();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();*/
	return hr;
}

SimulWeatherRendererDX1x::~SimulWeatherRendererDX1x()
{
	Destroy();
/*	SAFE_DELETE(simul2DCloudRenderer);
	SAFE_DELETE(simulPrecipitationRenderer);
	SAFE_DELETE(simulAtmosphericsRenderer);*/
}

/*HRESULT SimulWeatherRendererDX1x::IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat)
{
	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return hr;

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return hr;

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);

    return hr;
}
*/

HRESULT SimulWeatherRendererDX1x::Render()
{
	if(simulSkyRenderer)
	{
		float cloud_occlusion=0;
		if(show_3d_clouds&&simulCloudRenderer)
		{
			cloud_occlusion=simulCloudRenderer->GetSunOcclusion();
		}
		simulSkyRenderer->CalcSunOcclusion(cloud_occlusion);
	}
	HRESULT hr=S_OK;
	if(simulSkyRenderer)
		hr=simulSkyRenderer->Render();
	if(simulCloudRenderer)
		hr=simulCloudRenderer->Render();
	return hr;
}

void SimulWeatherRendererDX1x::Enable(bool sky,bool clouds3d,bool clouds2d,bool rain)
{
	show_3d_clouds=clouds3d;
	layer2=clouds2d;
	show_sky=sky;
	show_rain=rain;
}
void SimulWeatherRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(v,p);
	if(simulCloudRenderer)
		simulCloudRenderer->SetMatrices(v,p);
/*	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(w,v,p);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->SetMatrices(w,v,p);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(w,v,p);*/
}

void SimulWeatherRendererDX1x::Update(float dt)
{
	static bool pause=false;
    if(!pause)
	{
		if(simulSkyRenderer)
		{
			simulSkyRenderer->Update(dt);
			if(simulCloudRenderer)
			{
				simulCloudRenderer->SetLossTextures(simulSkyRenderer->GetLossTexture1(),
					simulSkyRenderer->GetLossTexture2());
				simulCloudRenderer->SetInscatterTextures(simulSkyRenderer->GetInscatterTexture1(),
					simulSkyRenderer->GetInscatterTexture2());
				simulCloudRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
				simulCloudRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
			}
		/*	if(simulAtmosphericsRenderer)
			{
				if(simulCloudRenderer)
					simulAtmosphericsRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
				simulAtmosphericsRenderer->SetLossTextures(simulSkyRenderer->GetLossTexture1(),
					simulSkyRenderer->GetLossTexture2());
				simulAtmosphericsRenderer->SetInscatterTextures(simulSkyRenderer->GetInscatterTexture1(),
					simulSkyRenderer->GetInscatterTexture2());
				simulAtmosphericsRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
			}*/
		}
		if(simulCloudRenderer)
		{
			simulCloudRenderer->Update(dt);
		}
/*		if(simul2DCloudRenderer)
			simul2DCloudRenderer->Update(dt);
		if(simulPrecipitationRenderer)
		{
			simulPrecipitationRenderer->Update(dt);
			if(simulCloudRenderer)
				simulPrecipitationRenderer->SetIntensity(simulCloudRenderer->GetPrecipitationIntensity());
			if(simulSkyRenderer)
			{
				simul::sky::float4 l=simulSkyRenderer->GetSkyInterface()->GetSunlightColour(0);
				simulPrecipitationRenderer->SetLightColour((const float*)(l));
			}
		}*/
	}
}

SimulSkyRendererDX1x *SimulWeatherRendererDX1x::GetSkyRenderer()
{
	return simulSkyRenderer.get();
}

SimulCloudRendererDX1x *SimulWeatherRendererDX1x::GetCloudRenderer()
{
	return simulCloudRenderer.get();
}

Simul2DCloudRenderer *SimulWeatherRendererDX1x::Get2DCloudRenderer()
{
	return NULL;
}

SimulPrecipitationRenderer *SimulWeatherRendererDX1x::GetPrecipitationRenderer()
{
	if(show_rain)
		return NULL;
	else return NULL;
}

SimulAtmosphericsRenderer *SimulWeatherRendererDX1x::GetAtmosphericsRenderer()
{
	return NULL;
}

const char *SimulWeatherRendererDX1x::GetDebugText() const
{
	static char debug_text[256];
	if(simulSkyRenderer)
		sprintf_s(debug_text,256,"%s",simulSkyRenderer->GetDebugText());
//		sprintf_s(debug_text,256,"%s",simulCloudRenderer->GetDebugText());
//	if(simulCloudRenderer)
		//sprintf_s(debug_text,256,"TIME %2.2g ms\n%s",timing,simulCloudRenderer->GetDebugText());
//		sprintf_s(debug_text,256,"%s",simulCloudRenderer->GetDebugText());
	return debug_text;
}
const wchar_t *SimulWeatherRendererDX1x::GetWDebugText() const
{
	static std::wstring wstr;
	wstr=simul::base::StringToWString(GetDebugText());
	return wstr.c_str();
}

float SimulWeatherRendererDX1x::GetTiming() const
{
	return timing;
}

//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
void SimulWeatherRendererDX1x::SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb)
{
}