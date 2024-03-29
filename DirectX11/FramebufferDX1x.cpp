// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Framebuffer.cpp A renderer for skies, clouds and weather effects.

#include "FramebufferDX1x.h"

#include <tchar.h>
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#endif
#include <string>
#include <assert.h>

#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"
#include "Utilities.h"
#include "Platform/Math/Pi.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/DirectX11/RenderPlatform.h"
#pragma optimize("",off)
using namespace platform;
using namespace dx11;

// First figure out sizes, for placement create
UINT64 iCurrentESRAMOffset = 0; // We allow this to grow beyond ESRAM_SIZE
const UINT64 ESRAM_SIZE = 32 * 1024 * 1024;

Framebuffer::Framebuffer(const char *n) :
	crossplatform::Framebuffer(n)
	,useESRAM(false)
	,useESRAMforDepth(false)
{
}

Framebuffer::~Framebuffer()
{
	InvalidateDeviceObjects();
}

void Framebuffer::MoveToFastRAM()
{
	if(useESRAM&&buffer_texture)
		buffer_texture->MoveToFastRAM();
	if(useESRAMforDepth&&buffer_depth_texture)
		buffer_depth_texture->MoveToFastRAM();
}

void Framebuffer::MoveToSlowRAM()
{
	if(useESRAM)
		buffer_texture->MoveToSlowRAM();
	if(useESRAMforDepth)
		buffer_depth_texture->MoveToSlowRAM();
}

void Framebuffer::MoveDepthToSlowRAM()
{
	if(useESRAMforDepth)
		buffer_depth_texture->MoveToSlowRAM();
}

void Framebuffer::Activate(crossplatform::GraphicsDeviceContext &deviceContext)
{
	if((!buffer_texture||!buffer_texture->IsValid())&&(!buffer_depth_texture||!buffer_depth_texture->IsValid()))
		CreateBuffers();
	SIMUL_ASSERT(IsValid());

	crossplatform::TextureView rtv_tv, dsv_tv;
	bool layered = buffer_texture->NumFaces() > 1;
	bool ms = buffer_texture->GetSampleCount() > 1;
	rtv_tv.elements.type = buffer_texture->GetShaderResourceTypeForRTVAndDSV();
	rtv_tv.elements.subresourceRange = {crossplatform::TextureAspectFlags::COLOUR, 0, uint8_t(1), uint8_t(is_cubemap ? current_face : 0), 1};
	layered = buffer_depth_texture ? buffer_depth_texture->NumFaces() > 1: false;
	ms = buffer_depth_texture ? buffer_depth_texture->GetSampleCount() > 1 : false;
	dsv_tv.elements.type = buffer_depth_texture ? buffer_depth_texture->GetShaderResourceTypeForRTVAndDSV() : crossplatform::ShaderResourceType::UNKNOWN;
	dsv_tv.elements.subresourceRange = {crossplatform::TextureAspectFlags::DEPTH, 0, 1, 0, 1};

	ID3D11RenderTargetView* renderTargetView = buffer_texture->AsD3D11RenderTargetView(rtv_tv);
	ID3D11DepthStencilView* dt = buffer_depth_texture ? buffer_depth_texture->AsD3D11DepthStencilView(dsv_tv) : NULL;

	if(renderTargetView)
	{
		colour_active=true;
		depth_active=buffer_depth_texture&&(dt!=NULL);
		deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1,&renderTargetView,dt);
	}
	else if(buffer_depth_texture&&buffer_depth_texture->IsValid())
	{
		depth_active=buffer_depth_texture&&(dt!=NULL);
		deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(0,nullptr,dt);
	}
	else
	{
	// This is ok, as long as we don't render to an invalid texture. e.g. a minimized window.
	}
	SetViewport(deviceContext,0,0,1.f,1.f,0,1.f);
	targetsAndViewport.m_rt[0]=renderTargetView;
	targetsAndViewport.textureTargets[0].texture=buffer_texture;
	targetsAndViewport.textureTargets[0].subresource={};
	targetsAndViewport.depthTarget.texture=buffer_depth_texture;
	targetsAndViewport.depthTarget.subresource={};
	targetsAndViewport.m_dt=dt;
	targetsAndViewport.viewport.x=targetsAndViewport.viewport.y=0;
	targetsAndViewport.viewport.w=Width;
	targetsAndViewport.viewport.h=Height;
	deviceContext.contextState.scissor.x=deviceContext.contextState.scissor.y=0;
	deviceContext.contextState.scissor.z=Width;
	deviceContext.contextState.scissor.w=Height;
	targetsAndViewport.num=1;
	
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
}

void Framebuffer::SetViewport(crossplatform::GraphicsDeviceContext &deviceContext,float X,float Y,float W,float H,float ,float )
{
	D3D11_VIEWPORT viewport;
	viewport.Width = floorf((float)Width*W + 0.5f);
	viewport.Height = floorf((float)Height*H + 0.5f);
	viewport.TopLeftX = floorf((float)Width*X + 0.5f);
	viewport.TopLeftY = floorf((float)Height*Y+ 0.5f);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	deviceContext.asD3D11DeviceContext()->RSSetViewports(1, &viewport);
}

void Framebuffer::ActivateDepth(crossplatform::GraphicsDeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	if((!buffer_texture||!buffer_texture->AsD3D11Texture2D())&&(!buffer_depth_texture||!buffer_depth_texture->AsD3D11Texture2D()))
		CreateBuffers();
	HRESULT hr=S_OK;
	
	crossplatform::TextureView rtv_tv, dsv_tv;
	rtv_tv.elements.type = buffer_texture->GetShaderResourceTypeForRTVAndDSV();
	rtv_tv.elements.subresourceRange = {crossplatform::TextureAspectFlags::COLOUR, 0, 1, 0, 1};
	dsv_tv.elements.type = buffer_depth_texture ? buffer_depth_texture->GetShaderResourceTypeForRTVAndDSV() : crossplatform::ShaderResourceType::UNKNOWN;
	dsv_tv.elements.subresourceRange = {crossplatform::TextureAspectFlags::DEPTH, 0, 1, 0, 1};

	ID3D11RenderTargetView* renderTargetView = buffer_texture->AsD3D11RenderTargetView(rtv_tv);
	ID3D11DepthStencilView* depthStencilView = buffer_depth_texture ? buffer_depth_texture->AsD3D11DepthStencilView(dsv_tv) : NULL;
	pContext->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
	depth_active = buffer_depth_texture && (depthStencilView);
	D3D11_VIEWPORT viewport;
	// Setup the viewport for rendering.
	viewport.Width = (float)Width;
	viewport.Height = (float)Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.f;
	viewport.TopLeftY = 0.f;

	// Create the viewport.
	pContext->RSSetViewports(1, &viewport);
}

void Framebuffer::Deactivate(crossplatform::GraphicsDeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	renderPlatform->PopRenderTargets(deviceContext);
	if(GenerateMips)
	{
		crossplatform::TextureView tv;
		tv.elements.type = buffer_texture->GetShaderResourceTypeForRTVAndDSV();
		tv.elements.subresourceRange = {crossplatform::TextureAspectFlags::COLOUR, 0, uint8_t(-1), 0, uint8_t(-1)};
		pContext->GenerateMips(buffer_texture->AsD3D11ShaderResourceView(tv));
	}
	colour_active=false;
	depth_active=false;
}

void Framebuffer::DeactivateDepth(crossplatform::GraphicsDeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	crossplatform::TextureView rtv_tv;
	rtv_tv.elements.type = buffer_texture->GetShaderResourceTypeForRTVAndDSV();
	rtv_tv.elements.subresourceRange = {crossplatform::TextureAspectFlags::COLOUR, 0, 1, 0, 1};

	//Check for an RTV.
	ID3D11RenderTargetView *renderTargetView = buffer_texture->AsD3D11RenderTargetView(rtv_tv);
	if (!renderTargetView)
	{
		Deactivate(deviceContext);
		return;
	}
	
	pContext->OMSetRenderTargets(1,&renderTargetView,NULL); //Set only the RTV, no DSV.
	depth_active=false;

	auto &fb=deviceContext.GetFrameBufferStack().top();
	fb->m_dt=nullptr;
}