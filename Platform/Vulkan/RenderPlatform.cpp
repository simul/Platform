
#include "Simul/Platform/Vulkan/RenderPlatform.h"
#include "Simul/Platform/Vulkan/Texture.h"
#include "Simul/Platform/Vulkan/Effect.h"
#include "Simul/Platform/Vulkan/Buffer.h"
#include "Simul/Platform/Vulkan/Framebuffer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/Vulkan/Texture.h"
#include "Simul/Platform/Vulkan/DisplaySurface.h"
#include <vulkan/vulkan.hpp>

using namespace simul;
using namespace vulkan;

void simul::vulkan::SetVulkanName(crossplatform::RenderPlatform *renderPlatform,void *ds,const char *name)
{
	//vk::Instance *instance=((vulkan::RenderPlatform*)renderPlatform)->AsVulkanInstance();
	//* It would be nice if this worked:
	/*PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT	=reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>     (instance->getProcAddr("vkDebugMarkerSetObjectNameEXT"));
	if(vkDebugMarkerSetObjectNameEXT)
	{
		if(ds!=0)
		{
			vk::DebugMarkerObjectNameInfoEXT nameInfo=vk::DebugMarkerObjectNameInfoEXT()
				.setObjectType(vk::DebugReportObjectTypeEXT::eImage)
				.setObject((uint64_t)ds)
				.setPObjectName(name);
			vk::Device *device=renderPlatform->AsVulkanDevice();
			auto inf=nameInfo.operator const VkDebugMarkerObjectNameInfoEXT &();
			vkDebugMarkerSetObjectNameEXT(device->operator VkDevice(),&inf);
		}
	}*/

	// But it doesn't. So instead we just list the objects and names.
#ifdef _DEBUG
	uint64_t *u=(uint64_t*)ds;
	std::cout<<"0x"<<std::hex<<*u<<"\t"<<name<<"\n";
#endif
}
void simul::vulkan::SetVulkanName(crossplatform::RenderPlatform *renderPlatform,void *ds,const std::string &name)
{
	SetVulkanName(renderPlatform,ds,name.c_str());
}

RenderPlatform::RenderPlatform():
	mDummy2D(nullptr)
    ,mDummy3D(nullptr)
{
    mirrorY     = false;
    mirrorY2    = false;
    mirrorYText = false;

}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

const char* RenderPlatform::GetName()const
{
    return "Vulkan";
}

void RenderPlatform::RestoreDeviceObjects(void* vkDevice_vkInstance_gpu)
{
	void **ptr=(void**)vkDevice_vkInstance_gpu;
	vulkanDevice=(vk::Device*)ptr[0];
	vulkanInstance=(vk::Instance*)ptr[1];
	vulkanGpu=(vk::PhysicalDevice*)ptr[2];
	immediateContext.platform_context=nullptr;
    crossplatform::RenderPlatform::RestoreDeviceObjects(nullptr);
    RecompileShaders();

	int swapchainImageCount=SIMUL_VULKAN_FRAME_LAG+1;
	vk::DescriptorPoolSize const poolSizes[] = {
		vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(swapchainImageCount)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eSampledImage).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eSampler).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageBufferDynamic).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageImage).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(swapchainImageCount * 32)};

	auto const descriptorPoolCreateInfo =
		vk::DescriptorPoolCreateInfo().setMaxSets(swapchainImageCount).setPoolSizeCount(_countof(poolSizes)).setPPoolSizes(poolSizes);

	auto result = vulkanDevice->createDescriptorPool(&descriptorPoolCreateInfo, nullptr, &mDescriptorPool);
}

vk::Device *RenderPlatform::AsVulkanDevice()
{
	return vulkanDevice;
}

vk::Instance *RenderPlatform::AsVulkanInstance()
{
	return vulkanInstance;
}

vk::PhysicalDevice *RenderPlatform::GetVulkanGPU()
{
	return vulkanGpu;
}

void RenderPlatform::InvalidateDeviceObjects()
{
	if(!vulkanDevice)
		return;
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
	SAFE_DELETE(mDummy3D);
	SAFE_DELETE(mDummy2D);
	vulkanDevice->destroyDescriptorPool(mDescriptorPool, nullptr);
	vulkanDevice=nullptr;
}

void RenderPlatform::BeginFrame()
{
	auto *vulkanDevice=AsVulkanDevice();
	//vulkanDevice->waitForFences(1, &deviceManagerInternal->fences[frame_index], VK_TRUE, UINT64_MAX);
	//vulkanDevice->resetFences(1, &deviceManagerInternal->fences[frame_index]);
}

void RenderPlatform::EndFrame()
{
}

float RenderPlatform::GetDefaultOutputGamma() const 
{
	static float g=1.0f;
	return g;
}

void RenderPlatform::BeginEvent(crossplatform::DeviceContext& deviceContext, const char* name)
{
}

void RenderPlatform::EndEvent(crossplatform::DeviceContext& deviceContext)
{
}


void RenderPlatform::DispatchCompute(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	auto *vkEffectPass=((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass);
    BeginEvent(deviceContext, vkEffectPass->name.c_str());
    ApplyContextState(deviceContext);
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;
	if(commandBuffer)
	{
		commandBuffer->dispatch(w, l, d);
	}
    InsertFences(deviceContext);
    EndEvent(deviceContext);
}

void RenderPlatform::DrawLine(crossplatform::DeviceContext &,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)
{
}

void RenderPlatform::DrawLineLoop(crossplatform::DeviceContext &,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, vec4 mult, bool blend, float gamma, bool debug)
{
    crossplatform::RenderPlatform::DrawTexture(deviceContext, x1, y1, dx, dy, tex, mult, blend,gamma);
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext& deviceContext)   
{
    BeginEvent(deviceContext, ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
    ApplyContextState(deviceContext);
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;

	if(commandBuffer)
	{
		commandBuffer->draw(4,1,0,0);
		commandBuffer->endRenderPass();
	}
//    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EndEvent(deviceContext);
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext &deviceContext,bool error_checking)
{
    crossplatform::ContextState* cs = &deviceContext.contextState;
    vulkan::EffectPass* pass    = (vulkan::EffectPass*)cs->currentEffectPass;

	if(!cs||!cs->currentEffectPass)
	{
		SIMUL_BREAK("No valid shader pass in ApplyContextState");
		return false;
	}
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;
	if(!commandBuffer)
		return false;

	if(!cs->effectPassValid)
	{
		if(cs->last_action_was_compute&&pass->shaders[crossplatform::SHADERTYPE_VERTEX]!=nullptr)
			cs->last_action_was_compute=false;
		else if((!cs->last_action_was_compute)&&pass->shaders[crossplatform::SHADERTYPE_COMPUTE]!=nullptr)
			cs->last_action_was_compute=true;
		cs->effectPassValid=true;
	}
	
	// We will only set the tables once per frame
	if (mLastFrame != deviceContext.frame_number)
	{
		// Call start render at least once per frame to make sure the bins 
		// release objects!
		BeginFrame();

		mLastFrame = deviceContext.frame_number;
		mCurIdx++;
		mCurIdx = mCurIdx % kNumIdx;

		// Reset the frame heaps (SRV_CBV_UAV and SAMPLER)
		//mFrameHeap[mCurIdx].Reset();
        // Reset the override samplers heap
        //mFrameOverrideSamplerHeap[mCurIdx].Reset();
	}
	//crossplatform::PixelOutputFormat pfm=GetCurrentPixelOutputFormat(deviceContext);
	
	// If not a compute shader, apply viewports:
	if(commandBuffer!=nullptr&&pass->shaders[crossplatform::SHADERTYPE_COMPUTE]==nullptr)
	{
		vk::DeviceSize offsets[] = { 0 };
		for(auto i:cs->applyVertexBuffers)
		{
			auto vulkanBuffer=(vulkan::Buffer*)i.second;
			vulkanBuffer->FinishLoading(deviceContext);
			vk::Buffer vertexBuffers[] ={vulkanBuffer->asVulkanBuffer()};
			commandBuffer->bindVertexBuffers( i.first, 1, vertexBuffers, offsets);
		}
		if(cs->indexBuffer)
		{
			vk::IndexType indexType=vk::IndexType::eUint32;
			if(cs->indexBuffer->stride==4)
				indexType=::vk::IndexType::eUint16;
			commandBuffer->bindIndexBuffer( ((vulkan::Buffer*)cs->indexBuffer)->asVulkanBuffer(), 0, indexType);
		}
		pass->Apply(deviceContext,false);
		                                                                                                        
		vk::Framebuffer *framebuffer=GetCurrentVulkanFramebuffer(deviceContext);

		vk::ClearValue const clearValues[2] = { vk::ClearColorValue(std::array<float, 4>({{0.0f, 0.0f, 0.0f, 0.0f}})),
										   vk::ClearDepthStencilValue(0.0f, 0u) };
		crossplatform::Viewport vp=GetViewport(deviceContext,0);
		vk::Rect2D renderArea(vk::Offset2D(0, 0), vk::Extent2D((uint32_t)vp.w, (uint32_t)vp.h));
		
		vk::RenderPassBeginInfo renderPassBeginInfo=vk::RenderPassBeginInfo().setRenderPass(
													pass->GetVulkanRenderPass(deviceContext,GetActivePixelFormat(deviceContext)))
													.setFramebuffer(*framebuffer)
													.setClearValueCount(2)
													.setPClearValues(clearValues)
													.setRenderArea(renderArea);
		commandBuffer->beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);

		vk::Viewport vkViewports[1];
		vk::Rect2D vkScissors[1];
		for(int i=0;i<1;i++)
		{
			crossplatform::Viewport &vp=cs->viewports[i];
			vkViewports[0].setHeight(vp.h).setWidth(vp.w).setX(vp.x).setY(vp.y)
				.setMaxDepth(1.0f).setMinDepth(0.0f);
			vkScissors[0].setExtent(vk::Extent2D(vp.w,vp.h)).setOffset(vk::Offset2D(vp.x,vp.y));
		}
		commandBuffer->setViewport(0,1,vkViewports);
		commandBuffer->setScissor(0,1,vkScissors);
	}
	else
	{
		pass->Apply(deviceContext,true);
	}
	return true;
}

crossplatform::PixelFormat RenderPlatform::GetActivePixelFormat(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::PixelFormat pixelFormat=crossplatform::PixelFormat::UNKNOWN;
	{
		pixelFormat=crossplatform::PixelFormat::BGRA_8_UNORM;
		crossplatform::TargetsAndViewport *tv=&deviceContext.defaultTargetsAndViewport;
		if(deviceContext.targetStack.size())
		{
			tv=deviceContext.targetStack.top();
			if(tv->textureTargets[0].texture)
				pixelFormat=tv->textureTargets[0].texture->pixelFormat;
		}
	}
	return pixelFormat;
}

uint32_t RenderPlatform::FindMemoryType(uint32_t typeFilter,vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties;
	vk::PhysicalDevice *gpu=GetVulkanGPU();
	gpu->getMemoryProperties(&memProperties);

     for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	 {
         if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		 {
             return i;
         }
     }

     SIMUL_BREAK("failed to find suitable memory type!");
	 return 0;
 }
#include "Simul/Base/StringFunctions.h"
void RenderPlatform::CreatVulkanBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory,const char *name)
{
    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    SIMUL_VK_CHECK (vulkanDevice->createBuffer(&bufferInfo, nullptr, &buffer));

    vk::MemoryRequirements memRequirements;
    vulkanDevice->getBufferMemoryRequirements( buffer, &memRequirements);

    vk::MemoryAllocateInfo allocInfo = {};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    SIMUL_VK_CHECK (vulkanDevice->allocateMemory(&allocInfo, nullptr, &bufferMemory)); 
    vulkanDevice->bindBufferMemory( buffer, bufferMemory, 0);
#ifdef _DEBUG
	if(name)
	{
		SetVulkanName(this,&(buffer),name);
		SetVulkanName(this,&(bufferMemory),base::QuickFormat("%s memory",name));
	}
#endif
}


void RenderPlatform::InsertFences(crossplatform::DeviceContext& deviceContext)
{
    auto pass = (vulkan::EffectPass*)deviceContext.contextState.currentEffectPass;

}

crossplatform::Texture* RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	crossplatform::Texture* tex= new vulkan::Texture;
    if (fileNameUtf8 && strlen(fileNameUtf8) > 0 && strcmp(fileNameUtf8, "ESRAM") != 0)
	{
		std::string str(fileNameUtf8);
        if (str.find(".") < str.length())
        {
            tex->LoadFromFile(this, fileNameUtf8);
        }
	}
    tex->SetName(fileNameUtf8);
	return tex;
}

crossplatform::BaseFramebuffer* RenderPlatform::CreateFramebuffer(const char *n)
{
	vulkan::Framebuffer* b=new vulkan::Framebuffer(n);
	return b;
}

crossplatform::SamplerState* RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc* desc)
{
	vulkan::SamplerState* s = new vulkan::SamplerState();
    s->Init(this,desc);
	return s;
}

crossplatform::Effect* RenderPlatform::CreateEffect()
{
	vulkan::Effect* e=new vulkan::Effect();
	return e;
}

crossplatform::Effect* RenderPlatform::CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	crossplatform::Effect* e=crossplatform::RenderPlatform::CreateEffect(filename_utf8,defines);
	return e;
}

crossplatform::PlatformConstantBuffer* RenderPlatform::CreatePlatformConstantBuffer()
{
	return new vulkan::PlatformConstantBuffer();
}

crossplatform::PlatformStructuredBuffer* RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer* b=new vulkan::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer* RenderPlatform::CreateBuffer()
{
	return new vulkan::Buffer();
}


const float whiteTexel[4] = { 1.0f,1.0f,1.0f,1.0f};
vulkan::Texture *RenderPlatform::GetDummyTexture(crossplatform::ShaderResourceType t)
{
	if((t&crossplatform::ShaderResourceType::TEXTURE_3D)==crossplatform::ShaderResourceType::TEXTURE_3D)
		return GetDummy3D();
	if((t&crossplatform::ShaderResourceType::TEXTURE_2D)==crossplatform::ShaderResourceType::TEXTURE_2D)
		return GetDummy2D();
	if((t&crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY)==crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY)
	{
		if (!mDummyTextureCubeArray)
		{
			mDummyTextureCubeArray = (vulkan::Texture*)CreateTexture("mDummyTextureCubeArray");
			mDummyTextureCubeArray->ensureTextureArraySizeAndFormat(this, 1, 1, 2, 1, crossplatform::PixelFormat::RGBA_8_UNORM,false,false,true);
		}
		return mDummyTextureCubeArray;
	}
	if((t&crossplatform::ShaderResourceType::TEXTURE_CUBE)==crossplatform::ShaderResourceType::TEXTURE_CUBE)
		return GetDummyTextureCube();
	return GetDummy2D();
}

vulkan::Texture* RenderPlatform::GetDummyTextureCube()
{
    if (!mDummyTextureCube)
    {
        mDummyTextureCube = (vulkan::Texture*)CreateTexture("mDummyTextureCube");
        mDummyTextureCube->ensureTextureArraySizeAndFormat(this, 1, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM,false,false,true);
		
		const float whiteTexels[24] = { 1.0f,1.0f,1.0f,1.0f
										,1.0f,1.0f,1.0f,1.0f
										,1.0f,1.0f,1.0f,1.0f
										,1.0f,1.0f,1.0f,1.0f
										,1.0f,1.0f,1.0f,1.0f
										,1.0f,1.0f,1.0f,1.0f};
        mDummyTextureCube->setTexels(immediateContext, &whiteTexels[0], 0, 6);
    }
    return mDummyTextureCube;
}

vulkan::Texture* RenderPlatform::GetDummy2D()
{
    if (!mDummy2D)
    {
        mDummy2D = (vulkan::Texture*)CreateTexture("dummy2d");
        mDummy2D->ensureTexture2DSizeAndFormat(this, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM);
        mDummy2D->setTexels(immediateContext, &whiteTexel[0], 0, 1);
    }
    return mDummy2D;
}

vulkan::Texture* RenderPlatform::GetDummy3D()
{
    if (!mDummy3D)
    {
        mDummy3D = (vulkan::Texture*)CreateTexture("dummy3d");
        mDummy3D->ensureTexture3DSizeAndFormat(this, 1, 1,1, crossplatform::PixelFormat::RGBA_8_UNORM);
        mDummy3D->setTexels(immediateContext, &whiteTexel[0], 0, 1);
    }
    return mDummy3D;
}
crossplatform::RenderState* RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	vulkan::RenderState* s  = new vulkan::RenderState;
	s->desc                 = desc;
	s->type                 = desc.type;
	return s;
}

crossplatform::Query* RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	vulkan::Query* q=new vulkan::Query(type);
	return q;
}

vk::Filter simul::vulkan::RenderPlatform::toVulkanMinFiltering(crossplatform::SamplerStateDesc::Filtering f)
{
    if (f == simul::crossplatform::SamplerStateDesc::LINEAR)
    {
        return vk::Filter::eLinear;
    }
    return vk::Filter::eNearest;
}

vk::Filter simul::vulkan::RenderPlatform::toVulkanMaxFiltering(crossplatform::SamplerStateDesc::Filtering f)
{
    if (f == simul::crossplatform::SamplerStateDesc::LINEAR)
    {
        return vk::Filter::eLinear;
    }
    return vk::Filter::eNearest;
}

vk::SamplerMipmapMode simul::vulkan::RenderPlatform::toVulkanMipmapMode(crossplatform::SamplerStateDesc::Filtering f)
{
    if (f == simul::crossplatform::SamplerStateDesc::LINEAR)
    {
        return vk::SamplerMipmapMode::eLinear;
    }
    return vk::SamplerMipmapMode::eNearest;
}

vk::SamplerAddressMode RenderPlatform::toVulkanWrapping(crossplatform::SamplerStateDesc::Wrapping w)
{
	switch(w)
	{
	case crossplatform::SamplerStateDesc::WRAP:
		return vk::SamplerAddressMode::eRepeat;
		break;
	case crossplatform::SamplerStateDesc::CLAMP:
		return vk::SamplerAddressMode::eClampToEdge;
		break;
	case crossplatform::SamplerStateDesc::MIRROR:
		return vk::SamplerAddressMode::eMirroredRepeat;
		break;
	}
	return vk::SamplerAddressMode::eRepeat;
}

vk::PrimitiveTopology RenderPlatform::toVulkanTopology(crossplatform::Topology t)
{
    switch (t)
    {
    case crossplatform::POINTLIST:
        return vk::PrimitiveTopology::ePointList;
    case crossplatform::LINELIST:
        return vk::PrimitiveTopology::eLineList;
    case crossplatform::LINESTRIP:
        return vk::PrimitiveTopology::eLineStrip;
    case crossplatform::TRIANGLELIST:
        return vk::PrimitiveTopology::eTriangleList;
    case crossplatform::TRIANGLESTRIP:
        return vk::PrimitiveTopology::eTriangleStrip;
    case crossplatform::LINELIST_ADJ:
        return vk::PrimitiveTopology::eLineListWithAdjacency;
    case crossplatform::LINESTRIP_ADJ:
        return vk::PrimitiveTopology::eLineStripWithAdjacency;
    case crossplatform::TRIANGLELIST_ADJ:
        return vk::PrimitiveTopology::eTriangleListWithAdjacency;
    case crossplatform::TRIANGLESTRIP_ADJ:
        return vk::PrimitiveTopology::eTriangleStripWithAdjacency;
    default:
        break;
    };
    return vk::PrimitiveTopology::eLineStrip;
}
/*
GLenum RenderPlatform::DataType(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return GL_FLOAT;
	case RGBA_16_FLOAT:
		return GL_FLOAT;
	case RGBA_32_FLOAT:
		return GL_FLOAT;
	case RGB_32_FLOAT:
		return GL_FLOAT;
	case RG_32_FLOAT:
		return GL_FLOAT;
	case R_32_FLOAT:
		return GL_FLOAT;
	case R_16_FLOAT:
		return GL_FLOAT;
	case LUM_32_FLOAT:
		return GL_FLOAT;
	case INT_32_FLOAT:
		return GL_FLOAT;
	case RGBA_8_UNORM_SRGB:
	case RGBA_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGBA_8_SNORM:
		return GL_UNSIGNED_INT;
	case RGB_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGB_8_SNORM:
		return GL_UNSIGNED_INT;
	case R_8_UNORM:
		return GL_UNSIGNED_BYTE;
	case R_8_SNORM:
		return GL_UNSIGNED_BYTE;
	case R_32_UINT:
		return GL_UNSIGNED_INT;
	case RG_32_UINT:
		return GL_UNSIGNED_INT;
	case RGB_32_UINT:
		return GL_UNSIGNED_INT;
	case RGBA_32_UINT:
		return GL_UNSIGNED_INT;
	case D_32_FLOAT:
		return GL_FLOAT;
	case D_16_UNORM:
		return GL_UNSIGNED_SHORT;
	case D_24_UNORM_S_8_UINT:
		return GL_UNSIGNED_INT_24_8;
	default:
		return 0;
	};
}*/

vk::CullModeFlags RenderPlatform::toVulkanCullFace(crossplatform::CullFaceMode c)
{
    switch (c)
    {
    case simul::crossplatform::CULL_FACE_FRONT:
        return vk::CullModeFlagBits::eFront;
    case simul::crossplatform::CULL_FACE_BACK:
        return vk::CullModeFlagBits::eBack;
    case simul::crossplatform::CULL_FACE_FRONTANDBACK:
        return vk::CullModeFlagBits::eFrontAndBack;
    default:
        break;
    }
    return vk::CullModeFlagBits::eFront;
}

vk::BlendOp RenderPlatform::toVulkanBlendOperation(crossplatform::BlendOperation o)
{
    switch (o)
    {
    case simul::crossplatform::BLEND_OP_ADD:
        return vk::BlendOp::eAdd;
    case simul::crossplatform::BLEND_OP_SUBTRACT:
        return vk::BlendOp::eSubtract;
    case simul::crossplatform::BLEND_OP_MAX:
        return vk::BlendOp::eMax;
    case simul::crossplatform::BLEND_OP_MIN:
        return vk::BlendOp::eMin;
    default:
        break;
    }
    return vk::BlendOp::eAdd;
}

vk::CompareOp RenderPlatform::toVulkanComparison(crossplatform::DepthComparison d)
{
	switch(d)
	{
	case crossplatform::DEPTH_ALWAYS:
		return vk::CompareOp::eAlways;
	case crossplatform::DEPTH_LESS:
		return vk::CompareOp::eLess;
	case crossplatform::DEPTH_EQUAL:
		return vk::CompareOp::eEqual;
	case crossplatform::DEPTH_LESS_EQUAL:
		return vk::CompareOp::eLessOrEqual;
	case crossplatform::DEPTH_GREATER:
		return vk::CompareOp::eGreater;
	case crossplatform::DEPTH_NOT_EQUAL:
		return vk::CompareOp::eNotEqual;
	case crossplatform::DEPTH_GREATER_EQUAL:
		return vk::CompareOp::eGreaterOrEqual;
	default:
		break;
	};
	return vk::CompareOp::eLess;
}

vk::BlendFactor RenderPlatform::toVulkanBlendFactor(crossplatform::BlendOption o)
{
	switch(o)
	{
	case crossplatform::BLEND_ZERO:
		return vk::BlendFactor::eZero;
	case crossplatform::BLEND_ONE:
		return vk::BlendFactor::eOne;
	case crossplatform::BLEND_SRC_COLOR:
		return vk::BlendFactor::eSrcColor;
	case crossplatform::BLEND_INV_SRC_COLOR:
		return vk::BlendFactor::eOneMinusSrcColor;
	case crossplatform::BLEND_SRC_ALPHA:
		return vk::BlendFactor::eSrcAlpha;
	case crossplatform::BLEND_INV_SRC_ALPHA:
		return vk::BlendFactor::eOneMinusSrcAlpha;
	case crossplatform::BLEND_DEST_ALPHA:
		return vk::BlendFactor::eDstAlpha;
	case crossplatform::BLEND_INV_DEST_ALPHA:
		return vk::BlendFactor::eOneMinusDstAlpha;
	case crossplatform::BLEND_DEST_COLOR:
		return vk::BlendFactor::eDstColor;
	case crossplatform::BLEND_INV_DEST_COLOR:
		return vk::BlendFactor::eOneMinusDstColor;
	case crossplatform::BLEND_SRC_ALPHA_SAT:
		return vk::BlendFactor::eSrcAlphaSaturate;
	case crossplatform::BLEND_BLEND_FACTOR:
		return vk::BlendFactor::eConstantColor;
	case crossplatform::BLEND_INV_BLEND_FACTOR:
		return vk::BlendFactor::eOneMinusConstantColor;
	case crossplatform::BLEND_SRC1_COLOR:
		return vk::BlendFactor::eSrc1Color;
	case crossplatform::BLEND_INV_SRC1_COLOR:
		return vk::BlendFactor::eOneMinusSrc1Color;
	case crossplatform::BLEND_SRC1_ALPHA:
		return vk::BlendFactor::eSrc1Alpha;
	case crossplatform::BLEND_INV_SRC1_ALPHA:
		return vk::BlendFactor::eOneMinusSrc1Alpha;
	default:
		break;
	};
	return vk::BlendFactor::eOne;
}

vk::Format RenderPlatform::ToVulkanFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return vk::Format::eB10G11R11UfloatPack32;
	case RGBA_16_FLOAT:
		return vk::Format::eR16G16B16A16Sfloat;
	case RGBA_32_FLOAT:
		return vk::Format::eR32G32B32A32Sfloat;
	case RGBA_32_UINT:
		return vk::Format::eR32G32B32A32Uint;
	case RGB_32_FLOAT:
		return vk::Format::eR32G32B32Sfloat;
	case RG_32_FLOAT:
		return vk::Format::eR32G32Sfloat;
	case R_32_FLOAT:
		return vk::Format::eR32Sfloat;
	case R_16_FLOAT:
		return vk::Format::eR16Sfloat;
	case LUM_32_FLOAT:
		return vk::Format::eR32Sfloat;
	case INT_32_FLOAT:
		return vk::Format::eR32Sfloat;
	case RGBA_8_UNORM:
		return vk::Format::eR8G8B8A8Unorm;
	case BGRA_8_UNORM:
		return vk::Format::eB8G8R8A8Unorm;
	case RGBA_8_UNORM_SRGB:
		return vk::Format::eR8G8B8A8Srgb;
	case RGBA_8_SNORM:
		return vk::Format::eR8G8B8A8Snorm;
	case RGB_8_UNORM:
		return vk::Format::eR8G8B8Unorm;
	case RGB_8_SNORM:
		return vk::Format::eR8G8B8Snorm;
	case R_8_UNORM:
		return vk::Format::eR8Unorm;
	case R_32_UINT:
		return vk::Format::eR32Uint;
	case RG_32_UINT:
		return vk::Format::eR32G32Uint;
	case RGB_32_UINT:
		return vk::Format::eR32G32B32Uint;
	case D_32_FLOAT:
		return vk::Format::eD32Sfloat;
	case D_24_UNORM_S_8_UINT:
		return vk::Format::eD24UnormS8Uint;
	case D_16_UNORM:
		return vk::Format::eD16UnormS8Uint;
	default:
		return vk::Format::eR8G8B8Unorm;
	};
}

crossplatform::PixelFormat RenderPlatform::FromVulkanFormat(vk::Format p)
{
	using namespace crossplatform;
	switch(p)
	{
		case vk::Format::eR16G16B16A16Sfloat:
			return RGBA_16_FLOAT;
		case vk::Format::eR32G32B32A32Sfloat:
			return RGBA_32_FLOAT;
		case vk::Format::eR32G32B32A32Uint:
			return RGBA_32_UINT;
		case vk::Format::eR32G32B32Sfloat:
			return RGB_32_FLOAT;
		case vk::Format::eR32G32Sfloat:
			return RG_32_FLOAT;
		case vk::Format::eR32Sfloat:
			return R_32_FLOAT;
		case vk::Format::eR16Sfloat:
			return R_16_FLOAT;
		case vk::Format::eR8G8B8A8Unorm:
			return RGBA_8_UNORM;
		case vk::Format::eB8G8R8A8Unorm:
			return BGRA_8_UNORM;
		case vk::Format::eR8G8B8A8Srgb:
			return RGBA_8_UNORM_SRGB;
		case vk::Format::eR8G8B8A8Snorm:
			return RGBA_8_SNORM;
		case vk::Format::eR8G8B8Unorm:
			return RGB_8_UNORM;
		case vk::Format::eR8G8B8Snorm:
			return RGB_8_SNORM;
		case vk::Format::eR8Unorm:
			return R_8_UNORM;
		case vk::Format::eR32Uint:
			return R_32_UINT;
		case vk::Format::eR32G32Uint:
			return RG_32_UINT;
		case vk::Format::eR32G32B32Uint:
			return RGB_32_UINT;
		case vk::Format::eD32Sfloat:
			return D_32_FLOAT;
		case vk::Format::eD24UnormS8Uint:
			return D_24_UNORM_S_8_UINT;
		case vk::Format::eD16UnormS8Uint:
			return D_16_UNORM;
	default:
		return UNKNOWN;
	};
}

int RenderPlatform::FormatTexelBytes(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return 4;
	case RGBA_16_FLOAT:
		return 8;
	case RGBA_32_FLOAT:
		return 16;
	case RGB_32_FLOAT:
		return 12;
	case RG_32_FLOAT:
		return 8;
	case R_32_FLOAT:
		return 4;
	case R_16_FLOAT:
		return 2;
	case LUM_32_FLOAT:
		return 4;
	case INT_32_FLOAT:
		return 4;
	case RGBA_8_UNORM:
	case BGRA_8_UNORM:
	case RGBA_8_UNORM_SRGB:
	case RGBA_8_SNORM:
		return 4;
	case RGB_8_UNORM:
	case RGB_8_SNORM:
		return 3;
	case R_8_UNORM:
	case R_8_SNORM:
		return 1;
	case R_32_UINT:
		return 4;
	case RG_32_UINT:
		return 8;
	case RGB_32_UINT:
		return 12;
	case RGBA_32_UINT:
		return 16;
	case D_32_FLOAT:
		return 4;
	case D_16_UNORM:
		return 2;
	case D_24_UNORM_S_8_UINT:
		return 4;
	default:
		return 0;
	};
}

int RenderPlatform::FormatCount(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return 3;
	case RGBA_16_FLOAT:
		return 4;
	case RGBA_32_FLOAT:
		return 4;
	case RGB_32_FLOAT:
		return 3;
	case RG_32_FLOAT:
		return 2;
	case R_32_FLOAT:
		return 1;
	case R_16_FLOAT:
		return 1;
	case LUM_32_FLOAT:
		return 1;
	case INT_32_FLOAT:
		return 1;
	case RGBA_8_UNORM:
	case BGRA_8_UNORM:
	case RGBA_8_UNORM_SRGB:
	case RGBA_8_SNORM:
		return 4;
	case RGB_8_UNORM:
	case RGB_8_SNORM:
		return 3;
	case R_8_UNORM:
	case R_8_SNORM:
	case R_32_UINT:
		return 1;
	case RG_32_UINT:
		return 2;
	case RGB_32_UINT:
		return 3;
	case RGBA_32_UINT:
		return 4;
	case D_32_FLOAT:
		return 1;
	case D_16_UNORM:
		return 1;
	case D_24_UNORM_S_8_UINT:
		return 3;
	default:
		return 0;
	};
}


void RenderPlatform::SetRenderState(crossplatform::DeviceContext& deviceContext,const crossplatform::RenderState* s)
{
    auto state = (vulkan::RenderState*)s;
    if (state->type == crossplatform::RenderStateType::BLEND)
    {
        crossplatform::BlendDesc bdesc = state->desc.blend;
        // We need to iterate over all the rts as we may have some settings
        // from older passes:
        const int kBlendMaxRt = 8;
        for (int i = 0; i < kBlendMaxRt; i++)
        {
            if (i >= bdesc.numRTs || bdesc.RenderTarget[i].blendOperation == crossplatform::BlendOperation::BLEND_OP_NONE)
            {
            }
            else
            {
            }
        }
    }
    else if (state->type == crossplatform::RenderStateType::DEPTH)
    {
        crossplatform::DepthStencilDesc ddesc = state->desc.depth;
        if (ddesc.test)
        {
        }
        else
        {
        }
    }
    else if (state->type == crossplatform::RenderStateType::RASTERIZER)
    {
        crossplatform::RasterizerDesc rdesc = state->desc.rasterizer;
        if (rdesc.cullFaceMode == crossplatform::CullFaceMode::CULL_FACE_NONE)
        {
         //   glDisable(GL_CULL_FACE);
        }
        else
        {
         //   glEnable(GL_CULL_FACE);
         //   glCullFace(toGlCullFace(rdesc.cullFaceMode));
        }
        // Reversed
      //  glFrontFace(rdesc.frontFace == crossplatform::FrontFace::FRONTFACE_CLOCKWISE ? GL_CCW : GL_CW);
    }
    else
    {
        SIMUL_CERR << "Trying to set an invalid render state \n";
    }
}

void RenderPlatform::SetStandardRenderState(crossplatform::DeviceContext& deviceContext, crossplatform::StandardRenderState s)
{
    SetRenderState(deviceContext, standardRenderStates[s]);
}

void RenderPlatform::Resolve(crossplatform::DeviceContext &,crossplatform::Texture *destination,crossplatform::Texture *source)
{
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
}

void* RenderPlatform::GetDevice()
{
	return (void*)vulkanDevice;
}

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext&,crossplatform::Buffer *buffer,int/* start_index*/)
{
}

void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext& deviceContext,int num,crossplatform::Texture** targs,crossplatform::Texture* depth)
{
  //  if (num >= mMaxColorAttatch)
    {
        SIMUL_CERR << "Too many targets \n";
        return;
    }
}
#include <cstdint>
void RenderPlatform::DeactivateRenderTargets(crossplatform::DeviceContext& deviceContext)
{
    deviceContext.GetFrameBufferStack().pop();

    // Default FBO:
    if (deviceContext.GetFrameBufferStack().empty())
    {
        auto defT = deviceContext.defaultTargetsAndViewport;
		const uintptr_t ll= uintptr_t(defT.m_rt[0]);
		GLuint id = GLuint(ll);
      //  glBindFramebuffer(GL_FRAMEBUFFER, id);
        SetViewports(deviceContext, 1, &defT.viewport);
    }
    // Plugin FBO:
    else
    {
        auto topRt = deviceContext.GetFrameBufferStack().top();
		uintptr_t ll=uintptr_t(topRt->m_rt[0]);
		GLuint id = GLuint(ll);
      //  glBindFramebuffer(GL_FRAMEBUFFER, id);
        SetViewports(deviceContext, 1, &topRt->viewport);
    }
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext& deviceContext,int num ,const crossplatform::Viewport* vps)
{
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;
	if(num>0&&vps!=nullptr)
	{
		memcpy(deviceContext.contextState.viewports,vps,num*sizeof(crossplatform::Viewport));
	}
}

void RenderPlatform::SetTopology(crossplatform::DeviceContext &,crossplatform::Topology t)
{
  //  mCurTopology = toGLTopology(t);
}

void RenderPlatform::EnsureEffectIsBuilt(const char *,const std::vector<crossplatform::EffectDefineOptions> &)
{
}


crossplatform::DisplaySurface* RenderPlatform::CreateDisplaySurface()
{
    return new vulkan::DisplaySurface();
}

void RenderPlatform::StoreRenderState(crossplatform::DeviceContext &)
{
}

void RenderPlatform::RestoreRenderState(crossplatform::DeviceContext &)
{
}

void RenderPlatform::PopRenderTargets(crossplatform::DeviceContext &)
{
}

void RenderPlatform::Draw(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
    BeginEvent(deviceContext, ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
    ApplyContextState(deviceContext);
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;

	if(commandBuffer)
	{
		commandBuffer->draw(num_verts,1,start_vert,0);
		commandBuffer->endRenderPass();
	}
    EndEvent(deviceContext);
}

void RenderPlatform::DrawIndexed(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vertex)
{
    BeginEvent(deviceContext, ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
    ApplyContextState(deviceContext);
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;

	if(commandBuffer)
	{
		commandBuffer->drawIndexed(num_indices,1,start_index,base_vertex,0);
		commandBuffer->endRenderPass();
	}
    EndEvent(deviceContext);
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &,crossplatform::PosColourVertex *lines,int vertex_count,bool strip,bool test_depth,bool view_centred)
{
}

void RenderPlatform::Draw2dLines(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int vertex_count,bool strip)
{
}

void RenderPlatform::DrawCircle	(crossplatform::DeviceContext &,const float *,float ,const float *,bool)
{
}

void RenderPlatform::GenerateMips(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* t, bool wrap, int array_idx)
{
    t->GenerateMips(deviceContext);
}

crossplatform::Shader* RenderPlatform::CreateShader()
{
	Shader* S = new Shader();
	return S;
}
#include "DeviceManager.h"

bool RenderPlatform::memory_type_from_properties(uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t *typeIndex)
{
	vk::PhysicalDeviceMemoryProperties memory_properties;
	vk::PhysicalDevice *gpu=GetVulkanGPU();
	gpu->getMemoryProperties(&memory_properties);
// Search memtypes to find first index with those properties
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if ((typeBits & 1) == 1)
		{
// Type is available, does it match user properties?
			if ((memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask)
			{
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}

	// No memory types matched, return failure
	return false;
}

vk::RenderPass *RenderPlatform::GetActiveVulkanRenderPass(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::TargetsAndViewport *tv;
	if(deviceContext.targetStack.size())
		tv=deviceContext.targetStack.top();
	else
		tv=&(deviceContext.defaultTargetsAndViewport);
	if(tv->textureTargets[0].texture!=nullptr)
	{
		if(tv->m_rt[0]!=nullptr)
		{
			vulkan::Framebuffer *fb=(vulkan::Framebuffer *)tv->m_rt[0];
			vk::RenderPass *r=fb->GetVulkanRenderPass(deviceContext);
			return r;
		}
		else
			return nullptr;
	}
	return nullptr;
}

vk::Framebuffer *RenderPlatform::GetCurrentVulkanFramebuffer(crossplatform::DeviceContext& deviceContext)
{
	crossplatform::TargetsAndViewport *tv;
	if(deviceContext.targetStack.size())
		tv=deviceContext.targetStack.top();
	else
		tv=&(deviceContext.defaultTargetsAndViewport);
	if(tv->textureTargets[0].texture!=nullptr)
	{
		if(tv->m_rt[0]!=nullptr)
		{
			vulkan::Framebuffer *fb=(vulkan::Framebuffer *)tv->m_rt[0];
			vk::Framebuffer *vfb=fb->GetVulkanFramebuffer(deviceContext);
			return vfb;
		}
		else
		{
			auto &tt=tv->textureTargets[0];
			auto vt=(vulkan::Texture*)tt.texture;
			vt->InitFramebuffers(deviceContext);
			vk::Framebuffer *vfb=vt->GetVulkanFramebuffer(tt.layer,tt.mip);
			SIMUL_ASSERT(vfb!=nullptr);
			return vfb;
		}
	}
	else
	{
		vk::Framebuffer *vfb=(vk::Framebuffer*)deviceContext.defaultTargetsAndViewport.m_rt[0];
		return vfb;
	}
}

void RenderPlatform::CreateVulkanRenderpass(vk::RenderPass &renderPass,int num_colour,crossplatform::PixelFormat pixelFormat,crossplatform::PixelFormat depthFormat,bool clear)
{
// The initial layout for the color and depth attachments will be LAYOUT_UNDEFINED
// because at the start of the renderpass, we don't care about their contents.
// At the start of the subpass, the color attachment's layout will be transitioned
// to LAYOUT_COLOR_ATTACHMENT_OPTIMAL and the depth stencil attachment's layout
// will be transitioned to LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL.  At the end of
// the renderpass, the color attachment's layout will be transitioned to
// LAYOUT_PRESENT_SRC_KHR to be ready to present.  This is all done as part of
// the renderpass, no barriers are necessary.
	bool depth=(depthFormat!=crossplatform::PixelFormat::UNKNOWN);
	int num_attachments=num_colour+(depth?1:0);
	vk::AttachmentDescription *attachments=new vk::AttachmentDescription[num_attachments];
	vk::AttachmentReference *colour_reference = nullptr;
	if(num_colour!=0)
		colour_reference=new vk::AttachmentReference[num_colour];
	vk::AttachmentReference depth_reference;	
	for(int i=0;i<num_colour;i++)
	{
		attachments[i]=  vk::AttachmentDescription()	 .setFormat(ToVulkanFormat(pixelFormat))
														  .setSamples(vk::SampleCountFlagBits::e1)
														  .setLoadOp(clear?vk::AttachmentLoadOp::eClear:vk::AttachmentLoadOp::eLoad)
														  .setStoreOp(vk::AttachmentStoreOp::eStore)
														  .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
														  .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
														  .setInitialLayout(vk::ImageLayout::eUndefined)
														  .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
		colour_reference[i].setAttachment(i).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
	}
	
	if(depth)
	{
		attachments[num_attachments-1]=  vk::AttachmentDescription()	 .setFormat(ToVulkanFormat(depthFormat))
														  .setSamples(vk::SampleCountFlagBits::e1)
														  .setLoadOp(clear?vk::AttachmentLoadOp::eClear:vk::AttachmentLoadOp::eLoad)
														  .setStoreOp(vk::AttachmentStoreOp::eStore)
														  .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
														  .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
														  .setInitialLayout(vk::ImageLayout::eUndefined)
														  .setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);
		depth_reference.setAttachment(num_attachments-1).setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
	}
	
	auto const subpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setInputAttachmentCount(0)
		.setPInputAttachments(nullptr)
		.setColorAttachmentCount(num_colour)
		.setPColorAttachments(colour_reference)
		.setPResolveAttachments(nullptr)
		.setPDepthStencilAttachment(depth?&depth_reference:nullptr)
		.setPreserveAttachmentCount(0)
		.setPPreserveAttachments(nullptr);

	auto const rp_info = vk::RenderPassCreateInfo()
		.setAttachmentCount(num_attachments)
		.setPAttachments(attachments)
		.setSubpassCount(1)
		.setPSubpasses(&subpass)
		.setDependencyCount(0)
		.setPDependencies(nullptr);

	auto result = vulkanDevice->createRenderPass(&rp_info, nullptr, &renderPass);
	SetVulkanName(this,&renderPass,base::QuickFormat("RenderPass"));
	delete [] attachments;
	delete [] colour_reference;
	SIMUL_ASSERT(result == vk::Result::eSuccess);
}