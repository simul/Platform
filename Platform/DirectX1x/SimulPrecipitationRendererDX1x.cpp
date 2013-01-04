// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulPrecipitationRendererDX1x.h"
#include "Simul/Base/SmartPtr.h"
#include "CreateEffectDX1x.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }
#endif

typedef std::basic_string<TCHAR> tstring;

SimulPrecipitationRendererDX1x::SimulPrecipitationRendererDX1x() :
	m_pVtxDecl(NULL)
	,m_pVertexBuffer(NULL)
	,m_pRainEffect(NULL)
	,rain_texture(NULL)
	,radius(10.f)
	,height(100.f)
	,rain_intensity(0.f)
{
}

struct Vertex_t
{
	float x,y,z;
	float tex_x,tex_y,fade;
};
#define CONE_SIDES 36
#define NUM_VERT ((CONE_SIDES+1)*4)
static Vertex_t vertices[NUM_VERT];

HRESULT SimulPrecipitationRendererDX1x::RestoreDeviceObjects( void* dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
#ifdef DX10
	m_pImmediateContext=dev;
#else
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);

	int index=0;
	for(int j=-1;j<=1;j+=2)
	for(int i=0;i<CONE_SIDES+1;i++)
	{
		float angle1=2.f*3.14159f*(float)i/(float)CONE_SIDES;
		vertices[index].x=radius*cos(angle1);
		vertices[index].z=radius*sin(angle1);
		vertices[index].y=0;
		vertices[index].tex_x=i*14.f/(float)CONE_SIDES;
		vertices[index].tex_y=0;
		vertices[index].fade=1.f;
		index++;
		vertices[index].x=0;
		vertices[index].z=0;
		vertices[index].y=height*j;
		vertices[index].tex_x=i*14.f/(float)CONE_SIDES;
		vertices[index].tex_y=(float)j;
		vertices[index].fade=0.f;
		index++;
	}
    LPD3DXBUFFER errors=0;
	CreateEffect(m_pd3dDevice,&m_pRainEffect,L"simul_rain.fx");

	SAFE_RELEASE(rain_texture);
	rain_texture		=simul::dx11::LoadTexture("Rain.jpg");
	m_hTechniqueRain	=m_pRainEffect->GetTechniqueByName("simul_rain");
	worldViewProj		=m_pRainEffect->GetVariableByName("worldViewProj")->AsMatrix();
	offset				=m_pRainEffect->GetVariableByName("offset")->AsScalar();
	intensity			=m_pRainEffect->GetVariableByName("intensity")->AsScalar();
	lightColour			=m_pRainEffect->GetVariableByName("lightColour")->AsVector();
	
	D3D1x_INPUT_ELEMENT_DESC decl[] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D1x_INPUT_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D1x_INPUT_PER_VERTEX_DATA,0}
	};
	SAFE_RELEASE(m_pVtxDecl);
    D3D1x_PASS_DESC PassDesc;
	m_hTechniqueRain->GetPassByIndex(0)->GetDesc(&PassDesc);
	hr=m_pd3dDevice->CreateInputLayout(decl,1, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pVtxDecl);
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(Vertex_t);
    m_pd3dDevice->CreateInputLayout(decl,1, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pVtxDecl);

	D3D1x_BUFFER_DESC desc=
	{
        36*sizeof(Vertex_t),
        D3D1x_USAGE_DEFAULT,
        D3D1x_BIND_VERTEX_BUFFER,
        0,
        0
	};
	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&m_pVertexBuffer);
	return hr;
}


HRESULT SimulPrecipitationRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pRainEffect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(rain_texture);
	SAFE_RELEASE(m_pVertexBuffer);
	return hr;
}

HRESULT SimulPrecipitationRendererDX1x::Destroy()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pRainEffect);
	SAFE_RELEASE(rain_texture);
	return hr;
}

SimulPrecipitationRendererDX1x::~SimulPrecipitationRendererDX1x()
{
	Destroy();
}
	static const float radius=50.f;
	static const float height=150.f;

HRESULT SimulPrecipitationRendererDX1x::Render()
{
	PIXBeginNamedEvent(0,"Render Precipitation");
	rainTexture->SetResource(rain_texture);
/*	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
*/
	m_pImmediateContext->IASetInputLayout( m_pVtxDecl );

/*	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
*/
	D3DXMatrixIdentity(&world);
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	worldViewProj->SetMatrix((const float *)(&tmp1));
	offset->SetFloat(offs);
	intensity->SetFloat(rain_intensity);

	lightColour->SetFloatVector((const float*)(light_colour));

	UINT passes=1;
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		ApplyPass(m_hTechniqueRain->GetPassByIndex(i));
		
		m_pImmediateContext->IASetInputLayout( m_pVtxDecl );
		UINT stride = sizeof(Vertex_t);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = 0;
		Offsets[0] = 0;
		m_pImmediateContext->IASetVertexBuffers(	0,					// the first input slot for binding
													1,					// the number of buffers in the array
													&m_pVertexBuffer,	// the array of vertex buffers
													&stride,			// array of stride values, one for each buffer
													&offset );

		m_pImmediateContext->IASetInputLayout(m_pVtxDecl);

		m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_pImmediateContext->Draw(NUM_VERT-2,0);
	}
	//hr=m_pRainEffect->End();
	//m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  //  hr=m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
	D3DXMatrixIdentity(&world);
	PIXEndNamedEvent();
	return S_OK;
}

void SimulPrecipitationRendererDX1x::SetLightColour(const float c[4])
{
	static float cc=0.05f;
	light_colour[0]=cc*c[0];
	light_colour[1]=cc*c[1];
	light_colour[2]=cc*c[2];
	light_colour[3]=1.f;
}

void SimulPrecipitationRendererDX1x::SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	world=w;
	view=v;
	proj=p;
}

void SimulPrecipitationRendererDX1x::Update(float dt)
{
	static bool pause=false;
		static float cc=1.6f;
    if(!pause)
	{
		offs+=cc*dt;
	}
}