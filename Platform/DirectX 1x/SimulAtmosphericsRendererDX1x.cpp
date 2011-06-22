// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulAtmosphericsRendererdx1x.h"

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
	#define PIXBeginNamedEvent(a,b) D3DPERF_BeginEvent(a,L##b)
	#define PIXEndNamedEvent D3DPERF_EndEvent
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#endif
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRendererDX10.h"
#include "SimulPrecipitationRendererdx10.h"
#include "Simul2DCloudRendererdx10.h"
#include "SimulSkyRendererDX10.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "CreateEffectdx10.h"


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

SimulAtmosphericsRenderer::SimulAtmosphericsRenderer() :
	m_pd3dDevice(NULL),
	vertexDecl(NULL),
	effect(NULL),
	lightDir(NULL),
	MieRayleighRatio(NULL),
	HazeEccentricity(NULL),
	fadeInterp(NULL),
	overcastFactor(NULL),
	imageTexture(NULL),
	depthTexture(NULL),
	lossTexture1(NULL),
	lossTexture2(NULL),
	inscatterTexture1(NULL),
	inscatterTexture2(NULL),
	skyInterface(NULL),
	overcast_factor(0.f),
	fade_interp(0.f)
{
}

HRESULT SimulAtmosphericsRenderer::RestoreDeviceObjects(ID3D10Device* dev)
{
	m_pd3dDevice=dev;
	HRESULT hr;
	SAFE_RELEASE(effect);
	V_RETURN(CreateEffect(m_pd3dDevice,&effect,"media\\HLSL\\atmospherics.fx"));

	technique			=effect->GetTechniqueByName("simul_atmospherics");
	
	invViewProj			=effect->GetVariableByName("invViewProj");
	lightDir			=effect->GetVariableByName("lightDir");
	MieRayleighRatio	=effect->GetVariableByName("MieRayleighRatio");
	HazeEccentricity	=effect->GetVariableByName("HazeEccentricity");
	fadeInterp			=effect->GetVariableByName("fadeInterp");
	overcastFactor		=effect->GetVariableByName("overcastFactor");
	imageTexture		=effect->GetVariableByName("imageTexture");
	depthTexture		=effect->GetVariableByName("depthTexture");
	lossTexture1		=effect->GetVariableByName("lossTexture1");
	lossTexture2		=effect->GetVariableByName("lossTexture2");
	inscatterTexture1	=effect->GetVariableByName("inscatterTexture1");
	inscatterTexture2	=effect->GetVariableByName("inscatterTexture2");
	
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
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&vertexDecl);
	return hr;
}

HRESULT SimulAtmosphericsRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(vertexDecl);
	if(effect)
        hr=effect->OnLostDevice();
	SAFE_RELEASE(effect);
	return hr;
}

HRESULT SimulAtmosphericsRenderer::Destroy()
{
	return InvalidateDeviceObjects();
}

SimulAtmosphericsRenderer::~SimulAtmosphericsRenderer()
{
	Destroy();
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

void SimulAtmosphericsRenderer::SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	world=w;
	view=v;
	proj=p;
}

HRESULT SimulAtmosphericsRenderer::Render()
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
	hr=effect->AsScalar()->SetFloat(fadeInterp,fade_interp);
	hr=effect->AsScalar()->SetFloat(overcastFactor,overcast_factor);

	hr=effect->SetTexture(imageTexture,input_texture);
	hr=effect->SetTexture(depthTexture,depth_texture);

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

	hr=effect->SetTexture(lossTexture1,loss_texture_1);
	hr=effect->SetTexture(lossTexture2,loss_texture_2);

	hr=effect->SetTexture(inscatterTexture1,inscatter_texture_1);
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
