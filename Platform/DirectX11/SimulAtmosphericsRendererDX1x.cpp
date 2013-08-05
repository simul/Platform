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

using namespace simul;
using namespace dx11;

SimulAtmosphericsRendererDX1x::SimulAtmosphericsRendererDX1x() :
	m_pd3dDevice(NULL)
	,vertexDecl(NULL)
	,effect(NULL)
	,lightDir(NULL)
	,constantBuffer(NULL)
	,atmosphericsUniforms2ConstantsBuffer(NULL)
	,MieRayleighRatio(NULL)
	,HazeEccentricity(NULL)
	,fadeInterp(NULL)
	,depthTexture(NULL)
	,lossTexture(NULL)
	,inscTexture(NULL)
	,skylTexture(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
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
	
	invViewProj			=effect->GetVariableByName("invViewProj")->AsMatrix();
	lightDir			=effect->GetVariableByName("lightDir")->AsVector();
	MieRayleighRatio	=effect->GetVariableByName("MieRayleighRatio")->AsVector();
	HazeEccentricity	=effect->GetVariableByName("HazeEccentricity")->AsScalar();
	fadeInterp			=effect->GetVariableByName("fadeInterp")->AsScalar();
	depthTexture		=effect->GetVariableByName("depthTexture")->AsShaderResource();
	lossTexture			=effect->GetVariableByName("lossTexture")->AsShaderResource();
	inscTexture			=effect->GetVariableByName("inscTexture")->AsShaderResource();
	skylTexture			=effect->GetVariableByName("skylTexture")->AsShaderResource();
	
	MAKE_CONSTANT_BUFFER(constantBuffer,AtmosphericsUniforms);
	MAKE_CONSTANT_BUFFER(atmosphericsUniforms2ConstantsBuffer,AtmosphericsPerViewConstants);
}

void SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(void* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	RecompileShaders();
}

void SimulAtmosphericsRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(vertexDecl);
	SAFE_RELEASE(effect);
	SAFE_RELEASE(constantBuffer);
	SAFE_RELEASE(atmosphericsUniforms2ConstantsBuffer);
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

	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::RenderAsOverlay");
	ID3D11DeviceContext* m_pImmediateContext=(ID3D11DeviceContext*)context;
	ID3D1xShaderResourceView* depthTexture_SRV=(ID3D1xShaderResourceView*)depthTexture;
	
	lossTexture->SetResource(skyLossTexture_SRV);
	inscTexture->SetResource(overcInscTexture_SRV);
	skylTexture->SetResource(skylightTexture_SRV);
	
	simul::dx11::setParameter(effect,"illuminationTexture",illuminationTexture_SRV);
	simul::dx11::setParameter(effect,"depthTexture",depthTexture_SRV);
	simul::dx11::setParameter(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);

	cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;

	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	D3DXMATRIX p1=proj;
	if(ReverseDepth)
	{
		// Convert the proj matrix into a normal non-reversed matrix.
		p1=simul::dx11::ConvertReversedToRegularProjectionMatrix(proj);
	}

	AtmosphericsPerViewConstants atmosphericsPerViewConstants;
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,view,p1, relativeViewportTextureRegionXYWH);
	UPDATE_CONSTANT_BUFFER(m_pImmediateContext,atmosphericsUniforms2ConstantsBuffer,AtmosphericsPerViewConstants,atmosphericsPerViewConstants)
	ID3DX11EffectConstantBuffer* cbAtmosphericsUniforms2=effect->GetConstantBufferByName("AtmosphericsPerViewConstants");
	if(cbAtmosphericsUniforms2)
		cbAtmosphericsUniforms2->SetConstantBuffer(atmosphericsUniforms2ConstantsBuffer);
	
	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();

	{
		AtmosphericsUniforms atmosphericsUniforms;
		SetAtmosphericsConstants(atmosphericsUniforms,exposure,simul::sky::float4(1.0,1.0,1.0,0.0));
		UPDATE_CONSTANT_BUFFER(m_pImmediateContext,constantBuffer,AtmosphericsUniforms,atmosphericsUniforms)
		ID3DX11EffectConstantBuffer* cbAtmosphericsUniforms=effect->GetConstantBufferByName("AtmosphericsUniforms");
		if(cbAtmosphericsUniforms)
			cbAtmosphericsUniforms->SetConstantBuffer(constantBuffer);
	}
	
	ApplyPass(m_pImmediateContext,twoPassOverlayTechnique->GetPassByIndex(0));
	simul::dx11::UtilityRenderer::DrawQuad(m_pImmediateContext);
	ApplyPass(m_pImmediateContext,twoPassOverlayTechnique->GetPassByIndex(1));
	simul::dx11::UtilityRenderer::DrawQuad(m_pImmediateContext);
	
	lossTexture->SetResource(NULL);
	inscTexture->SetResource(NULL);
	skylTexture->SetResource(NULL);
	PIXEndNamedEvent();
}

void SimulAtmosphericsRendererDX1x::RenderGodrays(void *context,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH)
{
	if(!ShowGodrays)
		return;
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	ID3D11ShaderResourceView* depthTexture_SRV=(ID3D1xShaderResourceView*)depthTexture;
	lossTexture->SetResource(skyLossTexture_SRV);
	inscTexture->SetResource(skyInscatterTexture_SRV);
	skylTexture->SetResource(skylightTexture_SRV);
	simul::dx11::setParameter(effect,"illuminationTexture",illuminationTexture_SRV);
	simul::dx11::setParameter(effect,"depthTexture",depthTexture_SRV);
	simul::dx11::setParameter(effect,"overcTexture",overcInscTexture_SRV);
	simul::dx11::setParameter(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)cloudShadowStruct.texture);
	twoPassOverlayTechnique	=effect->GetTechniqueByName("simul_atmospherics_overlay");
	
	simul::geometry::SimulOrientation or;
	simul::math::Vector3 north(0.f,1.f,0.f);
	simul::math::Vector3 toSun(skyInterface->GetDirectionToSun());
	or.DefineFromYZ(north,toSun);
	or.SetPosition((const float*)cam_pos);
	AtmosphericsPerViewConstants atmosphericsPerViewConstants;
	D3DXMATRIX p1=proj;
	if(ReverseDepth)
	{
		// Convert the proj matrix into a normal non-reversed matrix.
		p1=simul::dx11::ConvertReversedToRegularProjectionMatrix(proj);
	}
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,view,p1,relativeViewportTextureRegionXYWH);

	simul::math::Matrix4x4 shadowMatrix					=cloudShadowStruct.shadowMatrix;
	simul::math::Matrix4x4 invShadowMatrix;
	shadowMatrix.Inverse(invShadowMatrix);
	atmosphericsPerViewConstants.invShadowMatrix		=invShadowMatrix;

//atmosphericsPerViewConstants.extentZMetres			=cloudShadowStruct.extentZMetres;
	atmosphericsPerViewConstants.startZMetres			=cloudShadowStruct.startZMetres;
	atmosphericsPerViewConstants.shadowRange			=cloudShadowStruct.shadowRange;

	UPDATE_CONSTANT_BUFFER(pContext,atmosphericsUniforms2ConstantsBuffer,AtmosphericsPerViewConstants,atmosphericsPerViewConstants)
	ID3DX11EffectConstantBuffer* cbAtmosphericsUniforms2=effect->GetConstantBufferByName("AtmosphericsPerViewConstants");
	if(cbAtmosphericsUniforms2)
		cbAtmosphericsUniforms2->SetConstantBuffer(atmosphericsUniforms2ConstantsBuffer);

	ApplyPass(pContext,twoPassOverlayTechnique->GetPassByName("godrays"));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
}