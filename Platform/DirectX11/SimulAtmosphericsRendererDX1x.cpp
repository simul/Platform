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
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

SimulAtmosphericsRendererDX1x::SimulAtmosphericsRendererDX1x(simul::base::MemoryInterface *m)
	:BaseAtmosphericsRenderer(m)
	,m_pd3dDevice(NULL)
{
}

SimulAtmosphericsRendererDX1x::~SimulAtmosphericsRendererDX1x()
{
	Destroy();
}

void SimulAtmosphericsRendererDX1x::RecompileShaders()
{
	SAFE_DELETE(effect);
	if(!m_pd3dDevice)
		return;
	BaseAtmosphericsRenderer::RecompileShaders();
	HRESULT hr=S_OK;
}

void SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	HRESULT hr=S_OK;
	BaseAtmosphericsRenderer::RestoreDeviceObjects(r);
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	RecompileShaders();
}
HRESULT SimulAtmosphericsRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return S_OK;
}

void SimulAtmosphericsRendererDX1x::SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p)
{
}

void SimulAtmosphericsRendererDX1x::RenderLoss(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass)
{
	HRESULT hr=S_OK;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	ID3D11ShaderResourceView* depthTexture_SRV=depthTexture->AsD3D11ShaderResourceView();
	effect->SetTexture("lossTexture"		,skyLossTexture);
	effect->SetTexture("illuminationTexture",illuminationTexture);
	effect->SetTexture("depthTexture"		,depthTexture);
	effect->SetTexture("depthTextureMS"		,depthTexture);
	effect->SetTexture("cloudShadowTexture"	,cloudShadowStruct.texture);
	sky::float4 cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)deviceContext.viewStruct.proj);
	math::Matrix4x4 p1=deviceContext.viewStruct.proj;
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,1.f,deviceContext.viewStruct.view,p1,deviceContext.viewStruct.proj,relativeViewportTextureRegionXYWH);
	atmosphericsPerViewConstants.Apply(deviceContext);
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("loss");
	if(depthTexture_SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
		depthTexture_SRV->GetDesc(&depthDesc);
		if(depthTexture&&depthDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS)
			tech=effect->GetTechniqueByName("loss_msaa");
	}
	ApplyPass(pContext,tech->asD3DX11EffectTechnique()->GetPassByName(near_pass?"near":"far"));
	renderPlatform->DrawQuad(deviceContext);
	atmosphericsPerViewConstants.Unbind(deviceContext);
	atmosphericsUniforms.Unbind(deviceContext);
	ApplyPass(pContext,tech->asD3DX11EffectTechnique()->GetPassByIndex(1));
}

void SimulAtmosphericsRendererDX1x::RenderInscatter(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass)
{
	ID3D11DeviceContext *pContext				=deviceContext.asD3D11DeviceContext();
	
	effect->SetTexture("lossTexture",skyLossTexture);
	effect->SetTexture("inscTexture",overcInscTexture);
	effect->SetTexture("skylTexture",skylightTexture);
	
	effect->SetTexture("illuminationTexture"	,illuminationTexture);
	if(depthTexture->GetSampleCount()>0)
		effect->SetTexture("depthTextureMS"		,depthTexture);
	else
		effect->SetTexture("depthTexture"		,depthTexture);
	effect->SetTexture("cloudShadowTexture",cloudShadowStruct.texture);
	sky::float4 cam_pos	=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
	
	math::Matrix4x4 p1=deviceContext.viewStruct.proj;
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,exposure,deviceContext.viewStruct.view,p1,deviceContext.viewStruct.proj,relativeViewportTextureRegionXYWH);
	atmosphericsPerViewConstants.Apply(deviceContext);
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("inscatter_nearfardepth");
	if(depthTexture)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
		depthTexture->AsD3D11ShaderResourceView()->GetDesc(&depthDesc);
		if(depthDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS)
			tech=effect->GetTechniqueByName("inscatter_msaa");
	}
	effect->Apply(deviceContext,tech,near_pass?"near":"far");
	SIMUL_GPU_PROFILE_START(pContext,"DrawQuad")
	renderPlatform->DrawQuad(deviceContext);
	SIMUL_GPU_PROFILE_END(pContext)
	effect->SetTexture("lossTexture",NULL);
	effect->SetTexture("inscTexture",NULL);
	effect->SetTexture("skylTexture",NULL);
	atmosphericsPerViewConstants.Unbind(deviceContext);
	atmosphericsUniforms.Unbind(deviceContext);
	ApplyPass(pContext,tech->asD3DX11EffectTechnique()->GetPassByIndex(0));
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
			effect->SetTexture("depthTextureMS"		,hiResDepthTexture);
		else
			effect->SetTexture("depthTexture"		,hiResDepthTexture);
	}
	effect->SetTexture("lossTexture",skyLossTexture);
	effect->SetTexture("inscTexture",overcInscTexture);
	effect->SetTexture("skylTexture",skylightTexture);
	
	effect->SetTexture("illuminationTexture",illuminationTexture);
	effect->SetTexture("cloudShadowTexture",cloudShadowStruct.texture);

	sky::float4 cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)deviceContext.viewStruct.proj);

	math::Matrix4x4 p1=deviceContext.viewStruct.proj;

	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,exposure,deviceContext.viewStruct.view,p1,deviceContext.viewStruct.proj,relativeViewportTextureRegionXYWH);
	
	atmosphericsPerViewConstants.Apply(deviceContext);
	
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	ID3DX11EffectGroup *group=effect->asD3DX11Effect()->GetGroupByName("atmospherics_overlay");
	ID3DX11EffectTechnique *tech=group->GetTechniqueByName("standard");
	
	if(msaa)
	{
		tech=group->GetTechniqueByName("msaa");
	}

	ApplyPass(pContext,tech->GetPassByIndex(0));
	renderPlatform->DrawQuad(deviceContext);
	ApplyPass(pContext,tech->GetPassByIndex(1));
	renderPlatform->DrawQuad(deviceContext);
	
	effect->SetTexture("lossTexture",NULL);
	effect->SetTexture("inscTexture",NULL);
	effect->SetTexture("skylTexture",NULL);
	atmosphericsPerViewConstants.Unbind(deviceContext);
	atmosphericsUniforms.Unbind(deviceContext);
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
	effect->SetTexture("lossTexture",skyLossTexture);
	effect->SetTexture("inscTexture",skyInscatterTexture);
	effect->SetTexture("skylTexture",skylightTexture);
	effect->SetTexture("overcTexture",overcInscTexture);
	effect->SetTexture("illuminationTexture",illuminationTexture);
	effect->SetTexture("depthTexture",depth_texture);
	effect->SetTexture("cloudDepthTexture",cloud_depth_texture);
	effect->SetTexture("cloudShadowTexture",cloudShadowStruct.texture);
	effect->SetTexture("cloudGodraysTexture",cloudShadowStruct.godraysTexture);
	effect->SetTexture("moistureTexture",cloudShadowStruct.moistureTexture);
	simul::geometry::SimulOrientation or;
	simul::math::Vector3 north(0.f,1.f,0.f);
	simul::math::Vector3 toSun(skyInterface->GetDirectionToSun());
	or.DefineFromYZ(north,toSun);
	sky::float4 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
	or.SetPosition((const float*)cam_pos);
	math::Matrix4x4 p1=deviceContext.viewStruct.proj;
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,strength*exposure,deviceContext.viewStruct.view,p1,deviceContext.viewStruct.proj,relativeViewportTextureRegionXYWH);
	SetGodraysConstants(atmosphericsPerViewConstants,deviceContext.viewStruct.view);

	atmosphericsPerViewConstants.Apply(deviceContext);

	effect->SetTexture("rainbowLookupTexture",rainbowLookupTexture);
	effect->SetTexture("coronaLookupTexture",coronaLookupTexture);
	
	ApplyPass(pContext,godraysTechnique->asD3DX11EffectTechnique()->GetPassByName(near_pass?"near":"far"));
	renderPlatform->DrawQuad(deviceContext);

	effect->SetTexture("lossTexture",NULL);
	effect->SetTexture("inscTexture",NULL);
	effect->SetTexture("skylTexture",NULL);
	effect->SetTexture("overcTexture",NULL);
	effect->SetTexture("illuminationTexture",NULL);
	effect->SetTexture("depthTexture",NULL);
	effect->SetTexture("cloudDepthTexture",NULL);
	effect->SetTexture("cloudShadowTexture",NULL);
	effect->SetTexture("cloudGodraysTexture",NULL);
	effect->SetTexture("rainbowLookupTexture",NULL);
	effect->SetTexture("coronaLookupTexture",NULL);
	//atmosphericsPerViewConstants.Unbind(pContext);
	//atmosphericsUniforms.Unbind(pContext);
	ApplyPass(pContext,godraysTechnique->asD3DX11EffectTechnique()->GetPassByIndex(0));
}