// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRendererDX1x.cpp A renderer for skies, clouds and weather effects.

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
#include "SimulPrecipitationRendererDX1x.h"
#include "Simul2DCloudRendererDX1x.h"
#include "SimulSkyRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Profiler.h"

using namespace simul;
using namespace dx11;

SimulAtmosphericsRendererDX1x::SimulAtmosphericsRendererDX1x(simul::base::MemoryInterface *m)
	:BaseAtmosphericsRenderer(m)
	,m_pd3dDevice(NULL)
	,vertexDecl(NULL)
	,effect(NULL)
	,lightDir(NULL)
	,constantBuffer(NULL)
	,atmosphericsUniforms2ConstantsBuffer(NULL)
	,MieRayleighRatio(NULL)
	,HazeEccentricity(NULL)
	,fadeInterp(NULL)
	,depthTexture(NULL)
	,cloudDepthTexture(NULL)
	,lossTexture(NULL)
	,inscTexture(NULL)
	,skylTexture(NULL)
	,illuminationTexture(NULL)
	,overcTexture(NULL)
	,cloudShadowTexture(NULL)
	,cloudNearFarTexture(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
	,overcInscTexture_SRV(NULL)
	,skylightTexture_SRV(NULL)
	,clouds_texture(NULL)
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
	overcInscTexture_SRV=(ID3D1xShaderResourceView*)o;
	skylightTexture_SRV=(ID3D1xShaderResourceView*)s;
}

void SimulAtmosphericsRendererDX1x::SetIlluminationTexture(void *i)
{
	illuminationTexture_SRV=(ID3D1xShaderResourceView*)i;
}

void SimulAtmosphericsRendererDX1x::SetCloudsTexture(void* t)
{
	clouds_texture=(ID3D1xShaderResourceView*)t;
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
	V_CHECK(CreateEffect(m_pd3dDevice,&effect,"atmospherics.fx",defines));
	singlePassTechnique		=effect->GetTechniqueByName("simul_atmospherics");
	twoPassOverlayTechnique	=effect->GetTechniqueByName("simul_atmospherics_overlay");
	godraysTechnique		=effect->GetTechniqueByName("simul_godrays");

	invViewProj				=effect->GetVariableByName("invViewProj")->AsMatrix();
	lightDir				=effect->GetVariableByName("lightDir")->AsVector();
	MieRayleighRatio		=effect->GetVariableByName("MieRayleighRatio")->AsVector();
	HazeEccentricity		=effect->GetVariableByName("HazeEccentricity")->AsScalar();
	fadeInterp				=effect->GetVariableByName("fadeInterp")->AsScalar();
	depthTexture			=effect->GetVariableByName("depthTexture")->AsShaderResource();
	cloudDepthTexture		=effect->GetVariableByName("cloudDepthTexture")->AsShaderResource();
	lossTexture				=effect->GetVariableByName("lossTexture")->AsShaderResource();
	inscTexture				=effect->GetVariableByName("inscTexture")->AsShaderResource();
	skylTexture				=effect->GetVariableByName("skylTexture")->AsShaderResource();

	illuminationTexture		=effect->GetVariableByName("illuminationTexture")->AsShaderResource();
	overcTexture			=effect->GetVariableByName("overcTexture")->AsShaderResource();
	cloudShadowTexture		=effect->GetVariableByName("cloudShadowTexture")->AsShaderResource();
	cloudNearFarTexture		=effect->GetVariableByName("cloudNearFarTexture")->AsShaderResource();
	
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
	SAFE_RELEASE(vertexDecl);
	SAFE_RELEASE(effect);
	SAFE_RELEASE(constantBuffer);
	SAFE_RELEASE(atmosphericsUniforms2ConstantsBuffer);
	atmosphericsPerViewConstants.InvalidateDeviceObjects();
	atmosphericsUniforms.InvalidateDeviceObjects();
}

HRESULT SimulAtmosphericsRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return S_OK;
}

void SimulAtmosphericsRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

void SimulAtmosphericsRendererDX1x::RenderAsOverlay(void *context,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH)
{
	HRESULT hr=S_OK;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
    ProfileBlock profileBlock(pContext,"Atmospherics:RenderAsOverlay");
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::RenderAsOverlay");
	ID3D1xShaderResourceView* depthTexture_SRV=(ID3D1xShaderResourceView*)depthTexture;
	
	lossTexture->SetResource(skyLossTexture_SRV);
	if(ShowGodrays)
		inscTexture->SetResource(overcInscTexture_SRV);
	else
		inscTexture->SetResource(skyInscatterTexture_SRV);
	skylTexture->SetResource(skylightTexture_SRV);
	
	simul::dx11::setParameter(effect,"illuminationTexture",illuminationTexture_SRV);
	simul::dx11::setParameter(effect,"depthTexture",depthTexture_SRV);
	simul::dx11::setParameter(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);

	cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;

	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	D3DXMATRIX p1=proj;

	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,view,p1,proj,relativeViewportTextureRegionXYWH);
	
	atmosphericsPerViewConstants.Apply(pContext);
	
	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();

	SetAtmosphericsConstants(atmosphericsUniforms,exposure,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(pContext);
	
	ApplyPass(pContext,twoPassOverlayTechnique->GetPassByIndex(0));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	ApplyPass(pContext,twoPassOverlayTechnique->GetPassByIndex(1));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	
	lossTexture->SetResource(NULL);
	inscTexture->SetResource(NULL);
	skylTexture->SetResource(NULL);
	PIXEndNamedEvent();
	atmosphericsPerViewConstants.Unbind(pContext);
	atmosphericsUniforms.Unbind(pContext);
	ApplyPass(pContext,twoPassOverlayTechnique->GetPassByIndex(1));
}

void SimulAtmosphericsRendererDX1x::RenderGodrays(void *context,float strength,const void *depth_texture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,const void *cloud_depth_texture)
{
	if(!ShowGodrays)
		return;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
    ProfileBlock profileBlock(pContext,"Atmospherics:RenderGodrays");
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
	cloudNearFarTexture	->SetResource((ID3D11ShaderResourceView*)cloudShadowStruct.nearFarTexture);
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
	cloudNearFarTexture	->SetResource(NULL);
	atmosphericsPerViewConstants.Unbind(pContext);
	atmosphericsUniforms.Unbind(pContext);
	ApplyPass(pContext,godraysTechnique->GetPassByIndex(0));
}