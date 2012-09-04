// Copyright (c) 2007-2011 Simul Software Ltd
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

SimulPrecipitationRenderer::SimulPrecipitationRenderer() :
	m_pVtxDecl(NULL),
	m_pRainEffect(NULL),
	rain_texture(NULL),
	external_rain_texture(false)
{
}

struct Vertex_t
{
	float x,y,z;
	float tex_x,tex_y,fade;
};

#define CONE_SIDES 36
#define NUM_VERT ((CONE_SIDES+1)*8)
static Vertex_t vertices[NUM_VERT];

void SimulPrecipitationRenderer::TextureRepeatChanged()
{
	InvalidateDeviceObjects();
	RestoreDeviceObjects(m_pd3dDevice);
}

void SimulPrecipitationRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	int index=0;
	static float rr=0.2f;
	for(int j=-2;j<2;j++)
	{
		for(int i=0;i<CONE_SIDES+1;i++)
		{
			float angle1=2.f*3.14159f*(float)i/(float)CONE_SIDES;
			float fade=1.f-0.5f*(float)abs(j);
			float rad=radius*(rr+fade)/(1.f+rr);
			vertices[index].x=rad*cos(angle1)*fade;
			vertices[index].z=rad*sin(angle1)*fade;
			vertices[index].y=height*0.5f*j;
			vertices[index].tex_x=i*TextureRepeat/(float)CONE_SIDES;
			vertices[index].tex_y=0.5f*(float)j*height/radius/Aspect*(float)TextureRepeat;
			vertices[index].fade=fade;
			index++;
			fade=1.f-0.5f*(float)abs(j+1);
			rad=radius*(rr+fade)/(1.f+rr);
			vertices[index].x=rad*cos(angle1)*fade;
			vertices[index].z=rad*sin(angle1)*fade;
			vertices[index].y=height*0.5f*(j+1);
			vertices[index].tex_x=i*TextureRepeat/(float)CONE_SIDES;
			vertices[index].tex_y=0.5f*(float)(j+1)*height/radius/Aspect*(float)TextureRepeat;
			vertices[index].fade=fade;
			index++;
		}
	}
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pRainEffect,"simul_rain.fxo"));

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

static D3DXVECTOR3 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR3 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}

bool SimulPrecipitationRenderer::Render()
{
	HRESULT hr=S_OK;
	if(rain_intensity<=0)
		return (hr==S_OK);
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	m_pd3dDevice->SetTexture(0,rain_texture);
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	hr=m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl);

	m_pRainEffect->SetTechnique( m_hTechniqueRain );
	cam_pos=GetCameraPosVector(view);

	simul::math::Vector3 diff=(const float*)cam_pos;
	diff-=last_cam_pos;
	last_cam_pos=(const float*)cam_pos;

	float pitch_angle=pi/2.f-atan2f(WindEffect*wind_speed,rain_speed);
	float rain_heading=wind_heading;
	simul::math::Vector3 rain_vector(	cos(pitch_angle)*sin(rain_heading),
										sin(pitch_angle),
										cos(pitch_angle)*cos(rain_heading));
	static float camera_motion_factor=0.1f;
	rain_vector-=camera_motion_factor*diff;
	rain_vector.Normalize();
	pitch_angle=pi/2.f-asin(rain_vector.y);
	rain_heading=atan2(rain_vector.x,rain_vector.z);

	D3DXMATRIX	world,direction;
	D3DXMatrixIdentity(&world);
	D3DXMatrixRotationY(&direction,rain_heading);


	D3DXMatrixRotationX(&world,-pitch_angle);
	D3DXMatrixMultiply(&world,&world,&direction);
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pRainEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));
	m_pRainEffect->SetFloat(offset,offs);
	m_pRainEffect->SetFloat(intensity,rain_intensity);

	static float cc1=3.f;
	static float cc2=4.f;
	static float cc3=3.5f;
	simul::sky::float4 offs1(sin(cc1*offs)		,cos(cc1*offs),0,0);
	simul::sky::float4 offs2(sin(cc2*offs+2.f)	,cos(cc2*offs+2.f),0,0);
	simul::sky::float4 offs3(sin(cc3*offs+4.f)	,cos(cc3*offs+4.f),0,0);
	static float cc1a=1.4f;
	static float cc2a=1.211f;
	static float cc3a=0.397f;
	simul::sky::float4 offs1a(sin(cc1a*offs)	,cos(cc1a*offs),0,0);
	simul::sky::float4 offs2a(sin(cc2a*offs+2.f),cos(cc2a*offs+2.f),0,0);
	simul::sky::float4 offs3a(sin(cc3a*offs+4.f),cos(cc3a*offs+4.f),0,0);
	offs1*=offs1a;
	offs2*=offs2a;
	offs3*=offs3a;
	static float ww=0.01f;
	offs1*=ww*Waver;
	offs2*=ww*Waver;
	offs3*=ww*Waver;
	m_pRainEffect->SetVector(offset1,(D3DXVECTOR4*)(&offs1));
	m_pRainEffect->SetVector(offset2,(D3DXVECTOR4*)(&offs2));
	m_pRainEffect->SetVector(offset3,(D3DXVECTOR4*)(&offs3));

	m_pRainEffect->SetVector(lightColour,(D3DXVECTOR4*)(light_colour));

	UINT passes=1;
	hr=m_pRainEffect->Begin( &passes, 0 );
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		hr=m_pRainEffect->BeginPass(i);
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,NUM_VERT-2,vertices,sizeof(Vertex_t));
		hr=m_pRainEffect->EndPass();
	}
	hr=m_pRainEffect->End();
	D3DXMatrixIdentity(&world);
	return (hr==S_OK);
}

#ifdef XBOX
void SimulPrecipitationRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
#endif