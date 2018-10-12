#include "Framebuffer.h"
#include <iostream>
#include <string>
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"

#ifdef _MSC_VER
    #include <windows.h>
#else
    #define BREAK_IF_DEBUGGING 
#endif

using namespace simul;
using namespace vulkan;

Framebuffer::Framebuffer(const char* name):
    BaseFramebuffer(name)
{
    this->name = name;
}

Framebuffer::~Framebuffer()
{
    InvalidateDeviceObjects();
}

void Framebuffer::SetWidthAndHeight(int w, int h, int m)
{
    if (Width != w || Height != h || mips != m)
    {
        InvalidateDeviceObjects();
        Width   = w;
        Height  = h;
        mips    = m;
    }
}

void Framebuffer::SetAsCubemap(int w, int num_mips, crossplatform::PixelFormat f)
{
    SetWidthAndHeight(w, w, num_mips);
    SetFormat(f);
    is_cubemap = true;
}

void Framebuffer::SetFormat(crossplatform::PixelFormat f)
{
    if (target_format == f)
    {
        return;
    }
    target_format = f;
    InvalidateDeviceObjects();
}

void Framebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
    if ((int)depth_format == f)
    {
        return;
    }
    depth_format = f;
    InvalidateDeviceObjects();
}

bool Framebuffer::IsValid() const
{
    return ((buffer_texture != nullptr && buffer_texture->IsValid()) || (buffer_depth_texture != nullptr && buffer_depth_texture->IsValid()));
}

void Framebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
    BaseFramebuffer::RestoreDeviceObjects(r);
}

void Framebuffer::ActivateDepth(crossplatform::DeviceContext &deviceContext)
{
}

void Framebuffer::SetAntialiasing(int s)
{
    numAntialiasingSamples = s;
}

bool Framebuffer::CreateBuffers()
{
    if (!Width || !Height)
    {
        return false;
    }
    if (!renderPlatform)
    {
        SIMUL_BREAK("renderPlatform should not be NULL here");
        return false;
    }
    if ((buffer_texture && buffer_texture->IsValid()))
    {
        return true;
    }
    if (buffer_depth_texture && buffer_depth_texture->IsValid())
    {
        return true;
    }
    if (buffer_texture)
    {
        buffer_texture->InvalidateDeviceObjects();
    }
    if (buffer_depth_texture)
    {
        buffer_depth_texture->InvalidateDeviceObjects();
    }
    if (!buffer_texture)
    {
        std::string sn = "BaseFramebuffer_" + name;
        buffer_texture = renderPlatform->CreateTexture(sn.c_str());
    }
    if (!buffer_depth_texture)
    {
        std::string sn = "BaseFramebufferDepth_" + name;
        buffer_depth_texture = renderPlatform->CreateTexture(sn.c_str());
    }
    static int quality = 0;
    if (!external_texture && target_format != crossplatform::UNKNOWN)
    {
        if (!is_cubemap)
        {
            buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height, target_format, false, true, false, numAntialiasingSamples, quality);
        }
        else
        {
            buffer_texture->ensureTextureArraySizeAndFormat(renderPlatform, Width, Height, 1, mips, target_format, false, true, true);
        }
    }
    if (!external_depth_texture && depth_format != crossplatform::UNKNOWN)
    {
        buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height, depth_format, false, false, true, numAntialiasingSamples, quality);
    }
    
    return true;
}

void Framebuffer::InvalidateDeviceObjects()
{
	BaseFramebuffer::InvalidateDeviceObjects();
}

void Framebuffer::Activate(crossplatform::DeviceContext& deviceContext)
{
	if((!buffer_texture||!buffer_texture->IsValid())&&(!buffer_depth_texture||!buffer_depth_texture->IsValid()))
		CreateBuffers();
    colour_active   = true;
    vulkan::Texture* glcol      = (vulkan::Texture*)buffer_texture;
    vulkan::Texture* gldepth    = nullptr;
    if (buffer_depth_texture)
    {
        gldepth         = (vulkan::Texture*)buffer_depth_texture;
        // Re-attach depth (we dont really to do this every time, only if we called deactivate depth)
        if (depth_active == false)
        {
        }
        depth_active    = true;
    }
    
    // We need to attach the requested face:
    if (is_cubemap)
    {
    }

    // Construct targets and viewport:
    targetsAndViewport.num              = 1;
    targetsAndViewport.m_rt[0]          = (void*)0;//id;
    targetsAndViewport.m_dt             = 0;
    targetsAndViewport.viewport.x       = 0;
    targetsAndViewport.viewport.y       = 0;
    targetsAndViewport.viewport.w       = Width;
    targetsAndViewport.viewport.h       = Height;

    deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

    // Cache it:
    deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
}

void Framebuffer::SetExternalTextures(crossplatform::Texture* colour, crossplatform::Texture* depth)
{
    BaseFramebuffer::SetExternalTextures(colour, depth);
}

void Framebuffer::Clear(crossplatform::DeviceContext &deviceContext, float r, float g, float b, float a, float d, int mask)
{
    // This call must be made within a Activate - Deactivate block!
    bool changed = false;
    if (!colour_active)
    {
        changed = true;
        Activate(deviceContext);
    }
    if (buffer_depth_texture)
    {
    }

    // Leave it as it was:
    if (changed)
    {
        Deactivate(deviceContext);
    }
}

void Framebuffer::ClearColour(crossplatform::DeviceContext &deviceContext, float r, float g, float b, float a)
{
    SIMUL_BREAK("");
}

void Framebuffer::Deactivate(crossplatform::DeviceContext& deviceContext)
{
    deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
    colour_active   = false;
    depth_active    = false;
}

void Framebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
    // This call must be made inside Activate - Deactivate block!
    if (depth_active)
    {
        depth_active = false;
    }
}
