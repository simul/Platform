// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulTerrainRendererDX1x.h"

#include <dxerr.h>
#include <string>
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Sky/SkyInterface.h"
using namespace simul::dx11;

struct TerrainVertex_t
{
	float pos[3];
	float n[3];
	float t[2];
	float u;
};
static const int MAX_VERTICES=1000000;

SimulTerrainRendererDX1x::SimulTerrainRendererDX1x(simul::base::MemoryInterface *m)
	:BaseTerrainRenderer(m)
	,m_pd3dDevice(NULL)
	,m_pVertexBuffer(NULL)
	,m_pVtxDecl(NULL)
	,m_pTerrainEffect(NULL)
	,m_pTechnique(NULL)
{
}

SimulTerrainRendererDX1x::~SimulTerrainRendererDX1x()
{
	InvalidateDeviceObjects();
}

void SimulTerrainRendererDX1x::ReloadTextures()
{
	std::vector<std::string> texture_files;
	texture_files.push_back("terrain.png");
	texture_files.push_back("moss.png");
	arrayTexture.create(m_pd3dDevice,texture_files);
}

void SimulTerrainRendererDX1x::RecompileShaders()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pTerrainEffect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	V_CHECK(CreateEffect(m_pd3dDevice,&m_pTerrainEffect,("simul_terrain.fx"),defines));
	
	m_pTechnique		=m_pTerrainEffect->GetTechniqueByName("simul_terrain");

	ReloadTextures();

	terrainConstants.LinkToEffect(m_pTerrainEffect,"TerrainConstants");
	
}

void SimulTerrainRendererDX1x::RestoreDeviceObjects(void *dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	terrainConstants.RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
	const D3D1x_INPUT_ELEMENT_DESC decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	1, DXGI_FORMAT_R32G32_FLOAT,		0,	24,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	2, DXGI_FORMAT_R32_FLOAT,			0,	32,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
    };
	D3D1x_PASS_DESC PassDesc;
	ID3D1xEffectPass *pass=m_pTechnique->GetPassByIndex(0);
	hr=pass->GetDesc(&PassDesc);
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateInputLayout(decl,4,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
	// Create the main vertex buffer:
	D3D1x_BUFFER_DESC desc=
	{
        MAX_VERTICES*sizeof(TerrainVertex_t),
        D3D1x_USAGE_DYNAMIC,
        D3D1x_BIND_VERTEX_BUFFER,
        D3D1x_CPU_ACCESS_WRITE,
        0
	};
	TerrainVertex_t *vertices=new TerrainVertex_t[MAX_VERTICES];
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(TerrainVertex_t);
	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&m_pVertexBuffer);
	delete [] vertices;
}

void SimulTerrainRendererDX1x::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pTerrainEffect);
	SAFE_RELEASE(m_pVtxDecl);
	arrayTexture.release();
	terrainConstants.InvalidateDeviceObjects();
}

void SimulTerrainRendererDX1x::Render(void *context,float exposure)
{
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	D3DXMATRIX wvp;
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	simul::math::Vector3 cam_pos=simul::dx11::GetCameraPosVector(view,false);
	simul::dx11::setTextureArray(m_pTerrainEffect,"textureArray",arrayTexture.m_pArrayTexture_SRV);
	simul::dx11::setParameter(m_pTerrainEffect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);
	terrainConstants.eyePosition=cam_pos;
	if(baseSkyInterface)
	{
		terrainConstants.ambientColour	=exposure*baseSkyInterface->GetAmbientLight(0.f);
		terrainConstants.lightDir		=baseSkyInterface->GetDirectionToLight(0.f);
		terrainConstants.sunlight		=0.1f*exposure*baseSkyInterface->GetLocalIrradiance(0.f);
	}
	terrainConstants.worldViewProj=wvp;
	terrainConstants.worldViewProj.transpose();

	
	simul::math::Matrix4x4 shadowMatrix		=cloudShadowStruct.shadowMatrix;
	simul::math::Matrix4x4 invShadowMatrix;
	shadowMatrix.Inverse(invShadowMatrix);
	terrainConstants.invShadowMatrix		=invShadowMatrix;
	terrainConstants.extentZMetres			=cloudShadowStruct.extentZMetres;
	terrainConstants.startZMetres			=cloudShadowStruct.startZMetres;
	terrainConstants.shadowRange			=cloudShadowStruct.shadowRange;

	terrainConstants.Apply(pContext);

	TerrainVertex_t *vertices;
	D3D11_MAPPED_SUBRESOURCE mapped_vertices;
	MapBuffer(pContext,m_pVertexBuffer,&mapped_vertices);
	vertices=(TerrainVertex_t*)mapped_vertices.pData;
	
	int h	=heightMapInterface->GetPageSize();
	simul::math::Vector3 origin=heightMapInterface->GetOrigin();
	float PageWorldX=heightMapInterface->GetPageWorldX();
	float PageWorldY=heightMapInterface->GetPageWorldY();
	float PageSize	=(float)heightMapInterface->GetPageSize();
	
	int v=0;
	for(int i=0;i<h-1;i++)
	{
		float x1=(i  )*PageWorldX/(float)PageSize+origin.x;
		float x2=(i+1)*PageWorldX/(float)PageSize+origin.x;
		for(int j=0;j<h-1;j++)
		{
			int J=j;
			if(i%2)
				J=(h-2-j);
			float y	=(J)*PageWorldX/(float)PageSize+origin.y;
			float z1=heightMapInterface->GetHeightAt(i,J);
			float z2=heightMapInterface->GetHeightAt(i+1,J);
			simul::math::Vector3 X1(x1,y,z1);
			simul::math::Vector3 X2(x2,y,z2);
			if(v>=MAX_VERTICES)
				break;
			if(i%2==1)
				std::swap(X1,X2);
			{
				TerrainVertex_t &vertex=vertices[v];
				vertex.pos[0]	=X1.x;
				vertex.pos[1]	=X1.y;
				vertex.pos[2]	=X1.z;
				vertex.n[0]		=0.f;
				vertex.n[1]		=0.f;
				vertex.n[2]		=1.f;
			}
			v++;
			{
				TerrainVertex_t &vertex=vertices[v];
				vertex.pos[0]	=X2.x;
				vertex.pos[1]	=X2.y;
				vertex.pos[2]	=X2.z;
				vertex.n[0]		=0.f;
				vertex.n[1]		=0.f;
				vertex.n[2]		=1.f;
			}
			v++;
		}
	}
	UnmapBuffer(pContext,m_pVertexBuffer);
	UINT stride =sizeof(TerrainVertex_t);
	UINT offset =0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0]	=0;
    Offsets[0]	=0;
	pContext->IASetVertexBuffers(	0,					// the first input slot for binding
									1,					// the number of buffers in the array
									&m_pVertexBuffer,	// the array of vertex buffers
									&stride,			// array of stride values, one for each buffer
									&offset);			// array of offset values, one for each buffer
	ApplyPass(pContext,m_pTechnique->GetPassByIndex(0));
	// Set the input layout
	pContext->IASetInputLayout(m_pVtxDecl);
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	if((v)>2)
		pContext->Draw((v)-2,0);
	pContext->IASetPrimitiveTopology(previousTopology);
	simul::dx11::setTextureArray(m_pTerrainEffect,"textureArray",NULL);
	simul::dx11::setParameter(m_pTerrainEffect,"cloudShadowTexture",(ID3D11ShaderResourceView*)NULL);
	ApplyPass(pContext,m_pTechnique->GetPassByIndex(0));
}

void SimulTerrainRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}