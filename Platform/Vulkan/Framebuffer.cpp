#include "Framebuffer.h"
#include <iostream>
#include <string>
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"
#include "Simul/Platform/Vulkan/Effect.h"

#ifdef _MSC_VER
    #include <windows.h>
#endif

using namespace simul;
using namespace vulkan;

Framebuffer::Framebuffer(const char* name):
    BaseFramebuffer(name)
{
	if(name)
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
        SAFE_DELETE(buffer_texture);
		SAFE_DELETE(buffer_depth_texture);
        Width   = w;
        Height  = h;
        mips    = m;
		InvalidateFramebuffers();
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
    SAFE_DELETE(buffer_texture);
	SAFE_DELETE(buffer_depth_texture);
	InvalidateFramebuffers();
}

void Framebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
    if ((int)depth_format == f)
    {
        return;
    }
    depth_format = f;
    SAFE_DELETE(buffer_texture);
	SAFE_DELETE(buffer_depth_texture);
	InvalidateFramebuffers();
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
        std::string sn = "BaseFramebufferDepth_" + name;
        buffer_depth_texture = renderPlatform->CreateTexture(sn.c_str());
        buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height, depth_format, false, false, true, numAntialiasingSamples, quality);
    }
    
    return true;
}

void Framebuffer::InvalidateFramebuffers()
{
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	for(int i=0;i<8;i++)
	{
		for(auto f:mFramebuffers[i])
			vulkanDevice->destroyFramebuffer(f);
		vulkanDevice->destroyRenderPass(mDummyRenderPasses[i]);
	}
	initialized=false;
}

void Framebuffer::InvalidateDeviceObjects()
{
	if(renderPlatform)
	{
		InvalidateFramebuffers();
		renderPlatform=nullptr;
	}
	BaseFramebuffer::InvalidateDeviceObjects();
}

void Framebuffer::Activate(crossplatform::DeviceContext& deviceContext)
{
	if((!buffer_texture||!buffer_texture->IsValid())&&(!buffer_depth_texture||!buffer_depth_texture->IsValid()))
		CreateBuffers();
    colour_active   = true;
    vulkan::Texture* col      = (vulkan::Texture*)buffer_texture;
    vulkan::Texture* depth    = nullptr;
    if (buffer_depth_texture)
    {
        depth         = (vulkan::Texture*)buffer_depth_texture;
        // Re-attach depth (we dont really to do this every time, only if we called deactivate depth)
        if (depth_active == false)
        {
        }
        depth_active    = true;
    }
    
    // We need to attach the requested face: 
	// For cubemap faces, we also include the native Vulkan framebuffer pointer in m_rt[1], which the renderplatform will handle.
    if (is_cubemap)
    {
		targetsAndViewport.m_rt[1] = (void*)GetVulkanFramebuffer(deviceContext, current_face);
    }

    // Construct targets and viewport:
    targetsAndViewport.num							= 1;
	targetsAndViewport.textureTargets[0].texture	= buffer_texture;
	targetsAndViewport.textureTargets[0].layer		= is_cubemap ? current_face : 0;
	targetsAndViewport.textureTargets[0].mip		= 0;
	targetsAndViewport.depthTarget.texture			= buffer_depth_texture;
	targetsAndViewport.depthTarget.layer			= 0;
	targetsAndViewport.depthTarget.mip				= 0;
	// note the different interpretation of m_rt in the case that it's a Simul framebuffer not native:
    targetsAndViewport.m_rt[0]          = (void*)this;
	targetsAndViewport.m_dt             = 0;
    targetsAndViewport.viewport.x       = 0;
    targetsAndViewport.viewport.y       = 0;
    targetsAndViewport.viewport.w       = Width;
    targetsAndViewport.viewport.h       = Height;


    deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

    // Cache it:
    deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
}
#include "Simul/Base/StringFunctions.h"
void Framebuffer::InitVulkanFramebuffer(crossplatform::DeviceContext &deviceContext)
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	if(buffer_texture)
	{
		vulkanRenderPlatform->CreateVulkanRenderpass(mDummyRenderPasses[RPType::COLOUR|RPType::DEPTH],1,target_format,depth_format,false,numAntialiasingSamples);
		vulkanRenderPlatform->CreateVulkanRenderpass(mDummyRenderPasses[RPType::COLOUR|RPType::DEPTH|RPType::CLEAR],1,target_format,depth_format,true,numAntialiasingSamples);
		vulkanRenderPlatform->CreateVulkanRenderpass(mDummyRenderPasses[RPType::COLOUR],1,target_format,crossplatform::PixelFormat::UNKNOWN,false,numAntialiasingSamples);
		vulkanRenderPlatform->CreateVulkanRenderpass(mDummyRenderPasses[RPType::COLOUR|RPType::CLEAR],1,target_format,crossplatform::PixelFormat::UNKNOWN,true,numAntialiasingSamples);
	}
	if(buffer_depth_texture)
		vulkanRenderPlatform->CreateVulkanRenderpass(mDummyRenderPasses[RPType::DEPTH],0,crossplatform::PixelFormat::UNKNOWN,depth_format,numAntialiasingSamples);

	vk::FramebufferCreateInfo framebufferCreateInfo = vk::FramebufferCreateInfo();
	framebufferCreateInfo.width = Width;
	framebufferCreateInfo.height = Height;
	framebufferCreateInfo.layers = 1;
	vk::ImageView attachments[2]={nullptr,nullptr};
	framebufferCreateInfo.pAttachments = attachments;
	if(buffer_depth_texture)
		attachments[1]=*(buffer_depth_texture->AsVulkanImageView(crossplatform::ShaderResourceType::TEXTURE_2D,0,0));
	int totalNum	= is_cubemap ? 6  : 1;
	for(int i=1;i<8;i++)
	{
		if(i==4||(i&RPType::COLOUR)==0)
			continue;
		mFramebuffers[i].resize(totalNum);
		for(int j=0;j<totalNum;j++)
		{
			framebufferCreateInfo.attachmentCount=1+((buffer_depth_texture!=nullptr&&(i&RPType::DEPTH)!=0&&(i&RPType::COLOUR)!=0)?1:0);
			framebufferCreateInfo.renderPass = mDummyRenderPasses[i];
			attachments[0]=*(buffer_texture->AsVulkanImageView(crossplatform::ShaderResourceType::TEXTURE_2D,j,0));
			SIMUL_ASSERT(vulkanDevice->createFramebuffer(&framebufferCreateInfo, nullptr, &mFramebuffers[i][j])==vk::Result::eSuccess);
			SetVulkanName(renderPlatform,(uint64_t*)&mFramebuffers[i][j],base::QuickFormat(+"%s mFramebuffers %d %d",name.c_str(),i,j));
		}
	}
	initialized=true;
}

vk::Framebuffer *Framebuffer::GetVulkanFramebuffer(crossplatform::DeviceContext &deviceContext,int cube_face)
{
	if(!initialized)
		InitVulkanFramebuffer(deviceContext);
	if(cube_face<0)
		cube_face=0;
	if(buffer_texture&&colour_active)
		((vulkan::Texture*)buffer_texture)->SplitLayouts();//vk::ImageLayout::ePresentSrcKHR);
	if(buffer_depth_texture&&depth_active)
		((vulkan::Texture*)buffer_depth_texture)->SplitLayouts();//vk::ImageLayout::eDepthStencilReadOnlyOptimal);
	if(depth_active)
	{
		if(colour_active)
			return &(mFramebuffers[RPType::COLOUR|RPType::DEPTH][cube_face]);
		else
			return &(mFramebuffers[RPType::DEPTH][cube_face]);
	}
	else 
		return &(mFramebuffers[RPType::COLOUR][cube_face]);
}

vk::RenderPass *Framebuffer::GetVulkanRenderPass(crossplatform::DeviceContext &deviceContext)
{
	if(!initialized)
		InitVulkanFramebuffer(deviceContext);
	if(depth_active)
		return &mDummyRenderPasses[RPType::COLOUR|RPType::DEPTH];
	if(colour_active)
		return &mDummyRenderPasses[RPType::COLOUR];
	return nullptr;
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
    clear_next=true;
	auto &cb=renderPlatform->GetDebugConstantBuffer();
	cb.debugColour=vec4(r,g,b,a);
	cb.debugDepth=d;
	renderPlatform->GetDebugEffect()->SetConstantBuffer(deviceContext,&cb);
	renderPlatform->GetDebugEffect()->Apply(deviceContext,"clear_both",0);
	renderPlatform->DrawQuad(deviceContext);
	renderPlatform->GetDebugEffect()->Unapply(deviceContext);

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
