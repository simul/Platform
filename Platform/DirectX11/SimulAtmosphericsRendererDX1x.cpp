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

/*
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)			{ hr = x; if( FAILED(hr) ) { return hr; } }
#endif
#ifndef SAFE_DELETE
#define SAFE_DELETE(p)		{ if(p) { delete (p);     (p)=NULL; } }
#endif    */
#define BLUR_SIZE 9
#define MONTE_CARLO_BLUR

#ifdef  MONTE_CARLO_BLUR
#include "Simul/Math/Pi.h"
#endif

SimulAtmosphericsRendererDX1x::SimulAtmosphericsRendererDX1x() :
	m_pd3dDevice(NULL),
	vertexDecl(NULL),
	framebuffer(NULL),
	effect(NULL),
	lightDir(NULL),
	constantBuffer(NULL),
	MieRayleighRatio(NULL),
	HazeEccentricity(NULL),
	fadeInterp(NULL),
	imageTexture(NULL),
	depthTexture(NULL),
	lossTexture1(NULL),
	inscatterTexture1(NULL),
	skylightTexture(NULL),
	skyLossTexture_SRV(NULL),
	skyInscatterTexture_SRV(NULL),
	skylightTexture_SRV(NULL),
	clouds_texture(NULL),
	fade_interp(0.f)
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

void SimulAtmosphericsRendererDX1x::SetDistanceTexture(void* t)
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
	lossTexture1		=effect->GetVariableByName("lossTexture1")->AsShaderResource();
	inscatterTexture1	=effect->GetVariableByName("inscatterTexture1")->AsShaderResource();
	skylightTexture		=effect->GetVariableByName("skylightTexture")->AsShaderResource();
	
	MAKE_CONSTANT_BUFFER(constantBuffer,AtmosphericsUniforms);
}

HRESULT SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(ID3D1xDevice* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=dev;
	RecompileShaders();
	if(framebuffer)
		framebuffer->RestoreDeviceObjects(dev);
	return hr;
}

HRESULT SimulAtmosphericsRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(framebuffer)
		framebuffer->InvalidateDeviceObjects();
	SAFE_RELEASE(vertexDecl);
	SAFE_RELEASE(effect);
	SAFE_RELEASE(constantBuffer);
	return hr;
}

HRESULT SimulAtmosphericsRendererDX1x::Destroy()
{
	return InvalidateDeviceObjects();
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
	framebuffer->Clear(context,0.f,1.f,1.f,1.f,ReverseDepth?0.f:1.f);

	PIXEndNamedEvent();
}

void SimulAtmosphericsRendererDX1x::FinishRender(void *context)
{
	if(!framebuffer)
		return;
	ID3D11DeviceContext* m_pImmediateContext=(ID3D11DeviceContext*)context;
	framebuffer->Deactivate(m_pImmediateContext);
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::FinishRender");
	HRESULT hr=S_OK;
	hr=imageTexture->SetResource(framebuffer->buffer_texture_SRV);
	lossTexture1->SetResource(skyLossTexture_SRV);
	inscatterTexture1->SetResource(skyInscatterTexture_SRV);
	skylightTexture->SetResource(skylightTexture_SRV);
	simul::math::Matrix4x4 vpt;
	simul::math::Matrix4x4 viewproj;
	simul::math::Vector3 cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;
	if(ReverseDepth)
	{
		// Convert the proj matrix into a normal non-reversed matrix.
		simul::dx11::ConvertReversedToRegularProjectionMatrix(proj);
	}
	simul::math::Matrix4x4 v((const float *)view),p((const float*)proj);
	simul::math::Multiply4x4(viewproj,v,p);
	viewproj.Transpose(vpt);
	simul::math::Matrix4x4 ivp;
	vpt.Inverse(ivp);
	simul::dx11::setMatrix(effect,"invViewProj",ivp.RowPointer(0));
	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();
	simul::dx11::setParameter(effect,"mieRayleighRatio",ratio);
	simul::dx11::setParameter(effect,"lightDir",light_dir);
	simul::dx11::setParameter(effect,"hazeEccentricity",skyInterface->GetMieEccentricity());

	{
		// Lock the constant buffer so it can be written to.
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		m_pImmediateContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		// Get a pointer to the data in the constant buffer.
		AtmosphericsUniforms *constb=(AtmosphericsUniforms*)mappedResource.pData;
		float alt_km			=cam_pos.z/1000.f;
		constb->lightDir		=(const float*)skyInterface->GetDirectionToLight(alt_km);
		constb->mieRayleighRatio=(const float*)skyInterface->GetMieRayleighRatio();
		constb->texelOffsets	=D3DXVECTOR2(0,0);
		constb->hazeEccentricity=skyInterface->GetMieEccentricity();
		// Unlock the constant buffer.
		m_pImmediateContext->Unmap(constantBuffer, 0);
	}
	
	m_pImmediateContext->VSSetConstantBuffers(9,1,&constantBuffer);
	m_pImmediateContext->PSSetConstantBuffers(9,1,&constantBuffer);
	
	ApplyPass(m_pImmediateContext,technique->GetPassByIndex(0));
	framebuffer->Render(context,false);
	imageTexture->SetResource(NULL);
	lossTexture1->SetResource(NULL);
	inscatterTexture1->SetResource(NULL);
	skylightTexture->SetResource(NULL);
	PIXEndNamedEvent();
}
