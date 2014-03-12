#define NOMINMAX
// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulPrecipitationRenderer.h"

#ifdef XBOX
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <d3d9.h>
	#include <d3dx9.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif

#include "Simul/Base/SmartPtr.h"
#include "Simul/Math/Pi.h"
#include "Simul/Sky/Float4.h"
#include "Macros.h"
#include "CreateDX9Effect.h"
#include "Resources.h"

typedef std::basic_string<TCHAR> tstring;

#define CONE_SIDES 36
#define NUM_VERT ((CONE_SIDES+1)*8)

SimulPrecipitationRenderer::SimulPrecipitationRenderer() :
	m_pVtxDecl(NULL),
	m_pRainEffect(NULL),
	rain_texture(NULL),
	external_rain_texture(false)
{
}

void SimulPrecipitationRenderer::TextureRepeatChanged()
{
	InvalidateDeviceObjects();
	RestoreDeviceObjects(m_pd3dDevice);
}

void SimulPrecipitationRenderer::RecompileShaders()
{
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pRainEffect,"simul_rain.fx"));

	m_hTechniqueRain	=m_pRainEffect->GetTechniqueByName("simul_rain");
	worldViewProj		=m_pRainEffect->GetParameterByName(NULL,"worldViewProj");
	offset				=m_pRainEffect->GetParameterByName(NULL,"offset");
	intensity			=m_pRainEffect->GetParameterByName(NULL,"intensity");
	lightColour			=m_pRainEffect->GetParameterByName(NULL,"lightColour");
	offset1				=m_pRainEffect->GetParameterByName(NULL,"offset1");
	offset2				=m_pRainEffect->GetParameterByName(NULL,"offset2");
	offset3				=m_pRainEffect->GetParameterByName(NULL,"offset3");

	if(!external_rain_texture)
	{
		SAFE_RELEASE(rain_texture);
		V_CHECK(CreateDX9Texture(m_pd3dDevice,rain_texture,"Rain.jpg"));
	}
}

void SimulPrecipitationRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
//	int index=0;
	static float rr=0.2f;
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);
	RecompileShaders();
}

bool SimulPrecipitationRenderer::SetExternalRainTexture(LPDIRECT3DTEXTURE9 tex)
{
	if(!external_rain_texture)
	{
		SAFE_RELEASE(rain_texture);
	}
	external_rain_texture=true;
	rain_texture=tex;
	return true;
}

void SimulPrecipitationRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pRainEffect)
        hr=m_pRainEffect->OnLostDevice();
	SAFE_RELEASE(m_pRainEffect);
	SAFE_RELEASE(m_pVtxDecl);
	if(!external_rain_texture)
		SAFE_RELEASE(rain_texture);
}

SimulPrecipitationRenderer::~SimulPrecipitationRenderer()
{
	InvalidateDeviceObjects();
}

void SimulPrecipitationRenderer::Render(void *)
{
}

#ifdef XBOX
void SimulPrecipitationRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
#endif