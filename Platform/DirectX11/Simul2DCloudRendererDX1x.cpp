// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRendererDX11.cpp A renderer for 2D cloud layers.

#include "Simul2DCloudRendererdx1x.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/LicenseKey.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"
#include "Simul/Platform/CrossPlatform/simul_2d_clouds.hs"
#include "Simul/Platform/DirectX11/Utilities.h"

Simul2DCloudRendererDX11::Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck) :
	simul::clouds::Base2DCloudRenderer(ck)
	,m_pd3dDevice(NULL)
	,effect(NULL)
	,tech(NULL)
	,vertexBuffer(NULL)
	,indexBuffer(NULL)
	,inputLayout(NULL)
	,constantBuffer(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
	,skylightTexture_SRV(NULL)
{
}

Simul2DCloudRendererDX11::~Simul2DCloudRendererDX11()
{
	InvalidateDeviceObjects();
}

void Simul2DCloudRendererDX11::RestoreDeviceObjects(void* dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
    RecompileShaders();
	SAFE_RELEASE(inputLayout);
	if(tech)
	{
		D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT	,0,0	,D3D11_INPUT_PER_VERTEX_DATA,0},
		};
		D3D1x_PASS_DESC PassDesc;
		tech->GetPassByIndex(0)->GetDesc(&PassDesc);
		m_pd3dDevice->CreateInputLayout(decl,1, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &inputLayout);
	}
	static float max_cloud_distance=400000.f;
	helper->MakeDefaultGeometry(max_cloud_distance);
	const std::vector<simul::clouds::Cloud2DGeometryHelper::Vertex> &vertices=helper->GetVertices();
	simul::clouds::Cloud2DGeometryHelper::Vertex *v=new simul::clouds::Cloud2DGeometryHelper::Vertex[vertices.size()];
	for(int i=0;i<(int)vertices.size();i++)
		v[i]=vertices[i];
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData,sizeof(D3D1x_SUBRESOURCE_DATA));
    InitData.pSysMem	=v;
    InitData.SysMemPitch=sizeof(simul::clouds::Cloud2DGeometryHelper::Vertex);
	D3D11_BUFFER_DESC desc=
	{
        vertices.size()*sizeof(simul::clouds::Cloud2DGeometryHelper::Vertex),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,0
	};
	SAFE_RELEASE(vertexBuffer);
	m_pd3dDevice->CreateBuffer(&desc,&InitData,&vertexBuffer);
	delete v;
	const std::vector<simul::clouds::Cloud2DGeometryHelper::QuadStrip> &quads=helper->GetQuadStrips();
	num_indices=0;
	for(int i=0;i<(int)quads.size();i++)
		num_indices+=quads[i].indices.size()+2;
	unsigned short *indices=new unsigned short[num_indices];
	int n=0;
	for(int i=0;i<(int)quads.size();i++)
	{
		for(int j=0;j<2;j++)
			indices[n++]=quads[i].indices[j];
		for(int j=0;j<(int)quads[i].indices.size();j++)
			indices[n++]=quads[i].indices[j];
	}
	// index buffer
	D3D11_BUFFER_DESC indexBufferDesc=
	{
        num_indices*sizeof(unsigned short),
        D3D11_USAGE_DYNAMIC,
        D3D11_BIND_INDEX_BUFFER,
        D3D11_CPU_ACCESS_WRITE,
        0
	};
    ZeroMemory(&InitData,sizeof(D3D11_SUBRESOURCE_DATA));
    InitData.pSysMem		=indices;
    InitData.SysMemPitch	=sizeof(unsigned short);
	SAFE_RELEASE(indexBuffer);
	m_pd3dDevice->CreateBuffer(&indexBufferDesc,&InitData,&indexBuffer);
	delete [] indices;
	MAKE_CONSTANT_BUFFER(constantBuffer,Cloud2DConstants);
}

void Simul2DCloudRendererDX11::RecompileShaders()
{
	SAFE_RELEASE(effect);
	if(!m_pd3dDevice)
		return;
	CreateEffect(m_pd3dDevice,&effect,L"simul_clouds_2d.fx");
	tech=effect->GetTechniqueByName("simul_clouds_2d");
}

void Simul2DCloudRendererDX11::InvalidateDeviceObjects()
{
	SAFE_RELEASE(effect);
	SAFE_RELEASE(vertexBuffer);
	m_pd3dDevice=NULL;
	SAFE_RELEASE(inputLayout);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	SAFE_RELEASE(constantBuffer);
	for(int i=0;i<3;i++)
		coverage_tex[i].release();
}

void Simul2DCloudRendererDX11::EnsureCorrectTextureSizes()
{
	simul::clouds::CloudKeyframer::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	if(cloud_tex_width_x==width_x&&cloud_tex_length_y==length_y&&cloud_tex_depth_z==1
		&&coverage_tex[0].texture)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=1;
	for(int i=0;i<3;i++)
		coverage_tex[i].init(m_pd3dDevice,cloud_tex_width_x,cloud_tex_length_y,DXGI_FORMAT_R8G8B8A8_UNORM);
}

void Simul2DCloudRendererDX11::EnsureTexturesAreUpToDate(void *context)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	typedef simul::clouds::CloudKeyframer::seq_texture_fill iter;
	for(int i=0;i<3;i++)
	{
		iter texture_fill;
		while((texture_fill=cloudKeyframer->GetSequentialTextureFill(seq_texture_iterator[i])).num_texels!=0)
		{
			coverage_tex[i].setTexels(m_pImmediateContext,texture_fill.uint32_array,texture_fill.texel_index,texture_fill.num_texels);
		}
	}
}

void Simul2DCloudRendererDX11::EnsureTextureCycle()
{
	int cyc=(cloudKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(coverage_tex[0],coverage_tex[1]);
		std::swap(coverage_tex[1],coverage_tex[2]);
		std::swap(seq_texture_iterator[0],seq_texture_iterator[1]);
		std::swap(seq_texture_iterator[1],seq_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}

void Simul2DCloudRendererDX11::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

bool Simul2DCloudRendererDX11::Render(void*context,bool cubemap,void *depth_tex,bool default_fog,bool write_alpha)
{
	EnsureTexturesAreUpToDate(context);
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;

	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)detail_fb.GetColorTex());
	simul::dx11::setParameter(effect,"coverageTexture1",coverage_tex[0].shaderResourceView);
	simul::dx11::setParameter(effect,"coverageTexture2",coverage_tex[1].shaderResourceView);
	simul::dx11::setParameter(effect,"lossTexture",skyLossTexture_SRV);
	simul::dx11::setParameter(effect,"inscTexture",skyInscatterTexture_SRV);
	simul::dx11::setParameter(effect,"skylTexture",skylightTexture_SRV);
	
	D3DXMATRIX worldViewProj;
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);

	simul::dx11::MakeWorldViewProjMatrix(&worldViewProj,world,view,proj);
	D3DXMATRIX invw;
	D3DXMatrixInverse(&invw,NULL,&worldViewProj);
	static float ll=0.05f;
	static float ff=100.f; 
	Cloud2DConstants cloud2DConstants;
	simul::clouds::CloudInterface *ci=cloudKeyframer->GetCloudInterface();
	simul::math::Vector3 X1,X2;
	ci->GetExtents(X1,X2);
	simul::math::Vector3 DX=X2-X1;
	cloud2DConstants.worldViewProj			=worldViewProj;
	cloud2DConstants.worldViewProj.transpose();
	cloud2DConstants.origin					=X1+cloudKeyframer->GetCloudInterface()->GetWindOffset();
	cloud2DConstants.globalScale			=ci->GetCloudWidth();
	cloud2DConstants.detailScale			=ff*ci->GetFractalWavelength();
	cloud2DConstants.cloudEccentricity		=cloudKeyframer->GetInterpolatedKeyframe().light_asymmetry;
	cloud2DConstants.cloudInterp			=cloudKeyframer->GetInterpolation();
	cloud2DConstants.eyePosition			=cam_pos;
	if(skyInterface)
	{
		simul::sky::float4 amb					=skyInterface->GetAmbientLight(X1.z*.001f);
		cloud2DConstants.lightDir				=skyInterface->GetDirectionToLight(X1.z*0.001f);
		cloud2DConstants.lightResponse			=simul::sky::float4(ci->GetLightResponse(),0,0,ll*ci->GetSecondaryLightResponse());
		cloud2DConstants.maxFadeDistanceMetres	=max_fade_distance_metres;
		cloud2DConstants.sunlight				=skyInterface->GetLocalIrradiance(X1.z*0.001f);
		cloud2DConstants.hazeEccentricity		=skyInterface->GetMieEccentricity();
		cloud2DConstants.mieRayleighRatio		=skyInterface->GetMieRayleighRatio();
		cloud2DConstants.planetRadius			=6378000.f;
	}
	ID3D11InputLayout* previousInputLayout;
	m_pImmediateContext->IAGetInputLayout(&previousInputLayout);

	ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
	m_pImmediateContext->IASetInputLayout(inputLayout);
	SET_VERTEX_BUFFER(m_pImmediateContext,vertexBuffer,simul::clouds::Cloud2DGeometryHelper::Vertex);

	m_pImmediateContext->IASetIndexBuffer(indexBuffer,DXGI_FORMAT_R16_UINT,0);					

	UPDATE_CONSTANT_BUFFER(m_pImmediateContext,constantBuffer,Cloud2DConstants,cloud2DConstants);
	ID3D1xEffectConstantBuffer* cb=effect->GetConstantBufferByName("Cloud2DConstants");
	if(cb)
		cb->SetConstantBuffer(constantBuffer);

	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pImmediateContext->DrawIndexed(num_indices-2,0,0);
	m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
	m_pImmediateContext->IASetInputLayout(previousInputLayout);
	SAFE_RELEASE(previousInputLayout)
	
	return true;
}

void Simul2DCloudRendererDX11::RenderCrossSections(void *context,int width,int height)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext*)context;
	static int u=8;
	int w=(width-8)/u;
	if(w>height/2)
		w=height/2;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
static float mult=1.f;
	simul::dx11::UtilityRenderer::SetScreenSize(width,height);
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		simul::sky::float4 light_response(mult*kf->direct_light,mult*kf->indirect_light,mult*kf->ambient_light,0);

		simul::dx11::setParameter(effect,"imageTexture",coverage_tex[i].shaderResourceView);
		simul::dx11::setParameter(effect,"crossSectionOffset",GetCloudInterface()->GetWrap()?0.5f:0.f);
		simul::dx11::setParameter(effect,"lightResponse",light_response);
		simul::dx11::UtilityRenderer::RenderTexture(m_pImmediateContext,(i+1)*(w+8)+8,height-w-8,w,w,effect->GetTechniqueByName("simple"));
	}
	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)detail_fb.GetColorTex());

	simul::dx11::UtilityRenderer::RenderTexture(m_pImmediateContext,8,height-8-w,w,w,effect->GetTechniqueByName("simple"));
		
}

void Simul2DCloudRendererDX11::SetLossTexture(void *t)
{
	skyLossTexture_SRV=(ID3D11ShaderResourceView*)t;
}

void Simul2DCloudRendererDX11::SetInscatterTextures(void *t,void *s)
{
	skyInscatterTexture_SRV=(ID3D11ShaderResourceView*)t;
	skylightTexture_SRV=(ID3D11ShaderResourceView*)s;
}

void Simul2DCloudRendererDX11::SetWindVelocity(float x,float y)
{
}
