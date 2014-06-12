// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRendererDX1x.cpp A renderer for skies, clouds and weather effects.
#define NOMINMAX
#include "SimulAtmosphericsRendererDX1x.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"
#include <tchar.h>
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#endif
#include <string>
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRendererDX1x.h"
#include "PrecipitationRenderer.h"
#include "Simul2DCloudRendererDX1x.h"
#include "SimulSkyRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

SimulAtmosphericsRendererDX1x::SimulAtmosphericsRendererDX1x(simul::base::MemoryInterface *m)
	:BaseAtmosphericsRenderer(m)
	,m_pd3dDevice(NULL)
	,effect(NULL)
	,depthTexture(NULL)
	,cloudDepthTexture(NULL)
	,lossTexture(NULL)
	,inscTexture(NULL)
	,skylTexture(NULL)
	,illuminationTexture(NULL)
	,overcTexture(NULL)
	,cloudShadowTexture(NULL)
	,cloudGodraysTexture(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
	,overcInscTexture_SRV(NULL)
	,skylightTexture_SRV(NULL)
	,illuminationTexture_SRV(NULL)
	,rainbowLookupTexture(NULL)
	,coronaLookupTexture(NULL)
{
}

SimulAtmosphericsRendererDX1x::~SimulAtmosphericsRendererDX1x()
{
	Destroy();
}

void SimulAtmosphericsRendererDX1x::SetLossTexture(void* t)
{
	skyLossTexture_SRV=(ID3D11ShaderResourceView*)t;
}

void SimulAtmosphericsRendererDX1x::SetInscatterTextures(void* i,void *s,void *o)
{
	skyInscatterTexture_SRV=(ID3D11ShaderResourceView*)i;
	skylightTexture_SRV=(ID3D11ShaderResourceView*)s;
	overcInscTexture_SRV=(ID3D11ShaderResourceView*)o;
}

void SimulAtmosphericsRendererDX1x::SetIlluminationTexture(void *i)
{
	illuminationTexture_SRV=(ID3D11ShaderResourceView*)i;
}

void SimulAtmosphericsRendererDX1x::SetCloudsTexture(void *)
{
}

void SimulAtmosphericsRendererDX1x::RecompileShaders()
{
	SAFE_RELEASE(effect);
	if(!m_pd3dDevice)
		return;
	HRESULT hr=S_OK;
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]=ReverseDepth?"1":"0";
	//defines["GODRAYS_STEPS"]=simul::base::stringFormat("%d",
	V_CHECK(CreateEffect(m_pd3dDevice,&effect,"atmospherics.fx",defines));

	godraysTechnique		=effect->GetTechniqueByName("fast_godrays");

	depthTexture			=effect->GetVariableByName("depthTexture")->AsShaderResource();
	cloudDepthTexture		=effect->GetVariableByName("cloudDepthTexture")->AsShaderResource();
	lossTexture				=effect->GetVariableByName("lossTexture")->AsShaderResource();
	inscTexture				=effect->GetVariableByName("inscTexture")->AsShaderResource();
	skylTexture				=effect->GetVariableByName("skylTexture")->AsShaderResource();

	illuminationTexture		=effect->GetVariableByName("illuminationTexture")->AsShaderResource();
	overcTexture			=effect->GetVariableByName("overcTexture")->AsShaderResource();
	cloudShadowTexture		=effect->GetVariableByName("cloudShadowTexture")->AsShaderResource();
	cloudGodraysTexture		=effect->GetVariableByName("cloudGodraysTexture")->AsShaderResource();
	atmosphericsPerViewConstants.LinkToEffect(effect,"AtmosphericsPerViewConstants");
	atmosphericsUniforms.LinkToEffect(effect,"AtmosphericsUniforms");
}

void SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(void* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D11Device*)dev;
	atmosphericsPerViewConstants.RestoreDeviceObjects(m_pd3dDevice);
	atmosphericsUniforms.RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
	SAFE_RELEASE(rainbowLookupTexture);
	SAFE_RELEASE(coronaLookupTexture);
	rainbowLookupTexture=simul::dx11::LoadTexture(m_pd3dDevice,"rainbow_scatter.png");
	coronaLookupTexture=simul::dx11::LoadTexture(m_pd3dDevice,"rainbow_diffraction_i_vs_a.png");
}

void SimulAtmosphericsRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(effect);
	atmosphericsPerViewConstants.InvalidateDeviceObjects();
	atmosphericsUniforms.InvalidateDeviceObjects();
	SAFE_RELEASE(rainbowLookupTexture);
	SAFE_RELEASE(coronaLookupTexture);
}

HRESULT SimulAtmosphericsRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return S_OK;
}

void SimulAtmosphericsRendererDX1x::SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p)
{
 	view=v;
	proj=p;
}

void SimulAtmosphericsRendererDX1x::RenderLoss(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass)
{
	HRESULT hr=S_OK;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	ID3D11ShaderResourceView* depthTexture_SRV=depthTexture->AsD3D11ShaderResourceView();
	lossTexture->SetResource(skyLossTexture_SRV);
	setTexture(effect,"illuminationTexture"	,illuminationTexture_SRV);
	setTexture(effect,"depthTexture"		,depthTexture_SRV);
	setTexture(effect,"depthTextureMS"		,depthTexture_SRV);
	setTexture(effect,"cloudShadowTexture"	,(ID3D11ShaderResourceView*)cloudShadowStruct.texture);
	sky::float4 cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);
	math::Matrix4x4 p1=proj;
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,1.f,view,p1,proj,relativeViewportTextureRegionXYWH);
	atmosphericsPerViewConstants.Apply(deviceContext);
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("loss");
	if(depthTexture_SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
		depthTexture_SRV->GetDesc(&depthDesc);
		if(depthTexture&&depthDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS)
			tech=effect->GetTechniqueByName("loss_msaa");
	}
	ApplyPass(pContext,tech->GetPassByName(near_pass?"near":"far"));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	lossTexture->SetResource(NULL);
	atmosphericsPerViewConstants.Unbind(pContext);
	atmosphericsUniforms.Unbind(pContext);
	ApplyPass(pContext,tech->GetPassByIndex(1));
}

void SimulAtmosphericsRendererDX1x::RenderInscatter(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass)
{
	HRESULT hr=S_OK;
	ID3D11DeviceContext *pContext				=deviceContext.asD3D11DeviceContext();
	ID3D11ShaderResourceView *depthTexture_SRV	=depthTexture->AsD3D11ShaderResourceView();
	
	lossTexture->SetResource(skyLossTexture_SRV);
	inscTexture->SetResource(overcInscTexture_SRV);
	skylTexture->SetResource(skylightTexture_SRV);
	
	setTexture(effect,"illuminationTexture",illuminationTexture_SRV);
	setTexture(effect,"depthTexture"		,depthTexture_SRV);
	setTexture(effect,"depthTextureMS"		,depthTexture_SRV);
	setTexture(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);
	sky::float4 cam_pos	=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;
	
	math::Matrix4x4 p1=proj;
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,exposure,view,p1,proj,relativeViewportTextureRegionXYWH);
	atmosphericsPerViewConstants.Apply(deviceContext);
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("inscatter_nearfardepth");
	if(depthTexture_SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
		depthTexture_SRV->GetDesc(&depthDesc);
		if(depthDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS)
			tech=effect->GetTechniqueByName("inscatter_msaa");
	}
	ApplyPass(pContext,tech->GetPassByName(near_pass?"near":"far"));
	SIMUL_GPU_PROFILE_START(pContext,"DrawQuad")
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	SIMUL_GPU_PROFILE_END(pContext)
	lossTexture->SetResource(NULL);
	inscTexture->SetResource(NULL);
	skylTexture->SetResource(NULL);
	atmosphericsPerViewConstants.Unbind(pContext);
	atmosphericsUniforms.Unbind(pContext);
	ApplyPass(pContext,tech->GetPassByIndex(0));
}

void SimulAtmosphericsRendererDX1x::RenderAsOverlay(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *hiResDepthTexture,float exposure
	,const simul::sky::float4& relativeViewportTextureRegionXYWH)
{
	HRESULT hr=S_OK;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	
	bool msaa=false;
	if(hiResDepthTexture)
	{
		ID3D11ShaderResourceView* depthTexture_SRV=hiResDepthTexture->AsD3D11ShaderResourceView();
		if(depthTexture_SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
			depthTexture_SRV->GetDesc(&depthDesc);
			msaa=(depthDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
		}
		if(msaa)
			simul::dx11::setTexture(effect,"depthTextureMS"		,depthTexture_SRV);
		else
			simul::dx11::setTexture(effect,"depthTexture"		,depthTexture_SRV);
	}
	lossTexture->SetResource(skyLossTexture_SRV);
	inscTexture->SetResource(overcInscTexture_SRV);
	skylTexture->SetResource(skylightTexture_SRV);
	
	simul::dx11::setTexture(effect,"illuminationTexture",illuminationTexture_SRV);
	simul::dx11::setTexture(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);

	sky::float4 cam_pos=simul::dx11::GetCameraPosVector(view,false);
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	math::Matrix4x4 p1=proj;

	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,exposure,view,p1,proj,relativeViewportTextureRegionXYWH);
	
	atmosphericsPerViewConstants.Apply(deviceContext);
	
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	ID3DX11EffectGroup *group=effect->GetGroupByName("atmospherics_overlay");
	ID3DX11EffectTechnique *tech=group->GetTechniqueByName("standard");
	
	if(msaa)
	{
		tech=group->GetTechniqueByName("msaa");
	}

	ApplyPass(pContext,tech->GetPassByIndex(0));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	ApplyPass(pContext,tech->GetPassByIndex(1));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	
	lossTexture->SetResource(NULL);
	inscTexture->SetResource(NULL);
	skylTexture->SetResource(NULL);
	atmosphericsPerViewConstants.Unbind(pContext);
	atmosphericsUniforms.Unbind(pContext);
	ApplyPass(pContext,tech->GetPassByIndex(1));
}

void SimulAtmosphericsRendererDX1x::RenderGodrays(crossplatform::DeviceContext &deviceContext,float strength,bool near_pass
												  ,crossplatform::Texture *depth_texture,float exposure
												  ,const simul::sky::float4& relativeViewportTextureRegionXYWH,crossplatform::Texture *cloud_depth_texture)
{
	if(!ShowGodrays)
		return;
	ID3D11DeviceContext* pContext=deviceContext.asD3D11DeviceContext();
	ID3D11ShaderResourceView* depthTexture_SRV		=depth_texture->AsD3D11ShaderResourceView();
	ID3D11ShaderResourceView* cloudDepthTexture_SRV	=cloud_depth_texture->AsD3D11ShaderResourceView();
	lossTexture			->SetResource(skyLossTexture_SRV);
	inscTexture			->SetResource(skyInscatterTexture_SRV);
	skylTexture			->SetResource(skylightTexture_SRV);
	illuminationTexture	->SetResource(illuminationTexture_SRV);
	depthTexture		->SetResource(depthTexture_SRV);
	cloudDepthTexture	->SetResource(cloudDepthTexture_SRV);
	overcTexture		->SetResource(overcInscTexture_SRV);
	cloudShadowTexture	->SetResource((ID3D11ShaderResourceView*)cloudShadowStruct.texture);
	cloudGodraysTexture	->SetResource((ID3D11ShaderResourceView*)cloudShadowStruct.godraysTexture);
	setTexture(effect,"moistureTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.moistureTexture);
	simul::geometry::SimulOrientation or;
	simul::math::Vector3 north(0.f,1.f,0.f);
	simul::math::Vector3 toSun(skyInterface->GetDirectionToSun());
	or.DefineFromYZ(north,toSun);
	sky::float4 cam_pos=dx11::GetCameraPosVector(view);
	or.SetPosition((const float*)cam_pos);
	math::Matrix4x4 p1=proj;
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,strength*exposure,view,p1,proj,relativeViewportTextureRegionXYWH);
	SetGodraysConstants(atmosphericsPerViewConstants,view);

	atmosphericsPerViewConstants.Apply(deviceContext);

	dx11::setTexture(effect,"rainbowLookupTexture"	,rainbowLookupTexture);
	dx11::setTexture(effect,"coronaLookupTexture"	,coronaLookupTexture);
//	dx11::setTexture(effect,"moistureTexture"		,(ID3D11ShaderResourceView*)moistureTexture);

	//D3DX11_TECHNIQUE_DESC desc;
	//tech->GetDesc(&desc);
	
	ApplyPass(pContext,godraysTechnique->GetPassByName(near_pass?"near":"far"));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);


	lossTexture			->SetResource(NULL);
	inscTexture			->SetResource(NULL);
	skylTexture			->SetResource(NULL);
	illuminationTexture	->SetResource(NULL);
	depthTexture		->SetResource(NULL);
	cloudDepthTexture	->SetResource(NULL);
	overcTexture		->SetResource(NULL);
	cloudShadowTexture	->SetResource(NULL);
	cloudGodraysTexture	->SetResource(NULL);
	//atmosphericsPerViewConstants.Unbind(pContext);
	//atmosphericsUniforms.Unbind(pContext);
	ApplyPass(pContext,godraysTechnique->GetPassByIndex(0));
}