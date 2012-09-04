// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRendererDX1x.cpp A renderer for skies, clouds and weather effects.

#include "SimulAtmosphericsRendererDX1x.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#endif
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRendererDX1x.h"
#include "SimulPrecipitationRendererDX1x.h"
#include "Simul2DCloudRendererDX1x.h"
#include "SimulSkyRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "CreateEffectDX1x.h"


#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)			{ hr = x; if( FAILED(hr) ) { return hr; } }
#endif
#ifndef SAFE_DELETE
#define SAFE_DELETE(p)		{ if(p) { delete (p);     (p)=NULL; } }
#endif    
#define BLUR_SIZE 9
#define MONTE_CARLO_BLUR

#ifdef  MONTE_CARLO_BLUR
#include "Simul/Math/Pi.h"
#endif

SimulAtmosphericsRendererDX1x::SimulAtmosphericsRendererDX1x() :
	m_pd3dDevice(NULL),
	vertexDecl(NULL),
	framebuffer(NULL),
	effect(NULL),
	lightDir(NULL),
	m_pImmediateContext(NULL),
	MieRayleighRatio(NULL),
	HazeEccentricity(NULL),
	fadeInterp(NULL),
	imageTexture(NULL),
	depthTexture(NULL),
	lossTexture1(NULL),
	inscatterTexture1(NULL),
	skyInterface(NULL),
	

	loss_texture(NULL),
	inscatter_texture(NULL),
	clouds_texture(NULL),
	fade_interp(0.f)
{
	framebuffer=new FramebufferDX1x(256,256);
}

void SimulAtmosphericsRendererDX1x::SetBufferSize(int w,int h)
{
	if(framebuffer)
		framebuffer->SetWidthAndHeight(w,h);
	InvalidateDeviceObjects();
}

void SimulAtmosphericsRendererDX1x::SetYVertical(bool y)
{
}

void SimulAtmosphericsRendererDX1x::SetDistanceTexture(void* t)
{
	depth_texture=(ID3D1xTexture2D*)t;
}

void SimulAtmosphericsRendererDX1x::SetLossTexture(void* t)
{
	loss_texture=(ID3D1xTexture2D*)t;
}

void SimulAtmosphericsRendererDX1x::SetInscatterTexture(void* t)
{
	inscatter_texture=(ID3D1xTexture2D*)t;
}


void SimulAtmosphericsRendererDX1x::SetCloudsTexture(void* t)
{
	clouds_texture=(ID3D1xTexture2D*)t;
}

void SimulAtmosphericsRendererDX1x::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=S_OK;
	V_CHECK(CreateEffect(m_pd3dDevice,&effect,L"atmospherics.fx"));

	technique			=effect->GetTechniqueByName("simul_atmospherics");
	
	invViewProj			=effect->GetVariableByName("invViewProj")->AsMatrix();
	lightDir			=effect->GetVariableByName("lightDir")->AsVector();
	MieRayleighRatio	=effect->GetVariableByName("MieRayleighRatio")->AsVector();
	HazeEccentricity	=effect->GetVariableByName("HazeEccentricity")->AsScalar();
	fadeInterp			=effect->GetVariableByName("fadeInterp")->AsScalar();
	imageTexture		=effect->GetVariableByName("imageTexture")->AsShaderResource();
	depthTexture		=effect->GetVariableByName("depthTexture")->AsShaderResource();
	lossTexture1		=effect->GetVariableByName("lossTexture1")->AsShaderResource();
	inscatterTexture1	=effect->GetVariableByName("inscatterTexture1")->AsShaderResource();
}

HRESULT SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(ID3D1xDevice* dev)
{
	m_pd3dDevice=dev;
#ifdef DX10
	m_pImmediateContext=dev;
#else
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	HRESULT hr=S_OK;
return hr;
	RecompileShaders();
	if(framebuffer)
		framebuffer->RestoreDeviceObjects(dev);
	/*
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
	SAFE_RELEASE(vertexDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&vertexDecl);*/
	return hr;
}

HRESULT SimulAtmosphericsRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(framebuffer)
		framebuffer->InvalidateDeviceObjects();
#ifndef DX10
	SAFE_RELEASE(m_pImmediateContext);
#endif
	SAFE_RELEASE(vertexDecl);
	SAFE_RELEASE(effect);
	return hr;
}

HRESULT SimulAtmosphericsRendererDX1x::Destroy()
{
	return InvalidateDeviceObjects();
}

SimulAtmosphericsRendererDX1x::~SimulAtmosphericsRendererDX1x()
{
	Destroy();
	delete framebuffer;
}
static void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj)
{
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(wvp,&tmp2);
}

void SimulAtmosphericsRendererDX1x::SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	world=w;
	view=v;
	proj=p;
}

void SimulAtmosphericsRendererDX1x::StartRender()
{
	//if(!framebuffer)
		return;
	framebuffer->SetExposure(1.f);
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::StartRender");
	framebuffer->Activate();
	// Clear the screen to black:
    float clearColor[4]={0.0,0.0,0.0,0.0};
	m_pImmediateContext->ClearRenderTargetView(framebuffer->m_pHDRRenderTarget,clearColor);
	if(framebuffer->m_pBufferDepthSurface)
		m_pImmediateContext->ClearDepthStencilView(framebuffer->m_pBufferDepthSurface,D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL, 1.f, 0);
	PIXEndNamedEvent();
}

void SimulAtmosphericsRendererDX1x::FinishRender()
{
	if(!framebuffer)
		return;
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::FinishRender");
	framebuffer->DeactivateAndRender(false);
	PIXEndNamedEvent();
}
/*
HRESULT SimulAtmosphericsRendererDX1x::Render()
{
	HRESULT hr=S_OK;
	
	effect->SetTechnique(technique);
//  outpos=mul(wvp,pos);
// outpos=wvp*pos
// output=(view*proj)t * pos
// ((view*proj)')inv output=pos
	D3DXMATRIX vpt;
	D3DXMATRIX viewproj;
	D3DXMatrixMultiply(&viewproj, &view,&proj);
	D3DXMatrixTranspose(&vpt,&viewproj);
	D3DXMATRIX ivp;
	D3DXMatrixInverse(&ivp,NULL,&vpt);

	hr=effect->SetMatrix(invViewProj,&ivp);
	hr=fadeInterp->SetFloat(fade_interp);
	hr=overcastFactor->SetFloat(overcast_factor);

	hr=imageTexture->SetTexture(input_texture);
	hr=depthTexture->SetTexture(depth_texture);

	if(skyInterface)
	{
		hr=effect->AsScalar()->SetFloat(HazeEccentricity,skyInterface->GetMieEccentricity());
		D3DXVECTOR4 mie_rayleigh_ratio(skyInterface->GetMieRayleighRatio());
		D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToSun());
		//if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);

		effect->SetVector	(lightDir			,&sun_dir);
		effect->SetVector	(MieRayleighRatio	,&mie_rayleigh_ratio);
	}

	hr=effect->SetTexture(lossTexture1,loss_texture);
	hr=effect->SetTexture(lossTexture2,loss_texture_2);

	hr=effect->SetTexture(inscatterTexture1,inscatter_texture);
	hr=effect->SetTexture(inscatterTexture2,inscatter_texture_2);


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
	D3DSURFACE_DESC desc;
	input_texture->GetLevelDesc(0,&desc);
	float x=0,y=0;
	struct Vertext
	{
		float x,y,z,h;
		float tx,ty;
	};
	Vertext vertices[4] =
	{
		{x,				y,			1,	1, 0	,0},
		{x+desc.Width,	y,			1,	1, 1.f	,0},
		{x+desc.Width,	y+desc.Height,	1,	1, 1.f	,1.f},
		{x,				y+desc.Height,	1,	1, 0	,1.f},
	};
#endif
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

    m_pd3dDevice->SetVertexShader(NULL);
    m_pd3dDevice->SetPixelShader(NULL);

	m_pd3dDevice->IASetInputLayout(vertexDecl);
	
	UINT passes=1;
	hr=effect->Begin(&passes,0);
	hr=effect->BeginPass(0);
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertext) );
	hr=effect->EndPass();
	hr=effect->End();
	
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	return hr;
}
*/