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
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Camera/Camera.h"

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
	num_indices+=(quads.size()-1)*2;
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

	detail_fb.SetWidthAndHeight(256,256);
	detail_fb.RestoreDeviceObjects(m_pd3dDevice);
	
	noise_fb.RestoreDeviceObjects(m_pd3dDevice);
	noise_fb.SetWidthAndHeight(16,16);
	noise_fb.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	ID3D11DeviceContext *pImmediateContext=NULL;
	m_pd3dDevice->GetImmediateContext(&pImmediateContext);
	noise_fb.Activate(pImmediateContext);
	{
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("simul_random");
		t->GetPassByIndex(0)->Apply(0,pImmediateContext);
		noise_fb.DrawQuad(pImmediateContext);
	}
	noise_fb.Deactivate(pImmediateContext);

	dens_fb.RestoreDeviceObjects(m_pd3dDevice);
	dens_fb.SetWidthAndHeight(512,512);
	dens_fb.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
SAFE_RELEASE(pImmediateContext);
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
}

void Simul2DCloudRendererDX11::RenderDetailTexture(void *context)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	
	dens_fb.Activate(context);
	{
		simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("simul_2d_cloud_detail");
		t->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
		simul::dx11::setParameter(effect,"persistence",cloudKeyframer->GetEdgeNoisePersistence());
		dens_fb.DrawQuad(context);
	}
	dens_fb.Deactivate(context);
	detail_fb.Activate(context);
	{
		simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)dens_fb.GetColorTex());
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("simul_2d_cloud_detail_lighting");
		t->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
		simul::dx11::setParameter(effect,"lightDir2d",skyInterface->GetDirectionToLight(cloudKeyframer->GetCloudInterface()->GetCloudBaseZ()/1000.f));
		//glUniform3f(lightDir,1.f,0.f,0.f);
		detail_fb.DrawQuad(context);
	}
	detail_fb.Deactivate(context);
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
	for(int i=0;i<3;i++)
		coverage_tex[i].release();
	detail_fb.InvalidateDeviceObjects();
	noise_fb.InvalidateDeviceObjects();
	dens_fb.InvalidateDeviceObjects();
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

void Simul2DCloudRendererDX11::Update(void *context)
{
}

bool Simul2DCloudRendererDX11::Render(void *context,float exposure,bool cubemap,const void *depthTexture,bool default_fog,bool write_alpha,int viewport_id,const simul::sky::float4& viewportTextureRegionXYWH)
{
	EnsureTexturesAreUpToDate(context);
	RenderDetailTexture(context);
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	ID3D1xShaderResourceView* depthTexture_SRV=(ID3D1xShaderResourceView*)depthTexture;

	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)detail_fb.GetColorTex());
	simul::dx11::setParameter(effect,"coverageTexture1",coverage_tex[0].shaderResourceView);
	simul::dx11::setParameter(effect,"coverageTexture2",coverage_tex[1].shaderResourceView);
	simul::dx11::setParameter(effect,"lossTexture",skyLossTexture_SRV);
	simul::dx11::setParameter(effect,"inscTexture",skyInscatterTexture_SRV);
	simul::dx11::setParameter(effect,"skylTexture",skylightTexture_SRV);
	simul::dx11::setParameter(effect,"depthTexture",depthTexture_SRV);
	simul::dx11::setParameter(effect,"illuminationTexture",illuminationTexture_SRV);
	
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	D3DXMATRIX wvp;
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	static float ll=0.05f;
	static float ff=100.f; 
	simul::clouds::CloudInterface *ci=cloudKeyframer->GetCloudInterface();
	simul::math::Vector3 X1,X2;
	ci->GetExtents(X1,X2);
	simul::math::Vector3 DX=X2-X1;
	cam_pos=simul::dx11::GetCameraPosVector(view,false);

	cloud2DConstants.viewportToTexRegionScaleBias = simul::sky::float4( viewportTextureRegionXYWH.z, viewportTextureRegionXYWH.w, viewportTextureRegionXYWH.x, viewportTextureRegionXYWH.y );
	cloud2DConstants.worldViewProj			=wvp;
	cloud2DConstants.worldViewProj.transpose();
	cloud2DConstants.origin					=X1+cloudKeyframer->GetCloudInterface()->GetWindOffset();
	cloud2DConstants.globalScale			=ci->GetCloudWidth();
	cloud2DConstants.detailScale			=ff*ci->GetFractalWavelength();
	cloud2DConstants.cloudEccentricity		=cloudKeyframer->GetInterpolatedKeyframe().light_asymmetry;
	cloud2DConstants.cloudInterp			=cloudKeyframer->GetInterpolation();
	cloud2DConstants.eyePosition			=cam_pos;
	
	simul::camera::Frustum frustum			=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);
	cloud2DConstants.tanHalfFov				=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	cloud2DConstants.nearZ					=frustum.nearZ/max_fade_distance_metres;
	cloud2DConstants.farZ					=frustum.farZ/max_fade_distance_metres;
	cloud2DConstants.exposure				=exposure;
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

	m_pImmediateContext->IASetInputLayout(inputLayout);
	SET_VERTEX_BUFFER(m_pImmediateContext,vertexBuffer,simul::clouds::Cloud2DGeometryHelper::Vertex);

	UINT prevOffset;
	DXGI_FORMAT prevFormat;
	ID3D11Buffer* pPrevBuffer;
	m_pImmediateContext->IAGetIndexBuffer(&pPrevBuffer, &prevFormat, &prevOffset);

	m_pImmediateContext->IASetIndexBuffer(indexBuffer,DXGI_FORMAT_R16_UINT,0);					

	cloud2DConstants.Apply(m_pImmediateContext);

	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
	m_pImmediateContext->DrawIndexed(num_indices-2,0,0);
	m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
	m_pImmediateContext->IASetInputLayout(previousInputLayout);
	m_pImmediateContext->IASetIndexBuffer(pPrevBuffer, prevFormat, prevOffset);
	SAFE_RELEASE(previousInputLayout)
	SAFE_RELEASE(pPrevBuffer);
	
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
		simul::dx11::UtilityRenderer::DrawQuad2(m_pImmediateContext,(i+4)*(w+8)+8,height-w-8,w,w,effect,effect->GetTechniqueByName("simple"));
	}
	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(m_pImmediateContext,(0)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)dens_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(m_pImmediateContext,(1)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
	simul::dx11::setParameter(effect,"imageTexture",(ID3D11ShaderResourceView*)detail_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(m_pImmediateContext,(2)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
		
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

void Simul2DCloudRendererDX11::SetIlluminationTexture(void *i)
{
	illuminationTexture_SRV=(ID3D1xShaderResourceView*)i;
}

void Simul2DCloudRendererDX11::SetWindVelocity(float x,float y)
{
}
