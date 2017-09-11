// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Framebuffer.cpp A renderer for skies, clouds and weather effects.
#define NOMINMAX
#include "FramebufferDX1x.h"

#include <tchar.h>
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#endif
#include <string>
#include <assert.h>

#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"
#include "Utilities.h"
#include "Simul/Math/Pi.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"
#pragma optimize("",off)
using namespace simul;
using namespace dx11on12;

// First figure out sizes, for placement create
UINT64 iCurrentESRAMOffset = 0; // We allow this to grow beyond ESRAM_SIZE
const UINT64 ESRAM_SIZE = 32 * 1024 * 1024;

Framebuffer::Framebuffer(const char *n) :
	BaseFramebuffer(n)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
	,num_OldViewports(0) //The usual case is for the user to supply depth look-up textures, which is all we need for the majority of cases... So let's avoid needless construction of depth buffers unless otherwise indicated with a SetDepthFormat(...)
	,useESRAM(false)
	,useESRAMforDepth(false)
{
}

Framebuffer::~Framebuffer()
{
	InvalidateDeviceObjects();
}

void Framebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseFramebuffer::RestoreDeviceObjects(r);
}

void Framebuffer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
	BaseFramebuffer::InvalidateDeviceObjects();
}

void Framebuffer::ActivateColour(crossplatform::DeviceContext &deviceContext,const float viewportXYWH[4])
{	
	SIMUL_BREAK_ONCE("Nacho has to check this");
}

void Framebuffer::SaveOldRTs(crossplatform::DeviceContext &deviceContext)
{
	//SIMUL_BREAK_ONCE("Nacho has to check this");
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

void Framebuffer::ActivateViewport(crossplatform::DeviceContext &deviceContext, float viewportX, float viewportY, float viewportW, float viewportH)
{
	// This is a bit usless, Activate does the same?
}

void Framebuffer::Activate(crossplatform::DeviceContext &deviceContext)
{
	auto rPlat = (dx11on12::RenderPlatform*)deviceContext.renderPlatform;

	SaveOldRTs(deviceContext);
	if ((!buffer_texture || !buffer_texture->IsValid()) && (!buffer_depth_texture || !buffer_depth_texture->IsValid()))
		CreateBuffers();
	SIMUL_ASSERT(IsValid());

	// Here we will set both colour and depth
	dx11on12::Texture* col12Texture = (dx11on12::Texture*)buffer_texture;
	dx11on12::Texture* depth12Texture = (dx11on12::Texture*)buffer_depth_texture;
	if (!col12Texture && !depth12Texture)
	{
		SIMUL_BREAK_ONCE("No valid textures in the framebuffer");
	}
	rPlat->AsD3D12CommandList()->OMSetRenderTargets(1,buffer_texture->AsD3D12RenderTargetView(),false,buffer_depth_texture->AsD3D12DepthStencilView());

	// Inform the render platform the current output pixel format 
	// TO-DO: same for depth!
	mLastPixelFormat = rPlat->GetCurrentPixelFormat();
	rPlat->SetCurrentPixelFormat(((dx11on12::Texture*)buffer_texture)->dxgi_format);

	SetViewport(deviceContext,0,0,1.f,1.f,0,1.f);

	// Push current target and viewport
	mTargetAndViewport.num				= 1;
	mTargetAndViewport.m_rt[0]			= buffer_texture->AsD3D12RenderTargetView();
	mTargetAndViewport.m_dt				= buffer_depth_texture->AsD3D12DepthStencilView();
	mTargetAndViewport.viewport.w		= mViewport.Width;
	mTargetAndViewport.viewport.h		= mViewport.Height;
	mTargetAndViewport.viewport.x		= mViewport.TopLeftX;
	mTargetAndViewport.viewport.y		= mViewport.TopLeftY;
	mTargetAndViewport.viewport.znear	= mViewport.MinDepth;
	mTargetAndViewport.viewport.zfar	= mViewport.MaxDepth;
	
	crossplatform::BaseFramebuffer::GetFrameBufferStack().push(&mTargetAndViewport);

	colour_active	= true;
	depth_active	= true;
}

void Framebuffer::SetViewport(crossplatform::DeviceContext &deviceContext,float X,float Y,float W,float H,float Z,float D)
{
	mViewport.Width			= floorf((float)Width*W + 0.5f);
	mViewport.Height		= floorf((float)Height*H + 0.5f);
	mViewport.TopLeftX		= floorf((float)Width*X + 0.5f);
	mViewport.TopLeftY		= floorf((float)Height*Y + 0.5f);
	mViewport.MinDepth		= 0.0f;
	mViewport.MaxDepth		= 1.0f;
	deviceContext.renderPlatform->AsD3D12CommandList()->RSSetViewports(1, &mViewport);
}

void Framebuffer::ActivateDepth(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_BREAK_ONCE("Nacho has to check this");
}

void Framebuffer::ActivateColour(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_BREAK_ONCE("Nacho has to check this");
}

void Framebuffer::Deactivate(crossplatform::DeviceContext &deviceContext)
{
	auto rPlat = (dx11on12::RenderPlatform*)deviceContext.renderPlatform;
	
	// We should leave the state as it was when we started rendering
	if (crossplatform::BaseFramebuffer::GetFrameBufferStack().size() <= 1)
	{
		// Set the default targets
		crossplatform::BaseFramebuffer::GetFrameBufferStack().pop();
		rPlat->AsD3D12CommandList()->OMSetRenderTargets
		(
			1,
			(CD3DX12_CPU_DESCRIPTOR_HANDLE*)BaseFramebuffer::defaultTargetsAndViewport.m_rt[0],
			false,
			(CD3DX12_CPU_DESCRIPTOR_HANDLE*)BaseFramebuffer::defaultTargetsAndViewport.m_dt
		);
	}
	else
	{
		// There are other plugin Framebuffers
		crossplatform::BaseFramebuffer::GetFrameBufferStack().pop();
		auto curTargets = crossplatform::BaseFramebuffer::GetFrameBufferStack().top();
		SIMUL_ASSERT(curTargets->num == 1);
		rPlat->AsD3D12CommandList()->OMSetRenderTargets
		(
			1,
			(CD3DX12_CPU_DESCRIPTOR_HANDLE*)curTargets->m_rt[0],
			false,
			(CD3DX12_CPU_DESCRIPTOR_HANDLE*)curTargets->m_dt
		);
	}

	// Set back the last pixel format
	rPlat->SetCurrentPixelFormat(mLastPixelFormat);

	colour_active	= false;
	depth_active	= false;
}

void Framebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	deviceContext.renderPlatform->AsD3D12CommandList()->OMSetRenderTargets
	(
		1,
		(CD3DX12_CPU_DESCRIPTOR_HANDLE*)buffer_texture->AsD3D12RenderTargetView(),
		false,
		nullptr
	);
	depth_active=false;
}

void Framebuffer::Clear(crossplatform::DeviceContext &deviceContext,float r,float g,float b,float a,float depth,int mask)
{
	if((!buffer_texture||!buffer_texture->IsValid())&&(!buffer_depth_texture||!buffer_depth_texture->IsValid()))
		CreateBuffers();
	
	// Make sure that the Framebuffer is activated
	bool changed = false;
	if (!depth_active || !colour_active)
	{
		Activate(deviceContext);
		changed = true;
	}
	
	ClearColour(deviceContext, r, g, b, a);
	ClearDepth(deviceContext, depth);

	// Leave it in the same state
	// TO-DO: Actual "same" state check if depth or colour
	if (changed)
	{
		Deactivate(deviceContext);
	}
}

void Framebuffer::ClearDepth(crossplatform::DeviceContext &context,float depth)
{
	context.renderPlatform->AsD3D12CommandList()->ClearDepthStencilView(*buffer_depth_texture->AsD3D12DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void Framebuffer::ClearColour(crossplatform::DeviceContext &deviceContext,float r,float g,float b,float a)
{
	float clearColor[4]={r,g,b,a};
	if (is_cubemap)
	{
		SIMUL_BREAK("Not implemented");
	}
	else
	{
		deviceContext.renderPlatform->AsD3D12CommandList()->ClearRenderTargetView(*buffer_texture->AsD3D12RenderTargetView(), clearColor, 0, nullptr);
	}
}
