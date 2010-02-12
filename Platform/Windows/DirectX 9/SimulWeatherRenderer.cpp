// Copyright (c) 2007-2009 Simul Software Ltd
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
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
#else
	#include <tchar.h>
	#include <dxerr9.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#endif
#include "CreateDX9Effect.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRenderer.h"
#include "SimulPrecipitationRenderer.h"
#include "Simul2DCloudRenderer.h"
#include "SimulSkyRenderer.h"
#include "SimulAtmosphericsRenderer.h"
#include "Simul/Base/Timer.h"
#include "Macros.h"
#include "Resources.h"

SimulWeatherRenderer::SimulWeatherRenderer(
	bool usebuffer,bool tonemap,int width,
	int height,bool sky,bool clouds3d,
	bool clouds2d,bool rain,
	bool always_render_clouds_late) :
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
	layer1(true),
	layer2(true),
	BufferWidth(width),
	BufferHeight(height),
	timing(0.f),
	exposure_multiplier(1.f),
	gamma_resource_id(0),
	RenderCloudsLate(false),
	AlwaysRenderCloudsLate(always_render_clouds_late),
	renderDepthBufferCallback(NULL)
{
	if(sky)
		simulSkyRenderer=new SimulSkyRenderer();
	if(clouds3d)
		simulCloudRenderer=new SimulCloudRenderer();
	if(clouds2d)
		simul2DCloudRenderer=new Simul2DCloudRenderer();
	if(rain)
		simulPrecipitationRenderer=new SimulPrecipitationRenderer();
	
	simulAtmosphericsRenderer=new SimulAtmosphericsRenderer;
}

HRESULT SimulWeatherRenderer::Create(LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
		simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
	}
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
		simul2DCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
		if(simulCloudRenderer)
			simul2DCloudRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	if(simulSkyRenderer)
	{
		if(simulCloudRenderer)
			simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	if(simulAtmosphericsRenderer&&simulSkyRenderer)
		simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	HRESULT hr=S_OK;
	return hr;
}

HRESULT SimulWeatherRenderer::RestoreDeviceObjects(LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr;
	if(!m_pTonemapEffect)
		V_RETURN(CreateDX9Effect(m_pd3dDevice,m_pTonemapEffect,"gamma.fx"));
	GammaTechnique=m_pTonemapEffect->GetTechniqueByName("simul_gamma");
	CloudBlendTechnique=m_pTonemapEffect->GetTechniqueByName("simul_cloud_blend");

	m_hExposure=m_pTonemapEffect->GetParameterByName(NULL,"exposure");
	m_hGamma=m_pTonemapEffect->GetParameterByName(NULL,"gamma");
	bufferTexture=m_pTonemapEffect->GetParameterByName(NULL,"hdrTexture");
	V_RETURN(CreateBuffers());
	if(simulSkyRenderer)
		V_RETURN(simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice));
	if(simulCloudRenderer)
	{
		if(simulSkyRenderer)
			simulCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
		V_RETURN(simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	}
	if(simul2DCloudRenderer)
		V_RETURN(simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	if(simulPrecipitationRenderer)
		V_RETURN(simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice));
	
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(dev);
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
	SAFE_RELEASE(m_pBufferVertexDecl);
	if(m_pTonemapEffect)
        hr=m_pTonemapEffect->OnLostDevice();
	SAFE_RELEASE(m_pTonemapEffect);
	SAFE_RELEASE(ldr_buffer_texture);
	SAFE_RELEASE(hdr_buffer_texture);
	SAFE_RELEASE(buffer_depth_texture);
	return hr;
}

HRESULT SimulWeatherRenderer::Destroy()
{
	HRESULT hr=S_OK;
	if(simulSkyRenderer)
		simulSkyRenderer->Destroy();
	if(simulCloudRenderer)
		simulCloudRenderer->Destroy();
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->Destroy();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->Destroy();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();
	SAFE_RELEASE(m_pBufferVertexDecl);
	if(m_pTonemapEffect)
        hr=m_pTonemapEffect->OnLostDevice();
	SAFE_RELEASE(m_pTonemapEffect);
	SAFE_RELEASE(ldr_buffer_texture);
	SAFE_RELEASE(hdr_buffer_texture);
	SAFE_RELEASE(buffer_depth_texture);
	return hr;
}

SimulWeatherRenderer::~SimulWeatherRenderer()
{
	Destroy();
	SAFE_DELETE(simulSkyRenderer);
	SAFE_DELETE(simulCloudRenderer);
	SAFE_DELETE(simul2DCloudRenderer);
	SAFE_DELETE(simulPrecipitationRenderer);
	SAFE_DELETE(simulAtmosphericsRenderer);
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

HRESULT SimulWeatherRenderer::CreateBuffers()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(hdr_buffer_texture);
#ifndef XBOX
	D3DFORMAT hdr_format=D3DFMT_A16B16G16R16F;
#else
	D3DFORMAT hdr_format=D3DFMT_LIN_A16B16G16R16F;
#endif
	hr=CanUse16BitFloats(m_pd3dDevice);

	if(hr!=S_OK)
#ifndef XBOX
		hdr_format=D3DFMT_A32B32G32R32F;
#else
		hdr_format=D3DFMT_LIN_A32B32G32R32F;
#endif

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

HRESULT SimulWeatherRenderer::Render()
{
	RenderCloudsLate=false;
	if(simulCloudRenderer)
		RenderCloudsLate=AlwaysRenderCloudsLate|simulCloudRenderer->IsCameraAboveCloudBase();
	PIXBeginNamedEvent(0,"Render Weather");
	if(simulSkyRenderer)
	{
		float cloud_occlusion=0;
		if(layer1&&simulCloudRenderer)
		{
			cloud_occlusion=simulCloudRenderer->GetSunOcclusion();
		}
		simulSkyRenderer->CalcSunOcclusion(cloud_occlusion);
	}
	HRESULT hr=S_OK;
	m_pHDRRenderTarget		=NULL;
	m_pBufferDepthSurface	=NULL;
	m_pLDRRenderTarget		=NULL;
	m_pOldRenderTarget		=NULL;
	m_pOldDepthSurface		=NULL;
	if(use_buffer)
	{
		PIXBeginNamedEvent(0,"Setup Sky Buffer");
		D3DSURFACE_DESC desc;
		hdr_buffer_texture->GetLevelDesc(0,&desc);
		hr=m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget);
		hr=m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
		m_pHDRRenderTarget=MakeRenderTarget(hdr_buffer_texture);
		if(buffer_depth_texture)
			buffer_depth_texture->GetSurfaceLevel(0,&m_pBufferDepthSurface);
		hr=m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget);
		if(buffer_depth_texture)
			m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface);
		hr=m_pd3dDevice->SetRenderState(D3DRS_STENCILENABLE,FALSE);
		hr=m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
		hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
		hr=m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_ALPHA);
		static float depth_start=1.f;
		hr=m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start, 0L);
		hr=m_pd3dDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,0); // Defaults to zero
		hr=m_pd3dDevice->SetRenderState(D3DRS_DEPTHBIAS,0);           // Defaults to zero
		PIXEndNamedEvent();
	}
	static bool draw_sky=true;
	if(simulSkyRenderer&&draw_sky)
		hr=simulSkyRenderer->Render();
	if(simul2DCloudRenderer&&layer2)
		hr=simul2DCloudRenderer->Render();
	if(simulCloudRenderer&&layer1&&!RenderCloudsLate)
	{
		if(renderDepthBufferCallback)
			renderDepthBufferCallback->Render();
		hr=simulCloudRenderer->Render();
	}
	if(simulPrecipitationRenderer&&layer1) 
		hr=simulPrecipitationRenderer->Render();
	static float depth_start=1.f;
	if(use_buffer)
	{
		PIXBeginNamedEvent(0,"Sky Buffer To Screen");
#ifdef XBOX
		m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
#endif
		D3DSURFACE_DESC desc;
		// here we're going to render to the LDR buffer. If tone_map is disabled, we don't do this:
		m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
		m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
		m_pd3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
		if(tone_map)
		{
			m_pLDRRenderTarget=MakeRenderTarget(ldr_buffer_texture);
			m_pd3dDevice->SetRenderTarget(0,m_pLDRRenderTarget);
			ldr_buffer_texture->GetLevelDesc(0,&desc);
			hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start,0L);
			// using gamma, render to the 8\8\8 buffer:
			RenderBufferToScreen(hdr_buffer_texture,desc.Width,desc.Height,true);
			m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
			m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
			m_pOldRenderTarget->GetDesc(&desc);
			RenderBufferToScreen(ldr_buffer_texture,desc.Width,desc.Height,false);
		}
		else
		{
			m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
			m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
			m_pOldRenderTarget->GetDesc(&desc);
			RenderBufferToScreen(hdr_buffer_texture,desc.Width,desc.Height,true);
		}
		SAFE_RELEASE(m_pOldRenderTarget)
		SAFE_RELEASE(m_pOldDepthSurface)
		SAFE_RELEASE(m_pLDRRenderTarget)
		SAFE_RELEASE(m_pHDRRenderTarget)
		SAFE_RELEASE(m_pBufferDepthSurface)
		PIXEndNamedEvent();
	}
	hr=m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_ZBUFFER,0xFF000000,depth_start, 0L);
	if(simulCloudRenderer&&layer1)
		hr=simulCloudRenderer->RenderLightning();
	static bool show_flare=false;
	if(simulSkyRenderer&&show_flare)
		hr=simulSkyRenderer->RenderFlare(exposure);
	PIXEndNamedEvent();
	return hr;
}

HRESULT SimulWeatherRenderer::RenderLateCloudLayer()
{
	HRESULT hr=S_OK;
	if(!RenderCloudsLate)
		return hr;
	PIXBeginNamedEvent(0,"Render Late Cloud Layer");
	m_pHDRRenderTarget		=NULL;
	m_pBufferDepthSurface	=NULL;
	m_pLDRRenderTarget		=NULL;
	m_pOldRenderTarget		=NULL;
	m_pOldDepthSurface		=NULL;
	if(use_buffer)
	{
		PIXBeginNamedEvent(0,"Setup Sky Buffer");
		D3DSURFACE_DESC desc;
		hdr_buffer_texture->GetLevelDesc(0,&desc);
		hr=m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget);
		hr=m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
		m_pHDRRenderTarget=MakeRenderTarget(hdr_buffer_texture);
		if(buffer_depth_texture)
			buffer_depth_texture->GetSurfaceLevel(0,&m_pBufferDepthSurface);
		hr=m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget);
		if(buffer_depth_texture)
			m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface);
		hr=m_pd3dDevice->SetRenderState(D3DRS_STENCILENABLE,FALSE);
		hr=m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
		hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
		hr=m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_ALPHA);
		static float depth_start=1.f;
		hr=m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start, 0L);
		hr=m_pd3dDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,0); // Defaults to zero
		hr=m_pd3dDevice->SetRenderState(D3DRS_DEPTHBIAS,0);           // Defaults to zero
		PIXEndNamedEvent();
	}
	if(renderDepthBufferCallback)
		renderDepthBufferCallback->Render();
	if(simulCloudRenderer&&layer1)
		hr=simulCloudRenderer->Render();
	static float depth_start=1.f;
	if(use_buffer)
	{
		PIXBeginNamedEvent(0,"Sky Buffer To Screen");
#ifdef XBOX
		m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
#endif
		D3DSURFACE_DESC desc;
		m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
		m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
		m_pd3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
		m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

		m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
		m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
		m_pOldRenderTarget->GetDesc(&desc);
		RenderBufferToScreen(hdr_buffer_texture,desc.Width,desc.Height,true,true);

		SAFE_RELEASE(m_pOldRenderTarget)
		SAFE_RELEASE(m_pOldDepthSurface)
		SAFE_RELEASE(m_pLDRRenderTarget)
		SAFE_RELEASE(m_pHDRRenderTarget)
		SAFE_RELEASE(m_pBufferDepthSurface)
		PIXEndNamedEvent();
	}
	return hr;
}

void SimulWeatherRenderer::EnableLayers(bool clouds3d,bool clouds2d)
{
	layer1=clouds3d;
	layer2=clouds2d;
}

HRESULT SimulWeatherRenderer::RenderBufferToScreen(LPDIRECT3DTEXTURE9 texture,int width,int height,
												   bool do_tonemap,bool blend)
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
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

	m_pTonemapEffect->SetTexture(bufferTexture,texture);
	if(blend)
		m_pTonemapEffect->SetTechnique(CloudBlendTechnique);
	else
		m_pTonemapEffect->SetTechnique(GammaTechnique);

    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetPixelShader( NULL );

	m_pd3dDevice->SetVertexDeclaration(m_pBufferVertexDecl);
	if(do_tonemap)
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
	}
	m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertext) );
	if(do_tonemap)
	{
		hr=m_pTonemapEffect->EndPass();
		hr=m_pTonemapEffect->End();
	}
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	return hr;
}

void SimulWeatherRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(v,p);
	if(simulCloudRenderer)
		simulCloudRenderer->SetMatrices(v,p);
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(v,p);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->SetMatrices(v,p);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(v,p);
}

void SimulWeatherRenderer::Update(float dt)
{
	static bool pause=false;
    if(!pause)
	{
		PIXBeginNamedEvent(0,"Weather Update");
		if(simulSkyRenderer)
		{
			LPDIRECT3DBASETEXTURE9 l1,l2,i1,i2;
			simulSkyRenderer->GetLossAndInscatterTextures(&l1,&l2,&i1,&i2);
			simulSkyRenderer->Update(dt);
			if(layer1&&simulCloudRenderer)
			{
				simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
				simulCloudRenderer->SetLossTextures(l1,l2);
				simulCloudRenderer->SetInscatterTextures(i1,i2);
				simulCloudRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
				simulCloudRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
				if(simul2DCloudRenderer)
					simul2DCloudRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
			}
			else
			{
				simulSkyRenderer->SetOvercastFactor(0.f);
				if(simul2DCloudRenderer)
					simul2DCloudRenderer->SetOvercastFactor(0.f);
			}
			if(simulAtmosphericsRenderer)
			{
				if(layer1&&simulCloudRenderer)
					simulAtmosphericsRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
				simulAtmosphericsRenderer->SetLossTextures(l1,l2);
				simulAtmosphericsRenderer->SetInscatterTextures(i1,i2);
				simulAtmosphericsRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
				simulAtmosphericsRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
			}
		}
		if(simulCloudRenderer)
		{
			simulCloudRenderer->Update(dt);
		}
		if(simul2DCloudRenderer)
			simul2DCloudRenderer->Update(dt);
		if(simulPrecipitationRenderer)
		{
			simulPrecipitationRenderer->Update(dt);
			if(layer1&&simulCloudRenderer)
				simulPrecipitationRenderer->SetIntensity(simulCloudRenderer->GetPrecipitationIntensity());
			else
			{
				simulPrecipitationRenderer->SetIntensity(0.f);
			}
			if(simulSkyRenderer)
			{
				simul::sky::float4 l=simulSkyRenderer->GetSkyInterface()->GetSunlightColour(0);
				simulPrecipitationRenderer->SetLightColour((const float*)(l));
			}
		}
		PIXEndNamedEvent();
	}
}

SimulSkyRenderer *SimulWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer;
}

SimulCloudRenderer *SimulWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer;
}

Simul2DCloudRenderer *SimulWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer;
}

SimulPrecipitationRenderer *SimulWeatherRenderer::GetPrecipitationRenderer()
{
	return simulPrecipitationRenderer;
}

SimulAtmosphericsRenderer *SimulWeatherRenderer::GetAtmosphericsRenderer()
{
	return simulAtmosphericsRenderer;
}

const char *SimulWeatherRenderer::GetDebugText() const
{
	static char debug_text[256];
	if(simul2DCloudRenderer)
		//sprintf_s(debug_text,256,"TIME %2.2g ms\n%s",timing,simulCloudRenderer->GetDebugText());
		sprintf_s(debug_text,256,"%s",simul2DCloudRenderer->GetDebugText());
	return debug_text;
}

float SimulWeatherRenderer::GetTiming() const
{
	return timing;
}