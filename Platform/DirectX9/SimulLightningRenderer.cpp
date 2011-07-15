// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulLightningRenderer.h"

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
#include "Simul/Math/Vector3.h"
#include "Simul/Sky/Float4.h"
#include "Macros.h"
#include "CreateDX9Effect.h"
#include "Resources.h"

typedef std::basic_string<TCHAR> tstring;
using namespace simul::clouds;

SimulLightningRenderer::SimulLightningRenderer(simul::clouds::LightningRenderInterface *lightningRenderInterface) :
	simul::clouds::BaseLightningRenderer(lightningRenderInterface)
	,m_pLightningVtxDecl(NULL)
	,m_pLightningEffect(NULL)
	,lightning_vertices(NULL)
	,lightning_texture(NULL)
	,y_vertical(true)
{
}

SimulLightningRenderer::~SimulLightningRenderer()
{
}

bool SimulLightningRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	HRESULT hr;
	InitEffects();
	B_RETURN(CreateLightningTexture());
	return true;
}

bool SimulLightningRenderer::InitEffects()
{
	if(!m_pd3dDevice)
		return false;
	HRESULT hr;
	D3DVERTEXELEMENT9 std_decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 12, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pLightningVtxDecl);
	B_RETURN(m_pd3dDevice->CreateVertexDeclaration(std_decl,&m_pLightningVtxDecl))
	
	B_RETURN(CreateDX9Effect(m_pd3dDevice,m_pLightningEffect,"simul_lightning.fx"));
	m_hTechniqueLightningLines	=m_pLightningEffect->GetTechniqueByName("simul_lightning_lines");
	m_hTechniqueLightningQuads	=m_pLightningEffect->GetTechniqueByName("simul_lightning_quads");
	l_worldViewProj				=m_pLightningEffect->GetParameterByName(NULL,"worldViewProj");
	lightningColour				=m_pLightningEffect->GetParameterByName(NULL,"lightningColour");

	return true;
}

bool SimulLightningRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pLightningEffect)
        hr=m_pLightningEffect->OnLostDevice();
	SAFE_RELEASE(m_pLightningVtxDecl);
	SAFE_RELEASE(m_pLightningEffect);
	SAFE_RELEASE(lightning_texture);
	return true;
}

static D3DXVECTOR4 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR4 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}

bool SimulLightningRenderer::Render()
{
	if(!lightning_vertices)
		lightning_vertices=new PosTexVert_t[4500];

	HRESULT hr=S_OK;
bool y_vertical=true;
	m_pd3dDevice->SetTexture(0,lightning_texture);

	//m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	//m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1);
		
	//set up matrices
	D3DXMATRIX wvp;
	D3DXMatrixIdentity(&world);
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_WORLD,&world);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);

	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	D3DXVECTOR4 cam_pos=GetCameraPosVector(view);

    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetPixelShader( NULL );

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	hr=m_pd3dDevice->SetVertexDeclaration( m_pLightningVtxDecl );

	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

	simul::math::Vector3 pos;



	int vert_start=0;
	int vert_num=0;
	m_pLightningEffect->SetMatrix(l_worldViewProj,&wvp);
	m_pLightningEffect->SetTechnique(m_hTechniqueLightningQuads);
	UINT passes=1;
	for(unsigned i=0;i<lightningRenderInterface->GetNumLightSources();i++)
	{
		if(!lightningRenderInterface->IsSourceStarted(i))
			continue;
	/*	if(i==0)
			m_pLightningEffect->SetVector(lightningColour,&(D3DXVECTOR4(1,0,0,1)));
		if(i==1)
			m_pLightningEffect->SetVector(lightningColour,&(D3DXVECTOR4(0,1,0,1)));
		if(i==2)
			m_pLightningEffect->SetVector(lightningColour,&(D3DXVECTOR4(0,0,1,1)));
		if(i==3)
			m_pLightningEffect->SetVector(lightningColour,&(D3DXVECTOR4(1,1,1,1)));*/
		m_pLightningEffect->SetVector(lightningColour,(const D3DXVECTOR4*)lightningRenderInterface->GetLightningColour());
	hr=m_pLightningEffect->Begin(&passes,0);
	hr=m_pLightningEffect->BeginPass(0);
		bool quads=true;
		simul::math::Vector3 x1,x2;
		float bright1=0.f;
		simul::math::Vector3 camPos(cam_pos);
		lightningRenderInterface->GetSegmentVertex(i,0,0,bright1,x1.FloatPointer(0));
//		float dist=(x1-camPos).Magnitude();
		float vertical_shift=0;//helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(unsigned jj=0;jj<lightningRenderInterface->GetNumBranches(i);jj++)
		{
			if(jj==1)
			{
				hr=m_pLightningEffect->EndPass();
				hr=m_pLightningEffect->End();
				
				m_pLightningEffect->SetTechnique(m_hTechniqueLightningLines);
				quads=false;

				hr=m_pLightningEffect->Begin(&passes,0);
				hr=m_pLightningEffect->BeginPass(0);
			}
			simul::math::Vector3 last_transverse;
			vert_start=vert_num;
			for(unsigned k=0;k<lightningRenderInterface->GetNumSegments(i,jj)&&vert_num<4500;k++)
			{
				lightningRenderInterface->GetSegmentVertex(i,jj,k,bright1,x1.FloatPointer(0));
				simul::math::Vector3 dir;
				lightningRenderInterface->GetSegmentVertexDirection(i,jj,k,dir.FloatPointer(0));

				static float ww=50.f;
				float width=lightningRenderInterface->GetBranchWidth(i,jj);
				float width1=bright1*width;
				if(quads)
					width1*=ww;
				simul::math::Vector3 transverse;
				view_dir=x1-simul::math::Vector3(cam_pos.x,cam_pos.z,cam_pos.y);
				CrossProduct(transverse,view_dir,dir);
				transverse.Unit();
				transverse*=width1;
				simul::math::Vector3 t;//=transverse;
				if(!k)//||k==lightningRenderInterface->GetNumSegments(i,jj)-1)
					t*=0;
				else
					t=0.5f*(last_transverse+transverse);
				simul::math::Vector3 x1a=x1;//-t;
				if(quads)
					x1a=x1-t;
				simul::math::Vector3 x1b=x1+t;
				PosTexVert_t &v1=lightning_vertices[vert_num++];
				if(y_vertical)
				{
					std::swap(x1a.y,x1a.z);
					std::swap(x1b.y,x1b.z);
				}
				v1.position.x=x1a.x;
				v1.position.y=x1a.y+vertical_shift;
				v1.position.z=x1a.z;
				v1.texCoords.x=0;
				v1.texCoords.y=bright1;

				if(quads)
				{
					PosTexVert_t &v2=lightning_vertices[vert_num++];
					v2.position.x=x1b.x;
					v2.position.y=x1b.y+vertical_shift;
					v2.position.z=x1b.z;
					v2.texCoords.x=1.f;
					v2.texCoords.y=bright1;
				}
				else
					v1.texCoords.x=0.5f;
				last_transverse=transverse;
			}
			if(vert_num-vert_start>2)
			{
				if(quads)
				{
					hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,vert_num-vert_start-2,
					&(lightning_vertices[vert_start]),
					sizeof(PosTexVert_t));
				}
				else
				{
					hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINESTRIP,vert_num-vert_start-2,
					&(lightning_vertices[vert_start]),
					sizeof(PosTexVert_t));
				}
			}
		}
	hr=m_pLightningEffect->EndPass();
	hr=m_pLightningEffect->End();
	}
	return true;
}

HRESULT SimulLightningRenderer::CreateLightningTexture()
{
	HRESULT hr=S_OK;
	unsigned size=64;
	SAFE_RELEASE(lightning_texture);
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,1,0,D3DUSAGE_AUTOGENMIPMAP,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&lightning_texture)))
		return hr;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=lightning_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return hr;
	const float *lightning_colour=lightningRenderInterface->GetLightningColour();
	unsigned char *lightning_tex_data=(unsigned char *)(lockedRect.pBits);
	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float level=.5f*linear*linear+5.f*(linear>.97f);
		float r=lightning_colour[0]	*level;
		float g=lightning_colour[1]	*level;
		float b=lightning_colour[2]	*level;
		if(r>1.f)
			r=1.f;
		if(g>1.f)
			g=1.f;
		if(b>1.f)
			b=1.f;
		lightning_tex_data[4*i+0]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+1]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+2]=(unsigned char)(255.f*g);
		lightning_tex_data[4*i+3]=(unsigned char)(255.f*r);
	}
	hr=lightning_texture->UnlockRect(0);
	return hr;
}
