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

#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
#include "MacrosDX1x.h"
#include "Utilities.h"
#include "Simul/Math/Pi.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"

#include <tchar.h>
#include <string>
#include <assert.h>

#pragma optimize("",off)

using namespace simul;
using namespace dx12;


Framebuffer::Framebuffer(const char *n) :
	BaseFramebuffer(n)
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
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;

	SaveOldRTs(deviceContext);
	if ((!buffer_texture || !buffer_texture->IsValid()) && (!buffer_depth_texture || !buffer_depth_texture->IsValid()))
		CreateBuffers();
	SIMUL_ASSERT(IsValid());

	// Here we will set both colour and depth
	dx12::Texture* col12Texture = (dx12::Texture*)buffer_texture;
	dx12::Texture* depth12Texture = (dx12::Texture*)buffer_depth_texture;
	if (!col12Texture && !depth12Texture)
	{
		SIMUL_BREAK_ONCE("No valid textures in the framebuffer");
	}
	if(buffer_depth_texture->IsValid())
		rPlat->AsD3D12CommandList()->OMSetRenderTargets(1,buffer_texture->AsD3D12RenderTargetView(),false,buffer_depth_texture->AsD3D12DepthStencilView());
	else
		rPlat->AsD3D12CommandList()->OMSetRenderTargets(1,buffer_texture->AsD3D12RenderTargetView(),false,nullptr);

	// Inform the render platform the current output pixel format 
	// TO-DO: same for depth!
	mLastPixelFormat = rPlat->GetCurrentPixelFormat();
	rPlat->SetCurrentPixelFormat(((dx12::Texture*)buffer_texture)->dxgi_format);

	SetViewport(deviceContext,0,0,1.f,1.f,0,1.f);

	// Push current target and viewport
	mTargetAndViewport.num				= 1;
	mTargetAndViewport.m_rt[0]			= buffer_texture->AsD3D12RenderTargetView();
	mTargetAndViewport.m_dt				= buffer_depth_texture->AsD3D12DepthStencilView();
	mTargetAndViewport.viewport.w		= (int)mViewport.Width;
	mTargetAndViewport.viewport.h		= (int)mViewport.Height;
	mTargetAndViewport.viewport.x		= (int)mViewport.TopLeftX;
	mTargetAndViewport.viewport.y		= (int)mViewport.TopLeftY;
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
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	
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

	// TO-DO: check this NACHOOOOO! :)
	if (crossplatform::BaseFramebuffer::GetFrameBufferStack().size())
	{
		auto curTop = crossplatform::BaseFramebuffer::GetFrameBufferStack().top();
		curTop->m_dt = nullptr;
	}
	// And this :]
	else
	{
		crossplatform::BaseFramebuffer::defaultTargetsAndViewport.m_dt = nullptr;
	}
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
	if (buffer_depth_texture->AsD3D12DepthStencilView())
	{
		context.renderPlatform->AsD3D12CommandList()->ClearDepthStencilView(*buffer_depth_texture->AsD3D12DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
	}
}

void Framebuffer::ClearColour(crossplatform::DeviceContext &deviceContext,float r,float g,float b,float a)
{
	float clearColor[4] = { r,g,b,a };
	if (is_cubemap)
	{
		auto tex = (dx12::Texture*)buffer_texture;
		for (int i = 0; i < 6; i++)
		{
			deviceContext.renderPlatform->AsD3D12CommandList()->ClearRenderTargetView(*tex->AsD3D12RenderTargetView(i), clearColor, 0, nullptr);
		}
	}
	else
	{
		deviceContext.renderPlatform->AsD3D12CommandList()->ClearRenderTargetView(*buffer_texture->AsD3D12RenderTargetView(), clearColor, 0, nullptr);
	}
}
