// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#define NOMINMAX
#include "FramebufferDX1x.h"

#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
#include "MacrosDX1x.h"
#include "Simul/Math/Pi.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"

#include <tchar.h>
#include <string>
#include <assert.h>

using namespace simul;
using namespace dx12;

Framebuffer::Framebuffer(const char *n) :
	BaseFramebuffer(n)
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

void Framebuffer::SetAntialiasing(int a)
{
    if (numAntialiasingSamples != a)
    {
        numAntialiasingSamples = a;
        InvalidateDeviceObjects();
    }
}

void Framebuffer::Activate(crossplatform::DeviceContext &deviceContext)
{
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;

	if ((!buffer_texture || !buffer_texture->IsValid()) && (!buffer_depth_texture || !buffer_depth_texture->IsValid()))
		CreateBuffers();
	SIMUL_ASSERT(IsValid());

	// Here we will set both colour and depth
	dx12::Texture* col12Texture		= (dx12::Texture*)buffer_texture;
	dx12::Texture* depth12Texture	= (dx12::Texture*)buffer_depth_texture;
	if (!col12Texture && !depth12Texture)
	{
		SIMUL_BREAK_ONCE("No valid textures in the framebuffer");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* rtView = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE* dsView = nullptr;
	if (is_cubemap)
	{
		rtView = col12Texture->AsD3D12RenderTargetView(current_face, 0);
	}
	else
	{
		rtView = col12Texture->AsD3D12RenderTargetView();
	}
	if (buffer_depth_texture->IsValid())
	{
		dsView = depth12Texture->AsD3D12DepthStencilView();
	}
	deviceContext.asD3D12Context()->OMSetRenderTargets(1,rtView,false,dsView);

	// Inform the render platform the current output pixel format 
	// TO-DO: same for depth!
	mLastPixelFormat = rPlat->GetCurrentPixelFormat();
	rPlat->SetCurrentPixelFormat(((dx12::Texture*)buffer_texture)->dxgi_format);

	SetViewport
	(
		deviceContext,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 1.0f
	);

	// Push current target and viewport
	mTargetAndViewport.num				= 1;
	mTargetAndViewport.m_rt[0]			= rtView;
	mTargetAndViewport.m_dt				= dsView;
	mTargetAndViewport.viewport.w		= (int)mViewport.Width;
	mTargetAndViewport.viewport.h		= (int)mViewport.Height;
	mTargetAndViewport.viewport.x		= (int)mViewport.TopLeftX;
	mTargetAndViewport.viewport.y		= (int)mViewport.TopLeftY;
	mTargetAndViewport.viewport.znear	= mViewport.MinDepth;
	mTargetAndViewport.viewport.zfar	= mViewport.MaxDepth;
	
	deviceContext.GetFrameBufferStack().push(&mTargetAndViewport);

	colour_active	= true;
	depth_active	= dsView != nullptr;
}

void Framebuffer::SetViewport(crossplatform::DeviceContext &deviceContext,float X,float Y,float W,float H,float Z,float D)
{
	mViewport.Width			= floorf((float)Width*W + 0.5f);
	mViewport.Height		= floorf((float)Height*H + 0.5f);
	mViewport.TopLeftX		= floorf((float)Width*X + 0.5f);
	mViewport.TopLeftY		= floorf((float)Height*Y + 0.5f);
	mViewport.MinDepth		= Z;
	mViewport.MaxDepth		= D;

	CD3DX12_RECT scissor(0, 0, mViewport.Width, mViewport.Height);

	deviceContext.asD3D12Context()->RSSetScissorRects(1, &scissor);
	deviceContext.asD3D12Context()->RSSetViewports(1, &mViewport);
}

void Framebuffer::ActivateDepth(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_BREAK_ONCE("Nacho has to check this");
}

void Framebuffer::Deactivate(crossplatform::DeviceContext &deviceContext)
{
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
    rPlat->DeactivateRenderTargets(deviceContext);
	rPlat->SetCurrentPixelFormat(mLastPixelFormat);

	colour_active	= false;
	depth_active	= false;
}

void Framebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	if (!depth_active)
	{
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* rtView = nullptr;
	if (is_cubemap)
	{
		rtView = buffer_texture->AsD3D12RenderTargetView(current_face, 0);
	}
	else
	{
		rtView = buffer_texture->AsD3D12RenderTargetView();
	}
	deviceContext.asD3D12Context()->OMSetRenderTargets(1,rtView,false,nullptr);
    depth_active = false;

	// Inform that depth is not active:
	if (deviceContext.GetFrameBufferStack().size())
	{
		auto curTop = deviceContext.GetFrameBufferStack().top();
		curTop->m_dt = nullptr;
	}
	else
	{
		deviceContext.defaultTargetsAndViewport.m_dt = nullptr;
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
	if (changed)
	{
		Deactivate(deviceContext);
	}
}

void Framebuffer::ClearDepth(crossplatform::DeviceContext &context,float depth)
{
	if (buffer_depth_texture->AsD3D12DepthStencilView())
	{
		context.asD3D12Context()->ClearDepthStencilView(*buffer_depth_texture->AsD3D12DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
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
			deviceContext.asD3D12Context()->ClearRenderTargetView(*tex->AsD3D12RenderTargetView(i), clearColor, 0, nullptr);
		}
	}
	else
	{
		deviceContext.asD3D12Context()->ClearRenderTargetView(*buffer_texture->AsD3D12RenderTargetView(), clearColor, 0, nullptr);
	}
}
