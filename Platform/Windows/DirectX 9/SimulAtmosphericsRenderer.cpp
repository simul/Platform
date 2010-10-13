// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulAtmosphericsRenderer.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
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
#include "Resources.h"

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
	imageTexture(NULL),
	depthTexture(NULL),
	lossTexture1(NULL),
	lossTexture2(NULL),
	inscatterTexture1(NULL),
	inscatterTexture2(NULL),
	loss_texture_1(NULL),
	loss_texture_2(NULL),
	inscatter_texture_1(NULL),
	inscatter_texture_2(NULL),
	skyInterface(NULL),
	fade_interp(0.f),
	altitude_tex_coord(0.f)
	,input_texture(NULL)
{
}

HRESULT SimulAtmosphericsRenderer::RestoreDeviceObjects(LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr;
	SAFE_RELEASE(effect);
	V_RETURN(CreateDX9Effect(m_pd3dDevice,effect,"atmospherics.fx"));

	technique			=effect->GetTechniqueByName("simul_atmospherics");
	
	invViewProj			=effect->GetParameterByName(NULL,"invViewProj");
	altitudeTexCoord	=effect->GetParameterByName(NULL,"altitudeTexCoord");
	lightDir			=effect->GetParameterByName(NULL,"lightDir");
	MieRayleighRatio	=effect->GetParameterByName(NULL,"MieRayleighRatio");
	HazeEccentricity	=effect->GetParameterByName(NULL,"HazeEccentricity");
	fadeInterp			=effect->GetParameterByName(NULL,"fadeInterp");
	imageTexture		=effect->GetParameterByName(NULL,"imageTexture");
	depthTexture		=effect->GetParameterByName(NULL,"depthTexture");
	lossTexture1		=effect->GetParameterByName(NULL,"lossTexture1");
	lossTexture2		=effect->GetParameterByName(NULL,"lossTexture2");
	inscatterTexture1	=effect->GetParameterByName(NULL,"inscatterTexture1");
	inscatterTexture2	=effect->GetParameterByName(NULL,"inscatterTexture2");
	
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  8, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 8, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
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

#ifdef XBOX
void SimulAtmosphericsRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
#endif

HRESULT SimulAtmosphericsRenderer::Render()
{
	HRESULT hr=S_OK;
	PIXWrapper(0xFFFFFF00,"Render Atmospherics")
	{
#ifndef XBOX
		m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
		m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
		D3DXMATRIX vpt;
		D3DXMATRIX viewproj;
		view._41=view._42=view._43=0;
		D3DXMatrixMultiply(&viewproj, &view,&proj);
		D3DXMatrixTranspose(&vpt,&viewproj);
		D3DXMATRIX ivp;
		D3DXMatrixInverse(&ivp,NULL,&vpt);

		hr=effect->SetMatrix(invViewProj,&ivp);
		hr=effect->SetFloat(fadeInterp,fade_interp);

		hr=effect->SetTexture(imageTexture,input_texture);

		hr=effect->SetFloat(altitudeTexCoord,altitude_tex_coord);
		if(skyInterface)
		{
			hr=effect->SetFloat(HazeEccentricity,skyInterface->GetMieEccentricity());
			D3DXVECTOR4 mie_rayleigh_ratio(skyInterface->GetMieRayleighRatio());
			D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight());
			
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
			float tx,ty,tz;
		};
		Vertext vertices[4] =
		{
			{x,			y,			0	,0},
			{x+w,		y,			0	,0},
			{x+w,		y+h,		1.f,0},
			{x,			y+h,		1.f,0},
		};
	#else
		D3DSURFACE_DESC desc;
		if(input_texture)
			input_texture->GetLevelDesc(0,&desc);
	
		struct Vertext
		{
			float x,y;
			float tx,ty,tz;
		};
		Vertext vertices[4] =
		{
			{-1.f,	-1.f	,0.5f	,0		,1.f},
			{ 1.f,	-1.f	,0.5f	,1.f	,1.f},
			{ 1.f,	 1.f	,0.5f	,1.f	,0	},
			{-1.f,	 1.f	,0.5f	,0		,0	},
			/*{x-desc.Width/2,	y-desc.Height/2,	0	,0		,0},
			{x+desc.Width/2,	y-desc.Height/2,	1.f	,0		,0},
			{x+desc.Width/2,	y+desc.Height/2,	1.f	,1.f	,0},
			{x-desc.Width/2,	y+desc.Height/2,	0	,1.f	,0},*/
		};
	#endif
		D3DXMATRIX ident;
		D3DXMatrixIdentity(&ident);
		//m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

		//m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		//m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
		//m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

	   // m_pd3dDevice->SetVertexShader(NULL);
	   // m_pd3dDevice->SetPixelShader(NULL);
		m_pd3dDevice->SetVertexDeclaration(vertexDecl);
		effect->SetTechnique(technique);

		UINT passes=1;
		hr=effect->Begin(&passes,0);
		hr=effect->BeginPass(0);
		m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertext) );
		hr=effect->EndPass();
		hr=effect->End();
		
	//	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
		//m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		//m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	}
	return hr;
}
