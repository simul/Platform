// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRendererDX11.cpp A renderer for 2D cloud layers.
#define NOMINMAX
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
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/DirectX11/Profiler.h"

using namespace simul;
using namespace dx11;

Simul2DCloudRendererDX11::Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem) :
	simul::clouds::Base2DCloudRenderer(ck,mem)
	,m_pd3dDevice(NULL)
	,effect(NULL)
	,tech(NULL)
	,vertexBuffer(NULL)
	,indexBuffer(NULL)
	,inputLayout(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
	,skylightTexture_SRV(NULL)
	,illuminationTexture_SRV(NULL)
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
		(UINT)(vertices.size()*sizeof(simul::clouds::Cloud2DGeometryHelper::Vertex)),
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
		num_indices+=(int)quads[i].indices.size()+2;
	num_indices+=((int)quads.size()-1)*2;
	unsigned short *indices=new unsigned short[num_indices];
	int n=0;
	for(int i=0;i<(int)quads.size();i++)
	{
		if(i)
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

	cloud2DConstants.RestoreDeviceObjects(m_pd3dDevice);
	detail2DConstants.RestoreDeviceObjects(m_pd3dDevice);

	coverage_fb.RestoreDeviceObjects(m_pd3dDevice);

	detail_fb.SetWidthAndHeight(256,256);
	detail_fb.RestoreDeviceObjects(m_pd3dDevice);
	
	noise_fb.RestoreDeviceObjects(m_pd3dDevice);
	noise_fb.SetWidthAndHeight(16,16);
	noise_fb.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);

	dens_fb.RestoreDeviceObjects(m_pd3dDevice);
	dens_fb.SetWidthAndHeight(512,512);
	dens_fb.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
}

void Simul2DCloudRendererDX11::RecompileShaders()
{
	SAFE_RELEASE(effect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	CreateEffect(m_pd3dDevice,&effect,"simul_clouds_2d.fx",defines);
	tech=effect->GetTechniqueByName("simul_clouds_2d");
	cloud2DConstants.LinkToEffect(effect,"Cloud2DConstants");
	detail2DConstants.LinkToEffect(effect,"Detail2DConstants");
}

void Simul2DCloudRendererDX11::RenderDetailTexture(void *context)
{
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
    ProfileBlock profileBlock(pContext,"Simul2DCloudRendererDX11::RenderDetailTexture");

	int noise_texture_size		=cloudKeyframer->GetEdgeNoiseTextureSize();
	int noise_texture_frequency	=cloudKeyframer->GetEdgeNoiseFrequency();
	
	noise_fb.SetWidthAndHeight(noise_texture_frequency,noise_texture_frequency);
	noise_fb.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	noise_fb.Activate(pContext);
	{
	//	ProfileBlock profileBlock(pContext,"Simul2DCloudRendererDX11::RenderDetailTexture noise");
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("simul_random");
		t->GetPassByIndex(0)->Apply(0,pContext);
		noise_fb.DrawQuad(pContext);
	} 
	noise_fb.Deactivate(pContext);
	dens_fb.SetWidthAndHeight(noise_texture_size,noise_texture_size);
	dens_fb.Activate(context);
	{
	//	ProfileBlock profileBlock(pContext,"Simul2DCloudRendererDX11::RenderDetailTexture dens");
		SetDetail2DCloudConstants(detail2DConstants);
		detail2DConstants.Apply(pContext);
		simul::dx11::setParameter(effect,"imageTexture"	,(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("simul_2d_cloud_detail");
		t->GetPassByIndex(0)->Apply(0,pContext);
		dens_fb.DrawQuad(context);
	}
	dens_fb.Deactivate(context);
	detail_fb.Activate(context);
	{
	//	ProfileBlock profileBlock(pContext,"Simul2DCloudRendererDX11::RenderDetailTexture lighting");
		simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)dens_fb.GetColorTex());
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("simul_2d_cloud_detail_lighting");
		t->GetPassByIndex(0)->Apply(0,pContext);
		detail_fb.DrawQuad(context);
	}
	detail_fb.Deactivate(context);
	
	coverage_fb.Activate(pContext);
	{
	//	ProfileBlock profileBlock(pContext,"Simul2DCloudRendererDX11::RenderDetailTexture coverage");
		simul::dx11::setParameter(effect,"noiseTexture",(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("simul_coverage");
		t->GetPassByIndex(0)->Apply(0,pContext);
		coverage_fb.DrawQuad(pContext);
	} 
	coverage_fb.Deactivate(pContext);
}

void Simul2DCloudRendererDX11::InvalidateDeviceObjects()
{
	SAFE_RELEASE(effect);
	SAFE_RELEASE(vertexBuffer);
	m_pd3dDevice=NULL;
	SAFE_RELEASE(inputLayout);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	cloud2DConstants.InvalidateDeviceObjects();
	detail2DConstants.InvalidateDeviceObjects();
	detail_fb.InvalidateDeviceObjects();
	noise_fb.InvalidateDeviceObjects();
	dens_fb.InvalidateDeviceObjects();
	
	coverage_fb.InvalidateDeviceObjects();
}

void Simul2DCloudRendererDX11::EnsureCorrectTextureSizes()
{
	simul::clouds::CloudKeyframer::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	coverage_fb.SetWidthAndHeight(width_x,length_y);
	if(cloud_tex_width_x==width_x&&cloud_tex_length_y==length_y&&cloud_tex_depth_z==1)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=1;
	//for(int i=0;i<3;i++)
	//	coverage_tex[i].init(m_pd3dDevice,cloud_tex_width_x,cloud_tex_length_y,DXGI_FORMAT_R8G8B8A8_UNORM);
}

void Simul2DCloudRendererDX11::EnsureTexturesAreUpToDate(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
    ProfileBlock profileBlock(pContext,"Simul2DCloudRendererDX11::EnsureTexturesAreUpToDate");
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
}

void Simul2DCloudRendererDX11::EnsureTextureCycle()
{
	int cyc=(cloudKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
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

void Simul2DCloudRendererDX11::PreRenderUpdate(void *context)
{
	EnsureTexturesAreUpToDate(context);
	RenderDetailTexture(context);
}

bool Simul2DCloudRendererDX11::Render(void *context,float exposure,bool cubemap,const void *depthTexture,bool default_fog,bool write_alpha,int viewport_id,const simul::sky::float4& viewportTextureRegionXYWH)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
    ProfileBlock profileBlock(pContext,"Simul2DCloudRendererDX11::Render");
	
	ID3D1xShaderResourceView* depthTexture_SRV=(ID3D1xShaderResourceView*)depthTexture;

	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)detail_fb.GetColorTex());
	simul::dx11::setParameter(effect,"noiseTexture",(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
	simul::dx11::setParameter(effect,"coverageTexture",(ID3D11ShaderResourceView*)coverage_fb.GetColorTex());
	simul::dx11::setParameter(effect,"lossTexture",skyLossTexture_SRV);
	simul::dx11::setParameter(effect,"inscTexture",skyInscatterTexture_SRV);
	simul::dx11::setParameter(effect,"skylTexture",skylightTexture_SRV);
	simul::dx11::setParameter(effect,"depthTexture",depthTexture_SRV);
	simul::dx11::setParameter(effect,"illuminationTexture",illuminationTexture_SRV);
	
	static float ff=10000.f; 
	cam_pos=simul::dx11::GetCameraPosVector(view,false);
	Set2DCloudConstants(cloud2DConstants,view,proj,exposure,viewportTextureRegionXYWH);
	cloud2DConstants.Apply(pContext);

	ID3D11InputLayout* previousInputLayout;
	UINT prevOffset;
	DXGI_FORMAT prevFormat;
	ID3D11Buffer* pPrevBuffer;
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;

	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IAGetInputLayout(&previousInputLayout);
	pContext->IAGetIndexBuffer(&pPrevBuffer, &prevFormat, &prevOffset);

	pContext->IASetInputLayout(inputLayout);
	SET_VERTEX_BUFFER(pContext,vertexBuffer,simul::clouds::Cloud2DGeometryHelper::Vertex);
	pContext->IASetIndexBuffer(indexBuffer,DXGI_FORMAT_R16_UINT,0);					


	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	pContext->DrawIndexed(num_indices-2,0,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout(previousInputLayout);
	pContext->IASetIndexBuffer(pPrevBuffer, prevFormat, prevOffset);

	SAFE_RELEASE(previousInputLayout)
	SAFE_RELEASE(pPrevBuffer);

	simul::dx11::setParameter(effect,"imageTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setParameter(effect,"noiseTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setParameter(effect,"coverageTexture"		,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setParameter(effect,"lossTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setParameter(effect,"inscTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setParameter(effect,"skylTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setParameter(effect,"depthTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setParameter(effect,"illuminationTexture"	,(ID3D11ShaderResourceView*)NULL);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	
	return true;
}

void Simul2DCloudRendererDX11::RenderCrossSections(void *context,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
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
	}
	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)coverage_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(pContext,(0)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(pContext,(1)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)dens_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(pContext,(2)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)detail_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(pContext,(3)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("show_detail_texture"));
		
}

void Simul2DCloudRendererDX11::SetLossTexture(void *t)
{
	skyLossTexture_SRV=(ID3D11ShaderResourceView*)t;
}

void Simul2DCloudRendererDX11::SetInscatterTextures(void* i,void *s,void *o)
{
	skyInscatterTexture_SRV=(ID3D11ShaderResourceView*)o;
	skylightTexture_SRV=(ID3D11ShaderResourceView*)s;
}

void Simul2DCloudRendererDX11::SetIlluminationTexture(void *i)
{
	illuminationTexture_SRV=(ID3D1xShaderResourceView*)i;
}

void Simul2DCloudRendererDX11::SetWindVelocity(float x,float y)
{
}
