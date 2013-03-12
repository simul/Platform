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
	m_pImmediateContext(NULL),
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
	InvalidateDeviceObjects();
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
	V_CHECK(CreateEffect(m_pd3dDevice,&effect,L"atmospherics.fx"));
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
	
	// Setup the description of the dynamic matrix constant buffer that is in the vertex shader.
	/*D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage				= D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth			= PAD16(sizeof(AtmosphericsUniforms));
	bufferDesc.BindFlags			= D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags		= D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags			= 0;
	bufferDesc.StructureByteStride	= 0;

	// Create the constant buffer pointer so we can access the shader constant buffer from within this class.
	SAFE_RELEASE(constantBuffer);
	m_pd3dDevice->CreateBuffer(&bufferDesc, NULL, &constantBuffer);*/
	MAKE_CONSTANT_BUFFER(constantBuffer,AtmosphericsUniforms);
}

HRESULT SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(ID3D1xDevice* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=dev;
#ifdef DX10
	m_pImmediateContext=dev;
#else
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	RecompileShaders();
	if(framebuffer)
		framebuffer->RestoreDeviceObjects(dev);
/*	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  8, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(vertexDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&vertexDecl);*/
	return hr;
}

HRESULT SimulAtmosphericsRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(framebuffer)
		framebuffer->InvalidateDeviceObjects();
	SAFE_RELEASE(m_pImmediateContext);
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

void SimulAtmosphericsRendererDX1x::StartRender()
{
	if(!framebuffer)
		return;
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::StartRender");
	framebuffer->Activate();
	// Clear the screen to black, with alpha=1, representing far depth
	framebuffer->Clear(0.0,1.0,1.0,1.0);

	PIXEndNamedEvent();
}

void SimulAtmosphericsRendererDX1x::FinishRender()
{
	if(!framebuffer)
		return;
	framebuffer->Deactivate();
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::FinishRender");
	HRESULT hr=S_OK;
	hr=imageTexture->SetResource(framebuffer->buffer_texture_SRV);
	lossTexture1->SetResource(skyLossTexture_SRV);
	//skylightTexture_SRV=(ID3D1xTexture2D*)s;
	inscatterTexture1->SetResource(skyInscatterTexture_SRV);
skylightTexture->SetResource(skylightTexture_SRV);
	simul::math::Matrix4x4 vpt;
	simul::math::Matrix4x4 viewproj;
	simul::math::Vector3 cam_pos=simul::dx11::GetCameraPosVector(view,false);
	view(3,0)=view(3,1)=view(3,2)=0;
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
	m_pImmediateContext->VSSetConstantBuffers(0,1,&constantBuffer);
	m_pImmediateContext->PSSetConstantBuffers(0,1,&constantBuffer);
	ApplyPass(technique->GetPassByIndex(0));
	framebuffer->Render(false);
	imageTexture->SetResource(NULL);
	lossTexture1->SetResource(NULL);
	inscatterTexture1->SetResource(NULL);
	skylightTexture->SetResource(NULL);
	PIXEndNamedEvent();
}
