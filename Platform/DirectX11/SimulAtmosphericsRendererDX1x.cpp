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
#include <dxerr.h>
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
{
}

SimulAtmosphericsRendererDX1x::~SimulAtmosphericsRendererDX1x()
{
	Destroy();
}

void SimulAtmosphericsRendererDX1x::SetLossTexture(void* t)
{
	skyLossTexture_SRV=(ID3D1xShaderResourceView*)t;
}

void SimulAtmosphericsRendererDX1x::SetInscatterTextures(void* i,void *s,void *o)
{
	skyInscatterTexture_SRV=(ID3D1xShaderResourceView*)i;
	skylightTexture_SRV=(ID3D1xShaderResourceView*)s;
	overcInscTexture_SRV=(ID3D1xShaderResourceView*)o;
}

void SimulAtmosphericsRendererDX1x::SetIlluminationTexture(void *i)
{
	illuminationTexture_SRV=(ID3D1xShaderResourceView*)i;
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
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	//defines["GODRAYS_STEPS"]=simul::base::stringFormat("%d",
	V_CHECK(CreateEffect(m_pd3dDevice,&effect,"atmospherics.fx",defines));

	twoPassOverlayTechnique		=effect->GetTechniqueByName("simul_atmospherics_overlay");
	twoPassOverlayTechniqueMSAA	=effect->GetTechniqueByName("simul_atmospherics_overlay_msaa");

	godraysTechnique		=effect->GetTechniqueByName("fast_godrays");
	godraysNearPassTechnique=effect->GetTechniqueByName("near_depth_godrays");

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
	m_pd3dDevice=(ID3D1xDevice*)dev;
	atmosphericsPerViewConstants.RestoreDeviceObjects(m_pd3dDevice);
	atmosphericsUniforms.RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
}

void SimulAtmosphericsRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(effect);
	atmosphericsPerViewConstants.InvalidateDeviceObjects();
	atmosphericsUniforms.InvalidateDeviceObjects();
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

void SimulAtmosphericsRendererDX1x::RenderLoss(void *context,const void *depthTexture,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass)
{
	HRESULT hr=S_OK;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	ID3D11ShaderResourceView* depthTexture_SRV=(ID3D11ShaderResourceView*)depthTexture;
	lossTexture->SetResource(skyLossTexture_SRV);
	simul::dx11::setTexture(effect,"illuminationTexture",illuminationTexture_SRV);
	simul::dx11::setTexture(effect,"depthTexture"		,depthTexture_SRV);
	simul::dx11::setTexture(effect,"depthTextureMS"		,depthTexture_SRV);
	simul::dx11::setTexture(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);
	cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);
	D3DXMATRIX p1=proj;
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,view,p1,proj,relativeViewportTextureRegionXYWH);
	atmosphericsPerViewConstants.Apply(pContext);
	SetAtmosphericsConstants(atmosphericsUniforms,1.f,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(pContext);
	ID3D1xEffectTechnique *tech=effect->GetTechniqueByName("loss");
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

void SimulAtmosphericsRendererDX1x::RenderInscatter(void *context,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass)
{
	HRESULT hr=S_OK;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	ID3D11ShaderResourceView* depthTexture_SRV=(ID3D11ShaderResourceView*)depthTexture;
	
	lossTexture->SetResource(skyLossTexture_SRV);
	inscTexture->SetResource(overcInscTexture_SRV);
	skylTexture->SetResource(skylightTexture_SRV);
	
	simul::dx11::setTexture(effect,"illuminationTexture",illuminationTexture_SRV);
	simul::dx11::setTexture(effect,"depthTexture"		,depthTexture_SRV);
	simul::dx11::setTexture(effect,"depthTextureMS"		,depthTexture_SRV);
	simul::dx11::setTexture(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);
	cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);
	D3DXMATRIX p1=proj;
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,view,p1,proj,relativeViewportTextureRegionXYWH);
	atmosphericsPerViewConstants.Apply(pContext);
	SetAtmosphericsConstants(atmosphericsUniforms,exposure,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(pContext);
	ID3D1xEffectTechnique *tech=effect->GetTechniqueByName("inscatter");
	if(depthTexture_SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
		depthTexture_SRV->GetDesc(&depthDesc);
		if(depthTexture&&depthDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS)
			tech=effect->GetTechniqueByName("inscatter_msaa");
	}
	ApplyPass(pContext,tech->GetPassByName(near_pass?"near":"far"));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	lossTexture->SetResource(NULL);
	inscTexture->SetResource(NULL);
	skylTexture->SetResource(NULL);
	atmosphericsPerViewConstants.Unbind(pContext);
	atmosphericsUniforms.Unbind(pContext);
	ApplyPass(pContext,tech->GetPassByIndex(1));
}

void SimulAtmosphericsRendererDX1x::RenderAsOverlay(void *context,const void *depthTexture,float exposure
	,const simul::sky::float4& relativeViewportTextureRegionXYWH)
{
	HRESULT hr=S_OK;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	ID3D11ShaderResourceView* depthTexture_SRV=(ID3D11ShaderResourceView*)depthTexture;
	
	lossTexture->SetResource(skyLossTexture_SRV);
	inscTexture->SetResource(overcInscTexture_SRV);
	skylTexture->SetResource(skylightTexture_SRV);
	
	simul::dx11::setTexture(effect,"illuminationTexture",illuminationTexture_SRV);
	simul::dx11::setTexture(effect,"depthTexture"		,depthTexture_SRV);
	simul::dx11::setTexture(effect,"depthTextureMS"		,depthTexture_SRV);
	simul::dx11::setTexture(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);

	cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	D3DXMATRIX p1=proj;

	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,view,p1,proj,relativeViewportTextureRegionXYWH);
	
	atmosphericsPerViewConstants.Apply(pContext);

	SetAtmosphericsConstants(atmosphericsUniforms,exposure,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(pContext);
	
	ID3D1xEffectTechnique *tech=twoPassOverlayTechnique;
	
	if(depthTexture_SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
		depthTexture_SRV->GetDesc(&depthDesc);
		if(depthTexture&&depthDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS)
			tech=twoPassOverlayTechniqueMSAA;
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

void SimulAtmosphericsRendererDX1x::RenderGodrays(void *context,float strength,bool near_pass,const void *depth_texture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,const void *cloud_depth_texture)
{
	if(!ShowGodrays)
		return;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	ID3D11ShaderResourceView* depthTexture_SRV=(ID3D1xShaderResourceView*)depth_texture;
	ID3D11ShaderResourceView* cloudDepthTexture_SRV=(ID3D1xShaderResourceView*)cloud_depth_texture;
	lossTexture			->SetResource(skyLossTexture_SRV);
	inscTexture			->SetResource(skyInscatterTexture_SRV);
	skylTexture			->SetResource(skylightTexture_SRV);
	illuminationTexture	->SetResource(illuminationTexture_SRV);
	depthTexture		->SetResource(depthTexture_SRV);
	cloudDepthTexture	->SetResource(cloudDepthTexture_SRV);
	overcTexture		->SetResource(overcInscTexture_SRV);
	cloudShadowTexture	->SetResource((ID3D11ShaderResourceView*)cloudShadowStruct.texture);
	cloudGodraysTexture	->SetResource((ID3D11ShaderResourceView*)cloudShadowStruct.godraysTexture);
	simul::geometry::SimulOrientation or;
	simul::math::Vector3 north(0.f,1.f,0.f);
	simul::math::Vector3 toSun(skyInterface->GetDirectionToSun());
	or.DefineFromYZ(north,toSun);
	or.SetPosition((const float*)cam_pos);
	D3DXMATRIX p1=proj;
	SetAtmosphericsConstants(atmosphericsUniforms,strength*exposure,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(pContext);
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,view,p1,proj,relativeViewportTextureRegionXYWH);
	SetGodraysConstants(atmosphericsPerViewConstants,view);

	atmosphericsPerViewConstants.Apply(pContext);

	if(near_pass)
		ApplyPass(pContext,godraysNearPassTechnique->GetPassByIndex(0));
	else
		ApplyPass(pContext,godraysTechnique->GetPassByIndex(0));
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