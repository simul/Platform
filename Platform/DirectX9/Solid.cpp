#include "Solid.h"
#include <map>
#include "Simul/Platform/DirectX9/CreateDX9Effect.h"
Plane::Plane()
{
}

Plane::~Plane()
{
	InvalidateDeviceObjects();
}

void Plane::RestoreDeviceObjects(void *pd3dDevice)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)pd3dDevice;
	SAFE_RELEASE(m_pEffect);
	if(!m_pd3dDevice)
		return;
	
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	V_CHECK(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl));

	std::map<std::string,std::string> defines;
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pEffect,"plane.fx",defines));
	worldViewProj				=m_pEffect->GetParameterByName(NULL,"worldViewProj");
}

void Plane::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pEffect);
}
struct Vertex_t
{
	float x,y,z;
};
	static const float size=250.f;
	static Vertex_t vertices[36] =
	{
		{-size,		-size,	size},
		{size,		-size,	size},
		{size,		size,	size},
		{size,		size,	size},
		{-size,		size,	size},
		{-size,		-size,	size},
		
		{-size,		-size,	-size},
		{size,		-size,	-size},
		{size,		size,	-size},
		{size,		size,	-size},
		{-size,		size,	-size},
		{-size,		-size,	-size},
		
		{-size,		size,	-size},
		{size,		size,	-size},
		{size,		size,	size},
		{size,		size,	size},
		{-size,		size,	size},
		{-size,		size,	-size},
					
		{-size,		-size,  -size},
		{size,		-size,	-size},
		{size,		-size,	size},
		{size,		-size,	size},
		{-size,		-size,	size},
		{-size,		-size,  -size},
		
		{size,		-size,	-size},
		{size,		size,	-size},
		{size,		size,	size},
		{size,		size,	size},
		{size,		-size,	size},
		{size,		-size,	-size},
					
		{-size,		-size,	-size},
		{-size,		size,	-size},
		{-size,		size,	size},
		{-size,		size,	size},
		{-size,		-size,	size},
		{-size,		-size,	-size},
	};

void Plane::Render()
{
	D3DXMATRIX world,view,proj;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXMatrixIdentity(&world);
	//set up matrices
	world._41=pos.x;
	world._42=pos.y;
	world._43=pos.z;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));

	V_CHECK(m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl));

	//m_pSkyEffect->SetTechnique(m_hTechniqueSky);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);

	UINT passes=1;
	V_CHECK(m_pEffect->Begin( &passes, 0 ));
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		V_CHECK(m_pEffect->BeginPass(i));
		V_CHECK(m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,12,vertices,sizeof(Vertex_t)));
		V_CHECK(m_pEffect->EndPass());
	}
	V_CHECK(m_pEffect->End());
}