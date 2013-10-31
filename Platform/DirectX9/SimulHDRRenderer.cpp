#define NOMINMAX
// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulHDRRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulHDRRenderer.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	static tstring filepath=TEXT("game:\\");
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
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
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Macros.h"
#include "SimulAtmosphericsRenderer.h"
#include "Resources.h"


#define BLUR_SIZE 9
#define MONTE_CARLO_BLUR

#ifdef  MONTE_CARLO_BLUR
#include "Simul/Math/Pi.h"
#endif

SimulHDRRenderer::SimulHDRRenderer(int width,int height) :
	m_pBufferVertexDecl(NULL),
	m_pd3dDevice(NULL),
	m_pTonemapEffect(NULL),
	hdr_buffer_texture(NULL),
	faded_texture(NULL),
	buffer_depth_texture(NULL),
	depth_alpha_texture(NULL),
	m_pHDRRenderTarget(NULL),
	m_pBufferDepthSurface(NULL),
	m_pFadedRenderTarget(NULL),
	Exposure(1.f),
	Gamma(1.f/2.2f),
	BufferWidth(width),
	BufferHeight(height),
	timing(0.f),
	atmospherics(NULL)
{
}

void SimulHDRRenderer::RecompileShaders()
{
	SAFE_RELEASE(m_pTonemapEffect);
	if(!m_pTonemapEffect)
#ifdef  MONTE_CARLO_BLUR
	{
		char blur_size[20];
		sprintf_s(blur_size,20,"%d",BLUR_SIZE);
		std::map<std::string,std::string> defines;
		defines["BLUR_SIZE"]=blur_size;
		V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pTonemapEffect,"gamma.fx",defines));
	}
#else
		V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pTonemapEffect,"gamma.fx"));
#endif
	ToneMapTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_tonemap");
	ToneMapZWriteTechnique	=m_pTonemapEffect->GetTechniqueByName("simul_tonemap_zwrite");
	Exposure_				=m_pTonemapEffect->GetParameterByName(NULL,"exposure");
	Gamma_					=m_pTonemapEffect->GetParameterByName(NULL,"gamma");
	hdrTexture				=m_pTonemapEffect->GetParameterByName(NULL,"hdrTexture");
}

void SimulHDRRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	RecompileShaders();
	CreateBuffers();
}

void SimulHDRRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(depth_alpha_texture);
	SAFE_RELEASE(m_pBufferVertexDecl);
	SAFE_RELEASE(m_pHDRRenderTarget);
	SAFE_RELEASE(m_pBufferDepthSurface);
	SAFE_RELEASE(m_pFadedRenderTarget)
	if(m_pTonemapEffect)
        hr=m_pTonemapEffect->OnLostDevice();
	SAFE_RELEASE(m_pTonemapEffect);
	SAFE_RELEASE(hdr_buffer_texture);
	SAFE_RELEASE(faded_texture);
	SAFE_RELEASE(buffer_depth_texture);
}

SimulHDRRenderer::~SimulHDRRenderer()
{
	InvalidateDeviceObjects();
}


bool SimulHDRRenderer::IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat)
{
	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);

    return (hr==S_OK);
}

void SimulHDRRenderer::SetBufferSize(int w,int h)
{
	BufferWidth=w;
	BufferHeight=h;
}

bool SimulHDRRenderer::CreateBuffers()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(hdr_buffer_texture);
#ifndef XBOX
	D3DFORMAT hdr_format=D3DFMT_A16B16G16R16F;
#else
	D3DFORMAT hdr_format=D3DFMT_LIN_A16B16G16R16F;
#endif
	hr=CanUse16BitFloats(m_pd3dDevice);
	//if(hr!=S_OK)
#ifndef XBOX
		hdr_format=D3DFMT_A32B32G32R32F;
#else
		hdr_format=D3DFMT_LIN_A32B32G32R32F;
#endif
	B_RETURN(CanUseTexFormat(m_pd3dDevice,hdr_format));
	B_RETURN(m_pd3dDevice->CreateTexture(	BufferWidth,
											BufferHeight,
											1,
											D3DUSAGE_RENDERTARGET,
											hdr_format,
											D3DPOOL_DEFAULT,
											&hdr_buffer_texture,
											NULL
										));
	
	SAFE_RELEASE(depth_alpha_texture);
	B_RETURN(m_pd3dDevice->CreateTexture(	BufferWidth,
											BufferHeight,
											1,
											D3DUSAGE_RENDERTARGET,
											hdr_format,
											D3DPOOL_DEFAULT,
											&depth_alpha_texture,
											NULL
										));
	SAFE_RELEASE(faded_texture);
	B_RETURN(m_pd3dDevice->CreateTexture(	BufferWidth,
											BufferHeight,
											1,
											D3DUSAGE_RENDERTARGET,
											hdr_format,
											D3DPOOL_DEFAULT,
											&faded_texture,
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
	B_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl));
	D3DFORMAT fmtDepthTex = D3DFMT_UNKNOWN;
//	D3DFORMAT possibles[]={D3DFMT_D32,D3DFMT_D24S8,D3DFMT_D24FS8,D3DFMT_D24X8,D3DFMT_D16,D3DFMT_UNKNOWN};
	LPDIRECT3DSURFACE9 g_BackBuffer;
    m_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_BackBuffer);
	D3DSURFACE_DESC desc;
	g_BackBuffer->GetDesc(&desc);
	SAFE_RELEASE(g_BackBuffer);
    D3DDISPLAYMODE d3ddm;
	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
	B_RETURN(d3d->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm ));
	SAFE_RELEASE(m_pHDRRenderTarget);
	m_pHDRRenderTarget=MakeRenderTarget(hdr_buffer_texture);
	SAFE_RELEASE(m_pFadedRenderTarget)
	m_pFadedRenderTarget=MakeRenderTarget(faded_texture);
	SAFE_RELEASE(buffer_depth_texture);
	// Try creating a depth texture
	if(fmtDepthTex!=D3DFMT_UNKNOWN)
	{
		/*V_CHECK((m_pd3dDevice->CreateTexture(BufferWidth,
										BufferHeight,
										1,
										D3DUSAGE_DEPTHSTENCIL,
										fmtDepthTex,
										D3DPOOL_DEFAULT,
										&buffer_depth_texture,
										NULL
									)));*/
		hr=S_OK;
	}
	SAFE_RELEASE(m_pBufferDepthSurface);
	if(buffer_depth_texture)
		m_pBufferDepthSurface=MakeRenderTarget(buffer_depth_texture);
	return (hr==S_OK);
}

LPDIRECT3DSURFACE9 SimulHDRRenderer::MakeRenderTarget(const LPDIRECT3DTEXTURE9 pTexture)
{
	LPDIRECT3DSURFACE9 pRenderTarget;
	if(!pTexture)
	{
		MessageBox(NULL, _T("Trying to create RenderTarget from NULL texture!"), _T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
	}
	V_CHECK(pTexture->GetSurfaceLevel(0,&pRenderTarget));
	return pRenderTarget;
}
static float depth_start=1.f;

bool SimulHDRRenderer::StartRender()
{
	return (true);
}

bool SimulHDRRenderer::CopyDepthAlpha()
{
	D3DXMATRIX view;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
#endif
	// Unselect the current rt
	HRESULT hr=m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
	LPDIRECT3DSURFACE9		depthAlphaRenderTarget=NULL;
	if(depth_alpha_texture)
		depthAlphaRenderTarget=MakeRenderTarget(depth_alpha_texture);
	else
		return false;
	// Copy its surface to the depth texture.
	hr=m_pd3dDevice->StretchRect(	m_pHDRRenderTarget,
									NULL,
									depthAlphaRenderTarget,
									NULL,
									D3DTEXF_NONE	);
	SAFE_RELEASE(depthAlphaRenderTarget);
	// copy the render surface to the depth surface depth_alpha_texture
	hr=m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget);
	//if(atmospherics)
	//	atmospherics->SetInputTextures(depth_alpha_texture,buffer_depth_texture);
	return (hr==S_OK);
}

bool SimulHDRRenderer::FinishRender(void *)
{
	return true;
}

void SimulHDRRenderer::Render(void *context,void *tex)
{
	HRESULT hr=S_OK;
	D3DSURFACE_DESC desc;
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);

//	B_RETURN(m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET,0xFF000000,depth_start,0L));
	m_pTonemapEffect->SetTechnique(ToneMapTechnique);
	(m_pTonemapEffect->SetFloat(Exposure_,Exposure));
	(m_pTonemapEffect->SetFloat(Gamma_,Gamma));
	(m_pTonemapEffect->SetTexture(hdrTexture,(LPDIRECT3DBASETEXTURE9)tex));

	DrawFullScreenQuad(m_pd3dDevice,m_pTonemapEffect);

	PIXEndNamedEvent();
}

const char *SimulHDRRenderer::GetDebugText() const
{
	static char debug_text[256];
	return debug_text;
}

float SimulHDRRenderer::GetTiming() const
{
	return timing;
}