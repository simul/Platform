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
#include "Simul/Platform/CrossPlatform/atmospherics_constants.sl"
#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#endif
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
#include "CreateEffectDX1x.h"
#include "Utilities.h"
using namespace simul;
using namespace dx11;
#define BLUR_SIZE 9
#define MONTE_CARLO_BLUR

#ifdef  MONTE_CARLO_BLUR
#include "Simul/Math/Pi.h"
#endif

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

void SimulAtmosphericsRendererDX1x::SetInscatterTextures(void* t,void *s)
{
	skyInscatterTexture_SRV=(ID3D1xShaderResourceView*)t;
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
	MAKE_CONSTANT_BUFFER(atmosphericsUniforms2ConstantsBuffer,AtmosphericsUniforms2);
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

#include "Simul/Camera/Camera.h"

void SimulAtmosphericsRendererDX1x::RenderAsOverlay(void *context,const void *depthTexture,float exposure)
{
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::RenderAsOverlay");
	ID3D11DeviceContext* m_pImmediateContext=(ID3D11DeviceContext*)context;
	ID3D1xShaderResourceView* depthTexture_SRV=(ID3D1xShaderResourceView*)depthTexture;
	
	HRESULT hr=S_OK;
	lossTexture->SetResource(skyLossTexture_SRV);
	inscTexture->SetResource(skyInscatterTexture_SRV);
	skylTexture->SetResource(skylightTexture_SRV);
	
	simul::dx11::setParameter(effect,"illuminationTexture",illuminationTexture_SRV);

	simul::dx11::setParameter(effect,"depthTexture",depthTexture_SRV);//(ID3D11ShaderResourceView*)framebuffer->GetDepthTex());
	simul::math::Matrix4x4 vpt;
	simul::math::Matrix4x4 viewproj;
	simul::math::Vector3 cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;

	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	D3DXMATRIX p1=proj;
	if(ReverseDepth)
	{
		// Convert the proj matrix into a normal non-reversed matrix.
		p1=simul::dx11::ConvertReversedToRegularProjectionMatrix(proj);
	}
	simul::math::Matrix4x4 v((const float *)view),p((const float*)p1);
	simul::math::Multiply4x4(viewproj,v,p);
	viewproj.Transpose(vpt);
	simul::math::Matrix4x4 ivp;
	vpt.Inverse(ivp);

	AtmosphericsUniforms2 atmosphericsUniforms2;
	atmosphericsUniforms2.invViewProj=ivp;
	atmosphericsUniforms2.invViewProj.transpose();
	atmosphericsUniforms2.tanHalfFov=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	atmosphericsUniforms2.nearZ=frustum.nearZ/fade_distance_km;
	atmosphericsUniforms2.farZ=frustum.farZ/fade_distance_km;

	UPDATE_CONSTANT_BUFFER(m_pImmediateContext,atmosphericsUniforms2ConstantsBuffer,AtmosphericsUniforms2,atmosphericsUniforms2)
	ID3DX11EffectConstantBuffer* cbAtmosphericsUniforms2=effect->GetConstantBufferByName("AtmosphericsUniforms2");
	if(cbAtmosphericsUniforms2)
		cbAtmosphericsUniforms2->SetConstantBuffer(atmosphericsUniforms2ConstantsBuffer);
	
	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();

	{
		AtmosphericsUniforms atmosphericsUniforms;
		float alt_km							=cam_pos.z/1000.f;
		atmosphericsUniforms.lightDir			=(const float*)skyInterface->GetDirectionToLight(alt_km);
		atmosphericsUniforms.mieRayleighRatio	=(const float*)skyInterface->GetMieRayleighRatio();
		atmosphericsUniforms.texelOffsets		=D3DXVECTOR2(0,0);
		atmosphericsUniforms.hazeEccentricity	=skyInterface->GetMieEccentricity();
		atmosphericsUniforms.exposure			=exposure;
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