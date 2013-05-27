// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRendererDX1x.cpp A renderer for skies, clouds and weather effects.

#include "SimulAtmosphericsRendererDX1x.h"
#include "Simul/Platform/DirectX11/HLSL/AtmosphericsUniforms.hlsl"
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
	,framebuffer(NULL)
	,effect(NULL)
	,lightDir(NULL)
	,constantBuffer(NULL)
	,atmosphericsUniforms2ConstantsBuffer(NULL)
	,MieRayleighRatio(NULL)
	,HazeEccentricity(NULL)
	,fadeInterp(NULL)
	,imageTexture(NULL)
	,depthTexture(NULL)
	,lossTexture(NULL)
	,inscatterTexture(NULL)
	,skylightTexture(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
	,skylightTexture_SRV(NULL)
	,clouds_texture(NULL)
{
	framebuffer=new FramebufferDX1x(256,256);
}

SimulAtmosphericsRendererDX1x::~SimulAtmosphericsRendererDX1x()
{
	Destroy();
	delete framebuffer;
}

void SimulAtmosphericsRendererDX1x::SetBufferSize(int w,int h)
{
	if(framebuffer)
		framebuffer->SetWidthAndHeight(w,h);
}

void SimulAtmosphericsRendererDX1x::SetYVertical(bool y)
{
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
	V_CHECK(CreateEffect(m_pd3dDevice,&effect,L"atmospherics.fx",defines));
	technique			=effect->GetTechniqueByName("simul_atmospherics");
	invViewProj			=effect->GetVariableByName("invViewProj")->AsMatrix();
	lightDir			=effect->GetVariableByName("lightDir")->AsVector();
	MieRayleighRatio	=effect->GetVariableByName("MieRayleighRatio")->AsVector();
	HazeEccentricity	=effect->GetVariableByName("HazeEccentricity")->AsScalar();
	fadeInterp			=effect->GetVariableByName("fadeInterp")->AsScalar();
	imageTexture		=effect->GetVariableByName("imageTexture")->AsShaderResource();
	depthTexture		=effect->GetVariableByName("depthTexture")->AsShaderResource();
	lossTexture			=effect->GetVariableByName("lossTexture")->AsShaderResource();
	inscatterTexture	=effect->GetVariableByName("inscatterTexture")->AsShaderResource();
	skylightTexture		=effect->GetVariableByName("skylightTexture")->AsShaderResource();
	
	MAKE_CONSTANT_BUFFER(constantBuffer,AtmosphericsUniforms);
	MAKE_CONSTANT_BUFFER(atmosphericsUniforms2ConstantsBuffer,AtmosphericsUniforms2);
}

void SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(void* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	RecompileShaders();
	if(framebuffer)
		framebuffer->RestoreDeviceObjects(dev);
}

void SimulAtmosphericsRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(framebuffer)
		framebuffer->InvalidateDeviceObjects();
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

void SimulAtmosphericsRendererDX1x::StartRender(void *context)
{
	if(!framebuffer)
		return;
	ID3D11DeviceContext* m_pImmediateContext=(ID3D11DeviceContext*)context;
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::StartRender");
	framebuffer->Activate(m_pImmediateContext);
	// Clear the screen to black, with alpha=1, representing far depth
	framebuffer->Clear(context,0.f,0.f,0.f,1.f,ReverseDepth?0.f:1.f);

	PIXEndNamedEvent();
}
#include "Simul/Camera/Camera.h"
void SimulAtmosphericsRendererDX1x::FinishRender(void *context)
{
	if(!framebuffer)
		return;
	ID3D11DeviceContext* m_pImmediateContext=(ID3D11DeviceContext*)context;
	framebuffer->Deactivate(m_pImmediateContext);
	//RenderAsOverlay(context,NULL);
}

void SimulAtmosphericsRendererDX1x::RenderAsOverlay(void *context,const void *depthTexture)
{
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::RenderAsOverlay");
	ID3D11DeviceContext* m_pImmediateContext=(ID3D11DeviceContext*)context;
	ID3D1xShaderResourceView* depthTexture_SRV=(ID3D1xShaderResourceView*)depthTexture;
	
	HRESULT hr=S_OK;
	hr=imageTexture->SetResource(framebuffer->buffer_texture_SRV);
	lossTexture->SetResource(skyLossTexture_SRV);
	inscatterTexture->SetResource(skyInscatterTexture_SRV);
	skylightTexture->SetResource(skylightTexture_SRV);
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
	//ivp.Transpose();
	AtmosphericsUniforms2 atmosphericsUniforms2;
	atmosphericsUniforms2.invViewProj=ivp;
	atmosphericsUniforms2.invViewProj.transpose();
	atmosphericsUniforms2.tanHalfFov=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	atmosphericsUniforms2.nearZ=frustum.nearZ*0.001f/fade_distance_km;
	atmosphericsUniforms2.farZ=frustum.farZ*0.001f/fade_distance_km;

	UPDATE_CONSTANT_BUFFER(m_pImmediateContext,atmosphericsUniforms2ConstantsBuffer,AtmosphericsUniforms2,atmosphericsUniforms2)
	ID3DX11EffectConstantBuffer* cbAtmosphericsUniforms2=effect->GetConstantBufferByName("AtmosphericsUniforms2");
	if(cbAtmosphericsUniforms2)
		cbAtmosphericsUniforms2->SetConstantBuffer(atmosphericsUniforms2ConstantsBuffer);
	
	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();

	{
		AtmosphericsUniforms atmosphericsUniforms;
		float alt_km			=cam_pos.z/1000.f;
		atmosphericsUniforms.lightDir			=(const float*)skyInterface->GetDirectionToLight(alt_km);
		atmosphericsUniforms.mieRayleighRatio	=(const float*)skyInterface->GetMieRayleighRatio();
		atmosphericsUniforms.texelOffsets		=D3DXVECTOR2(0,0);
		atmosphericsUniforms.hazeEccentricity	=skyInterface->GetMieEccentricity();
		UPDATE_CONSTANT_BUFFER(m_pImmediateContext,constantBuffer,AtmosphericsUniforms,atmosphericsUniforms)
		ID3DX11EffectConstantBuffer* cbAtmosphericsUniforms=effect->GetConstantBufferByName("AtmosphericsUniforms");
		if(cbAtmosphericsUniforms)
			cbAtmosphericsUniforms->SetConstantBuffer(constantBuffer);
	}
	
	ApplyPass(m_pImmediateContext,technique->GetPassByIndex(0));
	framebuffer->Render(context,false);
	//UtilityRenderer::DrawQuad(m_pImmediateContext,-1.f,-1.f,2.f,2.f,technique);
	imageTexture->SetResource(NULL);
	lossTexture->SetResource(NULL);
	inscatterTexture->SetResource(NULL);
	skylightTexture->SetResource(NULL);
	PIXEndNamedEvent();
}