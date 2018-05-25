#define NOMINMAX
#include "FramebufferDX1x.h"

#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
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
    targetsAndViewport = {};
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
    if ((!buffer_texture || !buffer_texture->IsValid()) && (!buffer_depth_texture || !buffer_depth_texture->IsValid()))
    {
		CreateBuffers();
    }
	SIMUL_ASSERT(IsValid());

	// Get the views that we want to activate:
	dx12::Texture* col12Texture		    = (dx12::Texture*)buffer_texture;
	dx12::Texture* depth12Texture	    = (dx12::Texture*)buffer_depth_texture;
	if (!col12Texture && !depth12Texture)
	{
		SIMUL_BREAK_ONCE("No valid textures in the framebuffer");
        return;
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

	// Push current target and viewport
    targetsAndViewport.num				= 1;
    targetsAndViewport.m_rt[0]			= rtView;
    targetsAndViewport.rtFormats[0]     = col12Texture->pixelFormat;
    targetsAndViewport.m_dt				= dsView;
    if (buffer_depth_texture->IsValid())
    {
        targetsAndViewport.depthFormat = depth12Texture->pixelFormat;
    }
	targetsAndViewport.viewport.w		= Width;
	targetsAndViewport.viewport.h		= Height;
	targetsAndViewport.viewport.x		= 0;
	targetsAndViewport.viewport.y		= 0;
	targetsAndViewport.viewport.znear	= 0.0f;
	targetsAndViewport.viewport.zfar	= 1.0f;

    deviceContext.renderPlatform->ActivateRenderTargets(deviceContext, &targetsAndViewport);

    // Cache current state
	colour_active	                    = true;
	depth_active	                    = dsView != nullptr;
}

void Framebuffer::ActivateDepth(crossplatform::DeviceContext& deviceContext)
{
	SIMUL_BREAK_ONCE("Nacho has to check this");
}

void Framebuffer::Deactivate(crossplatform::DeviceContext &deviceContext)
{
    if (!colour_active && !depth_active)
    {
        SIMUL_CERR << "Deactivate was called on an already deactivated FBO \n";
        return;
    }
    deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
	colour_active	= false;
	depth_active	= false;
}

void Framebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	if (!depth_active)
	{
        SIMUL_CERR << "Depth is not active for this FBO \b";
		return;
	}
    deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
    targetsAndViewport.depthFormat  = crossplatform::UNKNOWN;
    targetsAndViewport.m_dt         = nullptr;
    deviceContext.renderPlatform->ActivateRenderTargets(deviceContext, &targetsAndViewport);
    depth_active                    = false;
}

void Framebuffer::Clear(crossplatform::DeviceContext &deviceContext,float r,float g,float b,float a,float depth,int mask)
{
    if ((!buffer_texture || !buffer_texture->IsValid()) && (!buffer_depth_texture || !buffer_depth_texture->IsValid()))
    {
		CreateBuffers();
    }
	
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
