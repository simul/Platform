#include "Framebuffer.h"
#include <iostream>
#include <string>
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/Effect.h"
#include "Platform/Core/StringFunctions.h"

#ifdef _MSC_VER
	#include <windows.h>
#endif

using namespace platform;
using namespace vulkan;

Framebuffer::Framebuffer(const char* name):
	crossplatform::Framebuffer(name)
{
	if(name)
		this->name = name;
}

Framebuffer::~Framebuffer()
{
	InvalidateDeviceObjects();
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

void Framebuffer::ActivateDepth(crossplatform::GraphicsDeviceContext &deviceContext)
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
		std::string sn = "Framebuffer_Colour_" + name;
		buffer_texture = renderPlatform->CreateTexture(sn.c_str());
	}
	static int quality = 0;
	if (!external_texture && target_format != crossplatform::UNKNOWN)
	{
		if (!is_cubemap)
		{
			buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height,1, target_format, false, true, false, numAntialiasingSamples, quality);
		}
		else
		{
			buffer_texture->ensureTextureArraySizeAndFormat(renderPlatform, Width, Height, 1, mips, target_format, nullptr, false, true, false, true);
		}
	}
	if (!external_depth_texture && depth_format != crossplatform::UNKNOWN)
	{
		std::string sn = "Framebuffer_Depth_" + name;
		buffer_depth_texture = renderPlatform->CreateTexture(sn.c_str());
		buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height,1, depth_format, false, false, true, numAntialiasingSamples, quality);
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
			vulkanDevice->destroyFramebuffer(f,nullptr);
		mFramebuffers[i].clear();
		if(mDummyRenderPasses[i])
			vulkanDevice->destroyRenderPass(mDummyRenderPasses[i],nullptr);
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
	crossplatform::Framebuffer::InvalidateDeviceObjects();
}

void Framebuffer::Activate(crossplatform::GraphicsDeviceContext& deviceContext)
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
	 
		
		
		depth_active    = true;
	}
	
	// We need to attach the requested face: 
	// For cubemap faces, we also include the native Vulkan framebuffer pointer in m_rt[1], which the renderplatform will handle.
	// UPDATE: I don't think we are using this.
	if (is_cubemap)
	{
		targetsAndViewport.m_rt[1] = (void*)nullptr;//GetVulkanFramebuffer(deviceContext, current_face);
	}

	// Construct targets and viewport:
	targetsAndViewport.num												= 1;
	targetsAndViewport.textureTargets[0].texture						= buffer_texture;
	targetsAndViewport.textureTargets[0].subresource.baseArrayLayer		= (is_cubemap && current_face != -1 ? current_face : 0);
	targetsAndViewport.textureTargets[0].subresource.arrayLayerCount	= 1;
	targetsAndViewport.textureTargets[0].subresource.mipLevel			= 0;
	targetsAndViewport.depthTarget.texture						= buffer_depth_texture;
	targetsAndViewport.depthTarget.subresource.baseArrayLayer	= 0;
	targetsAndViewport.depthTarget.subresource.arrayLayerCount	= 1;
	targetsAndViewport.depthTarget.subresource.mipLevel			= 0;
	// note the different interpretation of m_rt in the case that it's a Simul framebuffer not native:
	targetsAndViewport.m_rt[0]          = (void*)0;
	targetsAndViewport.m_dt             = 0;
	targetsAndViewport.viewport.x       = 0;
	targetsAndViewport.viewport.y       = 0;
	targetsAndViewport.viewport.w       = Width;
	targetsAndViewport.viewport.h       = Height;


	deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

	// Cache it:
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
}

void Framebuffer::InitVulkanFramebuffer(crossplatform::GraphicsDeviceContext &deviceContext)
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	if(buffer_texture)
	{
		vulkanRenderPlatform->CreateVulkanRenderpass(deviceContext,mDummyRenderPasses[RPType::COLOUR|RPType::DEPTH],1,&target_format,depth_format,true,true,false,numAntialiasingSamples);
		vulkanRenderPlatform->CreateVulkanRenderpass(deviceContext,mDummyRenderPasses[RPType::COLOUR|RPType::DEPTH|RPType::CLEAR],1,&target_format,depth_format,true,true,true,numAntialiasingSamples);
		vulkanRenderPlatform->CreateVulkanRenderpass(deviceContext,mDummyRenderPasses[RPType::COLOUR],1,&target_format,crossplatform::PixelFormat::UNKNOWN,false,false,false,numAntialiasingSamples);
		vulkanRenderPlatform->CreateVulkanRenderpass(deviceContext,mDummyRenderPasses[RPType::COLOUR|RPType::CLEAR],1,&target_format,crossplatform::PixelFormat::UNKNOWN,false,false,true,numAntialiasingSamples);
	}
	if(buffer_depth_texture)
		vulkanRenderPlatform->CreateVulkanRenderpass(deviceContext,mDummyRenderPasses[RPType::DEPTH],0,nullptr,depth_format,true,true,numAntialiasingSamples);

	vk::FramebufferCreateInfo framebufferCreateInfo = vk::FramebufferCreateInfo();
	framebufferCreateInfo.width = Width;
	framebufferCreateInfo.height = Height;
	framebufferCreateInfo.layers = 1;
	vk::ImageView attachments[2]={nullptr,nullptr};
	framebufferCreateInfo.pAttachments = attachments;
	if(buffer_depth_texture)
		attachments[1] = *(buffer_depth_texture->AsVulkanImageView({ crossplatform::ShaderResourceType::TEXTURE_2D, { crossplatform::TextureAspectFlags::DEPTH, 0, 1, 0, 1 } }));
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
			attachments[0]=*(buffer_texture->AsVulkanImageView({ crossplatform::ShaderResourceType::TEXTURE_2D, { crossplatform::TextureAspectFlags::COLOUR, 0, 1, j, 1 } }));
			SIMUL_ASSERT(vulkanDevice->createFramebuffer(&framebufferCreateInfo, nullptr, &mFramebuffers[i][j])==vk::Result::eSuccess);
			SetVulkanName(renderPlatform,mFramebuffers[i][j],platform::core::QuickFormat(+"%s mFramebuffers %d %d",name.c_str(),i,j));
		}
	}
	initialized=true;
}

vk::Framebuffer *Framebuffer::GetVulkanFramebuffer(crossplatform::GraphicsDeviceContext &deviceContext,int cube_face)
{
	if(!initialized)
		InitVulkanFramebuffer(deviceContext);
	if(cube_face<0)
		cube_face=0;
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

vk::RenderPass *Framebuffer::GetVulkanRenderPass(crossplatform::GraphicsDeviceContext &deviceContext)
{
	if(!initialized)
		InitVulkanFramebuffer(deviceContext);
	if(depth_active)
		return &mDummyRenderPasses[RPType::COLOUR|RPType::DEPTH];
	if(colour_active)
		return &mDummyRenderPasses[RPType::COLOUR];
	return nullptr;
}

void Framebuffer::Clear(crossplatform::GraphicsDeviceContext &deviceContext, float r, float g, float b, float a, float d, int mask)
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
	auto *effect=renderPlatform->GetDebugEffect();
	effect->SetConstantBuffer(deviceContext,&cb);
	effect->Apply(deviceContext,"clear_colour",0);
	renderPlatform->RestoreColourTextureState(deviceContext, buffer_texture);
	renderPlatform->RestoreDepthTextureState(deviceContext, buffer_depth_texture);
	renderPlatform->DrawQuad(deviceContext);
	effect->Unapply(deviceContext);
	// Leave it as it was:
	if (changed)
	{
		Deactivate(deviceContext);
	}
	if(buffer_depth_texture)
		buffer_depth_texture->ClearDepthStencil(deviceContext,d);
}

void Framebuffer::ClearColour(crossplatform::GraphicsDeviceContext &deviceContext, float r, float g, float b, float a)
{
	SIMUL_BREAK("");
}

void Framebuffer::Deactivate(crossplatform::GraphicsDeviceContext& deviceContext)
{
	deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
	colour_active   = false;
	depth_active    = false;
}

void Framebuffer::DeactivateDepth(crossplatform::GraphicsDeviceContext &deviceContext)
{
	// This call must be made inside Activate - Deactivate block!
	if (depth_active)
	{
		depth_active = false;
		auto &tv=deviceContext.GetFrameBufferStack().top();
		SIMUL_ASSERT(tv->textureTargets[0].texture==this->buffer_texture);
		tv->depthTarget.texture=nullptr;
	}
}
