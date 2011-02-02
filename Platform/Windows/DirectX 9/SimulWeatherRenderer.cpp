// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRenderer.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
#endif

#include "CreateDX9Effect.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRenderer.h"
#include "SimulLightningRenderer.h"
#include "SimulPrecipitationRenderer.h"
#include "Simul2DCloudRenderer.h"
#include "SimulSkyRenderer.h"
#include "SimulAtmosphericsRenderer.h"
#include "Simul/Base/Timer.h"
#include "Macros.h"
#include "Resources.h"
#define WRITE_PERFORMANCE_DATA

static D3DXMATRIX ident;
SimulWeatherRenderer::SimulWeatherRenderer(
			bool usebuffer,bool tonemap,int width,
			int height,bool sky,bool clouds3d,
			bool clouds2d,bool rain,bool colour_sky) :
	m_pBufferVertexDecl(NULL),
	m_pd3dDevice(NULL),
	m_pTonemapEffect(NULL),
	use_buffer(usebuffer),
	tone_map(tonemap),
	buffer_depth_texture(NULL),
	hdr_buffer_texture(NULL),
	ldr_buffer_texture(NULL),
	exposure(1.f),
	gamma(1.f/2.2f),
	simulSkyRenderer(NULL),
	simulCloudRenderer(NULL),
	simul2DCloudRenderer(NULL),
	simulPrecipitationRenderer(NULL),
	m_pLDRRenderTarget(NULL),
	m_pHDRRenderTarget(NULL),
	m_pBufferDepthSurface(NULL),
	BufferWidth(width),
	BufferHeight(height),
	timing(0.f),
	exposure_multiplier(1.f),
	RenderCloudsLate(false),
	show_rain(rain),
	AlwaysRenderCloudsLate(false),
	renderDepthBufferCallback(NULL)
{
	D3DXMatrixIdentity(&ident);
	show_sky=sky;
	if(sky)
	{
		simulSkyRenderer=new SimulSkyRenderer(colour_sky);
		baseSkyRenderer=simulSkyRenderer.get();
		AddChild(simulSkyRenderer.get());
	}
	EnableLayers(clouds3d,clouds2d);
	if(rain)
		simulPrecipitationRenderer=new SimulPrecipitationRenderer();

	simulAtmosphericsRenderer=new SimulAtmosphericsRenderer;
	ConnectInterfaces();
}

void SimulWeatherRenderer::EnableLayers(bool clouds3d,bool clouds2d)
{
	HRESULT hr=S_OK;
	BaseWeatherRenderer::EnableLayers(clouds3d,clouds2d);
	if(clouds3d&&simulCloudRenderer.get()==NULL)
	{
		simulCloudRenderer=new SimulCloudRenderer();
		baseCloudRenderer=simulCloudRenderer.get();
		AddChild(simulCloudRenderer.get());
		simulLightningRenderer=new SimulLightningRenderer(simulCloudRenderer->GetLightningRenderInterface());
		baseLightningRenderer=simulLightningRenderer.get();
		Restore3DCloudObjects();
		
	}
	if(clouds2d&&simul2DCloudRenderer.get()==NULL)
	{
		simul2DCloudRenderer=new Simul2DCloudRenderer();
		base2DCloudRenderer=simul2DCloudRenderer.get();
		Restore2DCloudObjects();
	}
	ConnectInterfaces();
}

void SimulWeatherRenderer::ConnectInterfaces()
{
	if(simulSkyRenderer)
	{
		if(simul2DCloudRenderer)
		{
			simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
			simul2DCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
		}
		if(simulCloudRenderer)
		{
			simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
			simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
			simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
		}
		if(simulAtmosphericsRenderer)
			simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
}

HRESULT SimulWeatherRenderer::Create(LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
		simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
	}
	if(simul2DCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
		simul2DCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
	}
	if(simulSkyRenderer)
	{
		if(simulCloudRenderer)
			simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
	}
	if(simulCloudRenderer&&simulSkyRenderer)
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	HRESULT hr=S_OK;
	return hr;
}


HRESULT SimulWeatherRenderer::Restore3DCloudObjects()
{
	HRESULT hr=S_OK;
	if(m_pd3dDevice)
	{
		if(simulCloudRenderer)
		{
			if(simulSkyRenderer)
				simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
			V_RETURN(simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
		}
		if(simulPrecipitationRenderer)
			V_RETURN(simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice));
		if(simulLightningRenderer)
			V_RETURN(simulLightningRenderer->RestoreDeviceObjects(m_pd3dDevice));
	}
	return hr;
}
HRESULT SimulWeatherRenderer::Restore2DCloudObjects()
{
	HRESULT hr=S_OK;
	if(m_pd3dDevice)
	{
		if(simul2DCloudRenderer)
			V_RETURN(simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	}
	return hr;
}
HRESULT SimulWeatherRenderer::RestoreDeviceObjects(LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr;
	if(!m_pTonemapEffect)
		V_RETURN(CreateDX9Effect(m_pd3dDevice,m_pTonemapEffect,"gamma.fx"));
	GammaTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_gamma");
	DirectTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_direct");
	SkyToScreenTechnique=m_pTonemapEffect->GetTechniqueByName("simul_sky_to_screen");
	
	CloudBlendTechnique	=m_pTonemapEffect->GetTechniqueByName("simul_cloud_blend");

	m_hExposure			=m_pTonemapEffect->GetParameterByName(NULL,"exposure");
	m_hGamma			=m_pTonemapEffect->GetParameterByName(NULL,"gamma");
	bufferTexture		=m_pTonemapEffect->GetParameterByName(NULL,"hdrTexture");
	V_RETURN(CreateBuffers());
	if(simulSkyRenderer)
		V_RETURN(simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice));
	V_RETURN(Restore3DCloudObjects());
	V_RETURN(Restore2DCloudObjects());
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(dev);

	UpdateSkyAndCloudHookup();
	return hr;
}

HRESULT SimulWeatherRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->InvalidateDeviceObjects();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();
	if(simulLightningRenderer)
		simulLightningRenderer->InvalidateDeviceObjects();
	SAFE_RELEASE(m_pBufferVertexDecl);
	if(m_pTonemapEffect)
        hr=m_pTonemapEffect->OnLostDevice();
	SAFE_RELEASE(m_pTonemapEffect);
	{
		SAFE_RELEASE(ldr_buffer_texture);
		SAFE_RELEASE(hdr_buffer_texture);
		SAFE_RELEASE(buffer_depth_texture);
	}
	SAFE_RELEASE(m_pLDRRenderTarget);
	SAFE_RELEASE(m_pHDRRenderTarget);
	SAFE_RELEASE(m_pBufferDepthSurface);
	return hr;
}

HRESULT SimulWeatherRenderer::Destroy()
{
	HRESULT hr=InvalidateDeviceObjects();
	if(simulSkyRenderer)
		simulSkyRenderer->Destroy();
	if(simulCloudRenderer)
		simulCloudRenderer->Destroy();
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->Destroy();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->Destroy();
	return hr;
}

SimulWeatherRenderer::~SimulWeatherRenderer()
{
	Destroy();
	SAFE_DELETE_SMARTPTR(simulSkyRenderer);
	SAFE_DELETE_SMARTPTR(simulCloudRenderer);
	SAFE_DELETE_SMARTPTR(simul2DCloudRenderer);
	SAFE_DELETE_SMARTPTR(simulPrecipitationRenderer);
	SAFE_DELETE_SMARTPTR(simulAtmosphericsRenderer);
	ClearChildren();
}
HRESULT SimulWeatherRenderer::IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat)
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

void SimulWeatherRenderer::EnableRain(bool e)
{
	show_rain=e;
}

void SimulWeatherRenderer::SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb)
{
	renderDepthBufferCallback=cb;
	AlwaysRenderCloudsLate=(cb!=NULL);
}

void SimulWeatherRenderer::SetBufferSize(int w,int h)
{
	BufferWidth=w;
	BufferHeight=h;
}


HRESULT SimulWeatherRenderer::CreateBuffers()
{
	HRESULT hr=S_OK;
#ifndef XBOX
	D3DFORMAT hdr_format=D3DFMT_A16B16G16R16F;
#else
	D3DFORMAT hdr_format=D3DFMT_LIN_A16B16G16R16F;
#endif
	hr=-1;//CanUse16BitFloats(m_pd3dDevice);

	if(hr!=S_OK)
#ifndef XBOX
		hdr_format=D3DFMT_A32B32G32R32F;
#else
		hdr_format=D3DFMT_LIN_A32B32G32R32F;
#endif

	{
		SAFE_RELEASE(hdr_buffer_texture);
		hr=(m_pd3dDevice->CreateTexture(	BufferWidth,
										BufferHeight,
										1,
										D3DUSAGE_RENDERTARGET,
										hdr_format,
										D3DPOOL_DEFAULT,
										&hdr_buffer_texture,
										NULL
									));
		SAFE_RELEASE(ldr_buffer_texture);
		hr=(m_pd3dDevice->CreateTexture(	BufferWidth,
											BufferHeight,
											1,
											D3DUSAGE_RENDERTARGET,
											D3DFMT_A8R8G8B8,
											D3DPOOL_DEFAULT,
											&ldr_buffer_texture,
											NULL
										));
		D3DFORMAT fmtDepthTex = D3DFMT_UNKNOWN;
		D3DFORMAT possibles[]={D3DFMT_D24S8,D3DFMT_D24FS8,D3DFMT_D32,D3DFMT_D24X8,D3DFMT_D16,D3DFMT_UNKNOWN};

		LPDIRECT3DSURFACE9 g_BackBuffer;
		m_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_BackBuffer);
		D3DSURFACE_DESC desc;
		g_BackBuffer->GetDesc(&desc);
		SAFE_RELEASE(g_BackBuffer);
		D3DDISPLAYMODE d3ddm;
	#ifdef XBOX
		if(FAILED(hr=m_pd3dDevice->GetDisplayMode( D3DADAPTER_DEFAULT, &d3ddm )))
		{
			return hr;
		}
	#else
		LPDIRECT3D9 d3d;
		m_pd3dDevice->GetDirect3D(&d3d);
		if(FAILED(hr=d3d->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm )))
		{
			return hr;
		}
	#endif
		for(int i=0;i<100;i++)
		{
			D3DFORMAT possible=possibles[i];
			if(possible==D3DFMT_UNKNOWN)
				break;
			hr=IsDepthFormatOk(possible,d3ddm.Format,desc.Format);
			if(SUCCEEDED(hr))
			{
				fmtDepthTex = possible;
			}
		}
		
		SAFE_RELEASE(buffer_depth_texture);
		LPDIRECT3DSURFACE9	m_pOldDepthSurface;
		hr=m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
		if(m_pOldDepthSurface)
			hr=m_pOldDepthSurface->GetDesc(&desc);
		SAFE_RELEASE(m_pOldDepthSurface);
		// Try creating a depth texture
		if(fmtDepthTex!=D3DFMT_UNKNOWN)
			hr=(m_pd3dDevice->CreateTexture(BufferWidth,
											BufferHeight,
											1,
											D3DUSAGE_DEPTHSTENCIL,
											desc.Format,
											D3DPOOL_DEFAULT,
											&buffer_depth_texture,
											NULL
										));
		SAFE_RELEASE(m_pBufferDepthSurface);
		if(buffer_depth_texture)
			buffer_depth_texture->GetSurfaceLevel(0,&m_pBufferDepthSurface);
	}
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  8, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pBufferVertexDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl);
		SAFE_RELEASE(m_pLDRRenderTarget)
		SAFE_RELEASE(m_pHDRRenderTarget)
		SAFE_RELEASE(m_pBufferDepthSurface)
		m_pHDRRenderTarget=MakeRenderTarget(hdr_buffer_texture);
		m_pLDRRenderTarget=MakeRenderTarget(ldr_buffer_texture);
	return hr;
}

LPDIRECT3DSURFACE9 SimulWeatherRenderer::MakeRenderTarget(const LPDIRECT3DTEXTURE9 pTexture)
{
	LPDIRECT3DSURFACE9 pRenderTarget;
#ifdef XBOX
	XGTEXTURE_DESC desc;
	XGGetTextureDesc( pTexture, 0, &desc );
	D3DSURFACE_PARAMETERS SurfaceParams = {0};
	//HANDLE handle=&SurfaceParams;
	HRESULT hr=m_pd3dDevice->CreateRenderTarget(
		desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &pRenderTarget,&SurfaceParams);
	if(hr!=S_OK)
		std::cerr<<"SimulWeatherRenderer::MakeRenderTarget - Failed to create render target!\n";
#else
	pTexture->GetSurfaceLevel(0,&pRenderTarget);
#endif
	return pRenderTarget;
}

HRESULT SimulWeatherRenderer::Render(bool is_cubemap)
{
	PIXBeginNamedEvent(0xFF888888,"SimulWeatherRenderer::Render");
	LPDIRECT3DSURFACE9	m_pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9	m_pOldDepthSurface=NULL;
	RenderCloudsLate=false;
	if(simulCloudRenderer)
		RenderCloudsLate=AlwaysRenderCloudsLate|simulCloudRenderer->IsCameraAboveCloudBase();
	D3DSURFACE_DESC desc;
	if(simulSkyRenderer)
	{
		static bool calc_occlusion=true;
		float cloud_occlusion=0;
		if(layer1&&simulCloudRenderer)
		{
			cloud_occlusion=simulCloudRenderer->GetSunOcclusion();
		}
		if(calc_occlusion)
			simulSkyRenderer->CalcSunOcclusion(cloud_occlusion);
	}
	HRESULT hr=S_OK;
	if(use_buffer&&!is_cubemap)
	{
		hdr_buffer_texture->GetLevelDesc(0,&desc);
		hr=m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget);
		m_pOldRenderTarget->GetDesc(&desc);
		hr=m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
		hr=m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget);
		if(m_pBufferDepthSurface)
			m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface);
		static float depth_start=1.f;
		hr=m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,1.f, 0L);
	}
	if(simulSkyRenderer&&show_sky)
		hr=simulSkyRenderer->Render();
	if(simul2DCloudRenderer&&layer2)
		hr=simul2DCloudRenderer->Render();
	if(simulCloudRenderer&&layer1&&(!RenderCloudsLate||is_cubemap))
	{
		if(renderDepthBufferCallback)
			renderDepthBufferCallback->Render();
//if(!is_cubemap)
		hr=simulCloudRenderer->Render(is_cubemap);
	}
	static float depth_start=1.f;
	if(use_buffer&&!is_cubemap)
	{
#ifdef XBOX
		m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
#endif
		// here we're going to render to the LDR buffer. If tone_map is disabled, we don't do this:

		if(tone_map)
		{
			m_pd3dDevice->SetRenderTarget(0,m_pLDRRenderTarget);
			//ldr_buffer_texture->GetLevelDesc(0,&desc);
			hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start,0L);
			simulSkyRenderer->RenderPointStars();
			simulSkyRenderer->RenderSun();
			simulSkyRenderer->RenderPlanets();
			// using gamma, render to the 8\8\8 buffer:
			RenderBufferToScreen(hdr_buffer_texture,BufferWidth,BufferHeight,true,true);
			m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
			if(m_pOldDepthSurface)
				m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
		//	m_pOldRenderTarget->GetDesc(&desc);
			RenderBufferToScreen(ldr_buffer_texture,desc.Width,desc.Height,false);
		}
		else
		{
			m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
			if(m_pOldDepthSurface)
				m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
			if(simulSkyRenderer&&show_sky)
			{
				simulSkyRenderer->RenderPointStars();
				simulSkyRenderer->RenderSun();
				simulSkyRenderer->RenderPlanets();
			}
			RenderBufferToScreen(hdr_buffer_texture,desc.Width,desc.Height,true,true);
		}
	}
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);

	return hr;
}

HRESULT SimulWeatherRenderer::RenderLightning()
{
	if(simulCloudRenderer&&simulLightningRenderer&&layer1)
		return simulLightningRenderer->Render();
	return S_OK;
}

HRESULT SimulWeatherRenderer::RenderPrecipitation()
{
	if(simulPrecipitationRenderer&&layer1) 
		return simulPrecipitationRenderer->Render();
	return S_OK;
}

HRESULT SimulWeatherRenderer::RenderFlares()
{
	simulSkyRenderer->RenderFlare(exposure);
	return S_OK;
}

HRESULT SimulWeatherRenderer::RenderLateCloudLayer()
{
	HRESULT hr=S_OK;
	if(!RenderCloudsLate||!layer1)
		return hr;
	
	LPDIRECT3DSURFACE9	m_pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9	m_pOldDepthSurface=NULL;
	if(use_buffer)
	{
		hr=m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget);
		hr=m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget);
		hr=m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
		if(m_pBufferDepthSurface)
			m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface);
		static float depth_start=1.f;
		hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start,0L);
	}
	if(renderDepthBufferCallback)
		renderDepthBufferCallback->Render();
	if(simulCloudRenderer&&layer1)
	{
		PIXWrapper(D3DCOLOR_RGBA(255,0,0,255),"CLOUDS")
		{
			hr=simulCloudRenderer->Render();
		}
	}
	//static float depth_start=1.f;
	if(use_buffer)
	{
	#ifdef XBOX
		m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
	#endif
		D3DSURFACE_DESC desc;
		m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
		m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
		m_pOldRenderTarget->GetDesc(&desc);
		RenderBufferToScreen(hdr_buffer_texture,desc.Width,desc.Height,true,true);
	}
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
	return hr;
}

HRESULT SimulWeatherRenderer::RenderBufferToScreen(LPDIRECT3DTEXTURE9 texture,int width,int height,
												   bool use_shader,bool blend)
{
	HRESULT hr=S_OK;
#ifdef XBOX
	float x=-1.f,y=1.f;
	float w=2.f;
	float h=-2.f;
	struct Vertext
	{
		float x,y;
		float tx,ty;
	};
	Vertext vertices[4] =
	{
		{x,			y,			0	,0},
		{x+w,		y,			1.f	,0},
		{x+w,		y+h,		1.f	,1.f},
		{x,			y+h,		0	,1.f},
	};
#else
	float x=0,y=0;
	struct Vertext
	{
		float x,y,z,h;
		float tx,ty;
	};
	Vertext vertices[4] =
	{
		{x,			y,			1,	1, 0	,0},
		{x+width,	y,			1,	1, 1.f	,0},
		{x+width,	y+height,	1,	1, 1.f	,1.f},
		{x,			y+height,	1,	1, 0	,1.f},
	};
#endif
	//m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

	//m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	//m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);

	m_pTonemapEffect->SetTexture(bufferTexture,texture);
	if(blend)
		m_pTonemapEffect->SetTechnique(CloudBlendTechnique);
	else if(tone_map)
		m_pTonemapEffect->SetTechnique(GammaTechnique);
	else
		m_pTonemapEffect->SetTechnique(DirectTechnique);

    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetPixelShader( NULL );

	m_pd3dDevice->SetVertexDeclaration(m_pBufferVertexDecl);
	if(use_shader)
	{
		if(tone_map)
		{
			hr=m_pTonemapEffect->SetFloat(m_hExposure,exposure*exposure_multiplier);
			hr=m_pTonemapEffect->SetFloat(m_hGamma,gamma);
		}
		else
		{
			hr=m_pTonemapEffect->SetFloat(m_hExposure,1.f);
			hr=m_pTonemapEffect->SetFloat(m_hGamma,1.f);
		}
		UINT passes=1;
		hr=m_pTonemapEffect->Begin(&passes,0);
		hr=m_pTonemapEffect->BeginPass(0);
	}
	else
	{
#ifndef XBOX
		m_pd3dDevice->SetTransform(D3DTS_VIEW,&ident);
		m_pd3dDevice->SetTransform(D3DTS_WORLD,&ident);
		m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&ident);
#endif
		//m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,D3DTOP_ADD );
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		m_pd3dDevice->SetTexture(0,texture);
	}
	m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertext) );
	if(use_shader)
	{
		hr=m_pTonemapEffect->EndPass();
		hr=m_pTonemapEffect->End();
	}
	//m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	//m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	//m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	return hr;
}

#ifdef XBOX
void SimulWeatherRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(v,p);
	if(simulCloudRenderer)
		simulCloudRenderer->SetMatrices(v,p);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->SetMatrices(v,p);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(v,p);
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(v,p);
}
#endif

void SimulWeatherRenderer::UpdateSkyAndCloudHookup()
{
	if(!simulSkyRenderer)
		return;
	LPDIRECT3DBASETEXTURE9 l1,l2,i1,i2;
	simulSkyRenderer->GetLossAndInscatterTextures(&l1,&l2,&i1,&i2);
	if(layer1&&simulCloudRenderer)
	{
		simulCloudRenderer->SetLossTextures(l1,l2);
		simulCloudRenderer->SetInscatterTextures(i1,i2);
		simulCloudRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
		simulCloudRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
	}
}

void SimulWeatherRenderer::Update(float dt)
{
	static bool pause=false;
    if(!pause)
	{
		if(simulSkyRenderer)
		{
			simulSkyRenderer->Update(dt);
			LPDIRECT3DBASETEXTURE9 l1,l2,i1,i2;
			simulSkyRenderer->GetLossAndInscatterTextures(&l1,&l2,&i1,&i2);
			if(simulAtmosphericsRenderer)
			{
				simulAtmosphericsRenderer->SetLossTextures(l1,l2);
				simulAtmosphericsRenderer->SetInscatterTextures(i1,i2);
				simulAtmosphericsRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
				simulAtmosphericsRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
			}
		}
		// Do this AFTER sky update, to catch any changes:
		UpdateSkyAndCloudHookup();
		static simul::base::Timer timer;
		timer.StartTime();
		if(simulCloudRenderer)
			simulCloudRenderer->Update(dt);
		timer.FinishTime();
		timing=timer.Time;
		if(simul2DCloudRenderer)
			simul2DCloudRenderer->Update(dt);
		if(simulPrecipitationRenderer)
		{
			simulPrecipitationRenderer->Update(dt);
			if(layer1&&simulCloudRenderer)
			{
				simulPrecipitationRenderer->SetWind(simulCloudRenderer->GetWindSpeed(),simulCloudRenderer->GetWindHeadingDegrees());
				simulPrecipitationRenderer->SetIntensity(simulCloudRenderer->GetPrecipitationIntensity());
				float rts=simulCloudRenderer->GetRainToSnow();
				if(rts<0.5f)
					simulPrecipitationRenderer->ApplyDefaultRainSettings();
				else
					simulPrecipitationRenderer->ApplyDefaultSnowSettings();
			}
			else
			{
				simulPrecipitationRenderer->SetIntensity(0.f);
				simulPrecipitationRenderer->SetWind(0,0);
			}
			if(simulSkyRenderer)
			{
				simul::sky::float4 l=simulSkyRenderer->GetLightColour();
				simulPrecipitationRenderer->SetLightColour((const float*)(l));
			}
		}
	}
}

SimulSkyRenderer *SimulWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer.get();
}

SimulCloudRenderer *SimulWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer.get();
}

Simul2DCloudRenderer *SimulWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer.get();
}

SimulPrecipitationRenderer *SimulWeatherRenderer::GetPrecipitationRenderer()
{
	return simulPrecipitationRenderer.get();
}

SimulAtmosphericsRenderer *SimulWeatherRenderer::GetAtmosphericsRenderer()
{
	return simulAtmosphericsRenderer.get();
}

float SimulWeatherRenderer::GetTiming() const
{
	return timing;
}

float SimulWeatherRenderer::GetTotalBrightness() const
{
	return exposure*exposure_multiplier;
}

const TCHAR *SimulWeatherRenderer::GetDebugText() const
{
	static TCHAR debug_text[256];
	if(simulCloudRenderer)
		stprintf_s(debug_text,256,_T("%s"),simulCloudRenderer->GetDebugText());
	return debug_text;
}
