#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/Vulkan/EffectPass.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"
#include "Simul/Platform/Vulkan/PlatformConstantBuffer.h"
#include "Simul/Platform/Vulkan/PlatformStructuredBuffer.h"
#include "Simul/Platform/Vulkan/Texture.h"

using namespace simul;
using namespace vulkan;

EffectPass::EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *e):
    crossplatform::EffectPass(r,e)
	,mLastFrameIndex(0)
	,mInternalFrameIndex(0)
	,mCurApplyCount(0)
{
	for(int i=0;i<kNumBuffers;i++)
		i_desc[i]=mDescriptorSets[i].begin();
}

EffectPass::~EffectPass()
{
    InvalidateDeviceObjects();
}

void EffectPass::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	
	if(!vulkanDevice)
		return;
	for(auto i:mRenderPasses)
	{
		vulkanDevice->destroyRenderPass(i.second.mRenderPass, nullptr);
		vulkanDevice->destroyPipeline(i.second.mPipeline, nullptr);
		vulkanDevice->destroyPipelineCache(i.second.mPipelineCache, nullptr);
	}
	mRenderPasses.clear();

	vulkanDevice->destroyPipelineLayout(mPipelineLayout, nullptr);
	vulkanDevice->destroyDescriptorSetLayout(mDescLayout, nullptr);
	vulkanDevice->destroyDescriptorPool(mDescriptorPool,nullptr);
	for(int i=0;i<kNumBuffers;i++)
	{
		for(auto r:mDescriptorSets[i])
		{
			//vulkanDevice->destroydescript(r);
		}
		mDescriptorSets[i].clear();
		i_desc[i]=mDescriptorSets[i].begin();
	}
	renderPlatform=nullptr;
	delete [] layout_bindings;
	
	layout_bindings=nullptr;
	initialized=false;
}

void EffectPass::ApplyContextState(crossplatform::DeviceContext &deviceContext,vk::DescriptorSet &descriptorSet)
{
    crossplatform::ContextState* cs = &deviceContext.contextState;
	
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;
	if(!commandBuffer)
		return ;
	vk::Device *vulkanDevice	=renderPlatform->AsVulkanDevice();
	
    vulkan::Shader* c	= (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	
    // If valid, activate render states:
    if (blendState)
    {
        deviceContext.renderPlatform->SetRenderState(deviceContext, blendState);
    }
    if (depthStencilState)
    {
        deviceContext.renderPlatform->SetRenderState(deviceContext, depthStencilState);
    }
    if (rasterizerState)
    {
        deviceContext.renderPlatform->SetRenderState(deviceContext, rasterizerState);
    }
	
	int num_descr=numResourceSlots+numRwResourceSlots
			+numSbResourceSlots
			+numRwSbResourceSlots
			+numSamplerResourceSlots
			+numConstantBufferResourceSlots;

	vk::WriteDescriptorSet *writes=new vk::WriteDescriptorSet[num_descr];
	
	cs->textureSlots =0;
	cs->rwTextureSlots =0;
	cs->bufferSlots =0;
	cs->rwTextureSlotsForSB =0;
	cs->textureSlotsForSB =0;
	
	vk::DescriptorImageInfo tex_desc[32];
	int b=0;
	for(int i=0;i<numResourceSlots;i++,b++)
	{
		int slot=resourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		auto &ta = cs->textureAssignmentMap[slot];
		vk::ImageView *t;
		vulkan::Texture *texture=(vulkan::Texture *)(ta.texture);
		if(!texture||!texture->IsValid())
		{
			// We really don't want to have to do this, but Vulkan GLSL can't eliminate unused textures in compilation:
			if(ta.resourceType==crossplatform::ShaderResourceType::UNKNOWN)
			{
				ta.resourceType=effect->GetShaderResourceAtSlot(slot)->shaderResourceType;
			}
			texture=((vulkan::RenderPlatform*)renderPlatform)->GetDummyTexture(ta.resourceType);
		}
		texture->FinishLoading(deviceContext);
		t=texture->AsVulkanImageView(ta.resourceType,ta.index,ta.mip);
		texture->SetLayout(deviceContext,vk::ImageLayout::eShaderReadOnlyOptimal,ta.index,ta.mip);
		write.setDstBinding(GenerateTextureSlot(slot));
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eSampledImage);
		if(t)
			tex_desc[i].setImageView(*t);
		tex_desc[i].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		write.setPImageInfo(&tex_desc[i]);
		cs->textureSlots |= 1 << slot;
	}
	vk::DescriptorImageInfo img_desc[8];
	for(int i=0;i<numRwResourceSlots;i++,b++)
	{
		int slot=rwResourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		auto &ta = cs->rwTextureAssignmentMap[slot];
		vk::ImageView *t;
		vulkan::Texture *texture=(vulkan::Texture *)(ta.texture);
		if(texture&&texture->IsValid())
		{
			t=texture->AsVulkanImageView(ta.resourceType,ta.index,ta.mip,true);
			texture->SetLayout(deviceContext,vk::ImageLayout::eGeneral,ta.index,ta.mip);
		}
		else
			t=nullptr;	// This WILL kill vulkan, so don't let it happen.
		write.setDstBinding(GenerateTextureWriteSlot(slot));
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageImage);
		if(t)
			img_desc[i].setImageView(*t);
		img_desc[i].setImageLayout(vk::ImageLayout::eGeneral);
		write.setPImageInfo(&img_desc[i]);
		
		cs->rwTextureSlots |= 1 << slot;
	}
	vk::DescriptorBufferInfo sbInfo[8];
	for(int i=0;i<numSbResourceSlots;i++,b++)
	{
		int slot=sbResourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateTextureSlot(slot));
		crossplatform::PlatformStructuredBuffer *sb =cs->applyStructuredBuffers[slot];
		if(!sb)
		{
			SIMUL_CERR<<"No structured buffer found\n";
		}
		else
		{
			vulkan::PlatformStructuredBuffer *psb=(vulkan::PlatformStructuredBuffer*)sb;
			psb->ActualApply(deviceContext,this,slot,false);
			vk::Buffer *bf=psb->GetLastBuffer();
			write.setDescriptorCount(1);
			write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			SIMUL_ASSERT(bf!=nullptr);
			if(bf)
			{
				sbInfo[i].setOffset(psb->GetLastOffset()).setRange(psb->GetSize()).setBuffer(*bf);
				write.setPBufferInfo(&sbInfo[i]);
			}
		}
	}
	vk::DescriptorBufferInfo rwSbInfo[8];
	for(int i=0;i<numRwSbResourceSlots;i++,b++)
	{
		int slot=rwSbResourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateTextureWriteSlot(slot));
		crossplatform::PlatformStructuredBuffer *sb =cs->applyRwStructuredBuffers[slot];
		vulkan::PlatformStructuredBuffer *psb=(vulkan::PlatformStructuredBuffer*)sb;
		psb->ActualApply(deviceContext,this, slot,true);
		vk::Buffer *bf=psb->GetLastBuffer();
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
		SIMUL_ASSERT(bf!=nullptr);
		if(bf)
		{
			rwSbInfo[i].setOffset(psb->GetLastOffset()).setRange(psb->GetSize()).setBuffer(*bf);
			write.setPBufferInfo(&rwSbInfo[i]);
		}
	}
	vk::DescriptorImageInfo samplerInfo[16];
	for(int i=0;i<numSamplerResourceSlots;i++,b++)
	{
		int slot=samplerResourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateSamplerSlot(slot));
		crossplatform::SamplerState *ss=nullptr;
        if (deviceContext.contextState.samplerStateOverrides.size() > 0 && deviceContext.contextState.samplerStateOverrides.HasValue(slot))
        {
            ss = deviceContext.contextState.samplerStateOverrides[slot];
            // We dont override this slot, just take a default sampler state:
        }
		if(!ss)
        {
            ss = effect->GetSamplers()[slot];
        }
		auto *s=ss->AsVulkanSampler();
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eSampler);
		SIMUL_ASSERT(s!=nullptr);
		if(s)
		{
			samplerInfo[i].setSampler(*s);
			write.setPImageInfo(&samplerInfo[i]);
		}

	}
	vk::DescriptorBufferInfo bufferInfo[8];
	for(int i=0;i<numConstantBufferResourceSlots;i++,b++)
	{
		int slot=constantBufferResourceSlots[i];
		crossplatform::ConstantBufferBase *cb=cs->applyBuffers[slot];
		if (!cb)
			continue;
		vulkan::PlatformConstantBuffer *pcb=(vulkan::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateConstantBufferSlot(slot));
		pcb->ActualApply(deviceContext,this,0);
		vk::Buffer *bf				=pcb->GetLastBuffer();
		vk::DeviceSize deviceSize	=pcb->GetSize();
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
		SIMUL_ASSERT(bf!=nullptr);
		if(bf)
		{
			bufferInfo[i].setOffset(pcb->GetLastOffset())
				.setRange(deviceSize)
				.setBuffer(*bf);
			write.setPBufferInfo(&bufferInfo[i]);
		}
		cs->bufferSlots|=(1<<slot);
	}

	vulkanDevice->updateDescriptorSets(num_descr, writes, 0,nullptr);

	static bool error_checking=true;
	// Now verify that ALL resource are set:
	if(error_checking)
	{
		unsigned required_slots=GetTextureSlots();
		if((cs->textureSlots&required_slots)!=required_slots)
		{
			static int count=10;
			count--;
			if(count>0)
			{
				SIMUL_CERR<<"Not all texture slots are assigned:"<<std::endl;
				unsigned missing_slots=required_slots&(~cs->textureSlots);
				for(unsigned i=0;i<32;i++)
				{
					unsigned slot=1<<i;
					if(slot&missing_slots)
					{
						std::string name;
						if(cs->currentEffect)
							name=cs->currentEffect->GetTextureForSlot(i);
						SIMUL_CERR<<"\tSlot "<<i<<": "<<name.c_str()<<", was not set."<<std::endl;
					}
				}
				SIMUL_BREAK_ONCE("Many API's require all used textures to have valid data.");
			}
		}
		unsigned required_rw_slots=GetRwTextureSlots();
		if((cs->rwTextureSlots&required_rw_slots)!=required_rw_slots)
		{
			static int count=10;
			count--;
			if(count>0)
			{
				SIMUL_BREAK_ONCE("Not all rw texture slots are assigned.");
				required_rw_slots=required_rw_slots&(~cs->rwTextureSlots);
				for(unsigned i=0;i<32;i++)
				{
					unsigned slot=1<<i;
					if(slot&required_rw_slots)
					{
						std::string name;
						if(cs->currentEffect)
							name=cs->currentEffect->GetTextureForSlot(1000+i);
						SIMUL_CERR<<"RW Slot "<<i<<" was not set ("<<name.c_str()<<")."<<std::endl;
					}
				}
			}
		}
	}
	crossplatform::PixelFormat pixelFormat=crossplatform::PixelFormat::UNKNOWN;
	if(!c)
		pixelFormat=vulkanRenderPlatform->GetActivePixelFormat(deviceContext);
	const auto &p=mRenderPasses.find(pixelFormat);
	RenderPassPipeline *renderPassPipeline=nullptr;
	if(p==mRenderPasses.end())
	{
		renderPassPipeline=&mRenderPasses[pixelFormat];
		InitializePipeline(deviceContext,renderPassPipeline,pixelFormat);
	}
	else
		renderPassPipeline=&(mRenderPasses[pixelFormat]);
	if(c)
	{
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute,renderPassPipeline->mPipeline);
		commandBuffer->bindDescriptorSets( vk::PipelineBindPoint::eCompute, mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	}
	else
	{
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics,renderPassPipeline->mPipeline);
		commandBuffer->bindDescriptorSets( vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	}
}

int EffectPass::GenerateSamplerSlot(int s,bool offset)
{
	if(offset)
		return s+300;
	return s;
}
int EffectPass::GenerateTextureSlot(int s,bool offset)
{
	if(offset)
		return s+100;
	return s;
}
int EffectPass::GenerateTextureWriteSlot(int s,bool offset)
{
	if(offset)
		return s+200;
	return s;
}
int EffectPass::GenerateConstantBufferSlot(int s,bool offset)
{
	return s;
}

void EffectPass::Initialize()
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	initialized=true;
	int swapchainImageCount=SIMUL_VULKAN_FRAME_LAG+1;
	int num_descr=numResourceSlots
					+numRwResourceSlots
					+numSbResourceSlots
					+numRwSbResourceSlots
					+numSamplerResourceSlots
					+numConstantBufferResourceSlots;
	// Create the "Descriptor Pool":
	if(!num_descr)
		return;
	vk::DescriptorPoolSize *poolSizes= new vk::DescriptorPoolSize[num_descr];// won't actually use that many unless we have 1 of each type.
	int p=0;
	static int count_per_frame=256;
	if(numResourceSlots)																		
		poolSizes[p++].setType(vk::DescriptorType::eSampledImage)			.setDescriptorCount(count_per_frame*swapchainImageCount * numResourceSlots);
	if(numSamplerResourceSlots)																	
		poolSizes[p++].setType(vk::DescriptorType::eSampler)				.setDescriptorCount(count_per_frame*swapchainImageCount * numSamplerResourceSlots);
	if(numSbResourceSlots+numRwSbResourceSlots)																		
		poolSizes[p++].setType(vk::DescriptorType::eStorageBuffer)			.setDescriptorCount(count_per_frame*swapchainImageCount * (numSbResourceSlots+numRwSbResourceSlots));
	//if(numRwSbResourceSlots)																	
	//	poolSizes[p++].setType(vk::DescriptorType::eStorageBufferDynamic)	.setDescriptorCount(count_per_frame*swapchainImageCount * numRwSbResourceSlots);
	if(numRwResourceSlots)																		
		poolSizes[p++].setType(vk::DescriptorType::eStorageImage)			.setDescriptorCount(count_per_frame*swapchainImageCount * numRwResourceSlots);
	if(numConstantBufferResourceSlots)															
		poolSizes[p++].setType(vk::DescriptorType::eUniformBuffer)			.setDescriptorCount(count_per_frame*swapchainImageCount * numConstantBufferResourceSlots);
	//if(numResourceSlots)
	//	poolSizes[p++].setType(vk::DescriptorType::eCombinedImageSampler)	.setDescriptorCount(count_per_frame*swapchainImageCount * numResourceSlots);

	auto const descriptor_pool =
		vk::DescriptorPoolCreateInfo().setMaxSets(swapchainImageCount*count_per_frame).setPoolSizeCount(p).setPPoolSizes(poolSizes);

	auto result = vulkanDevice->createDescriptorPool(&descriptor_pool, nullptr, &mDescriptorPool);
	SetVulkanName(renderPlatform,&mDescriptorPool,base::QuickFormat("%s Descriptor pool",name.c_str()));
	delete [] poolSizes;
    vulkan::Shader* v   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
    vulkan::Shader* f   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
    vulkan::Shader* c   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

// Create the "Descriptor Set Layout":

	layout_bindings =new vk::DescriptorSetLayoutBinding[num_descr];
	int b=0;
	for(int i=0;i<numResourceSlots;i++,b++)
	{
		int slot=resourceSlots[i];
		vk::DescriptorSetLayoutBinding &binding=layout_bindings[b];
		vk::ShaderStageFlags stageFlags;
		if(f&&f->usesTextureSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eFragment;
		if(v&&v->usesTextureSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eVertex;
		if(c&&c->usesTextureSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eCompute;

		binding.setBinding(GenerateTextureSlot(slot))
				.setDescriptorType(vk::DescriptorType::eSampledImage)
				.setDescriptorCount(1)
				.setStageFlags(stageFlags)
				.setPImmutableSamplers(nullptr);
	}
	for(int i=0;i<numRwResourceSlots;i++,b++)
	{
		int slot=rwResourceSlots[i];
		vk::DescriptorSetLayoutBinding &binding=layout_bindings[b];
		vk::ShaderStageFlags stageFlags;
		if(f&&f->usesRwTextureSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eFragment;
		if(v&&v->usesRwTextureSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eVertex;
		if(c&&c->usesRwTextureSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eCompute;

		binding.setBinding(GenerateTextureWriteSlot(slot))
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(1)
				.setStageFlags(stageFlags)
				.setPImmutableSamplers(nullptr);
	}
	for(int i=0;i<numSbResourceSlots;i++,b++)
	{
		int slot=sbResourceSlots[i];
		vk::DescriptorSetLayoutBinding &binding=layout_bindings[b];
		vk::ShaderStageFlags stageFlags;
		if(f&&f->usesTextureSlotForSB(slot))
			stageFlags|=vk::ShaderStageFlagBits::eFragment;
		if(v&&v->usesTextureSlotForSB(slot))
			stageFlags|=vk::ShaderStageFlagBits::eVertex;
		if(c&&c->usesTextureSlotForSB(slot))
			stageFlags|=vk::ShaderStageFlagBits::eCompute;

		binding.setBinding(GenerateTextureSlot(slot))
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setStageFlags(stageFlags)
				.setPImmutableSamplers(nullptr);
	}
	for(int i=0;i<numRwSbResourceSlots;i++,b++)
	{
		int slot=rwSbResourceSlots[i];
		vk::DescriptorSetLayoutBinding &binding=layout_bindings[b];
		vk::ShaderStageFlags stageFlags;
		if(f&&f->usesRwTextureSlotForSB(slot))
			stageFlags|=vk::ShaderStageFlagBits::eFragment;
		if(v&&v->usesRwTextureSlotForSB(slot))
			stageFlags|=vk::ShaderStageFlagBits::eVertex;
		if(c&&c->usesRwTextureSlotForSB(slot))
			stageFlags|=vk::ShaderStageFlagBits::eCompute;
		binding.setBinding(GenerateTextureWriteSlot(slot))
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setStageFlags(stageFlags)
				.setPImmutableSamplers(nullptr);
	}
	for(int i=0;i<numSamplerResourceSlots;i++,b++)
	{
		int slot=samplerResourceSlots[i];
		vk::DescriptorSetLayoutBinding &binding=layout_bindings[b];
		vk::ShaderStageFlags stageFlags;
		if(f&&f->usesSamplerSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eFragment;
		if(v&&v->usesSamplerSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eVertex;
		if(c&&c->usesSamplerSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eCompute;
		binding.setBinding(GenerateSamplerSlot(slot))
				.setDescriptorType(vk::DescriptorType::eSampler)
				.setDescriptorCount(1)
				.setStageFlags(stageFlags)
				.setPImmutableSamplers(nullptr);
	}
	for(int i=0;i<numConstantBufferResourceSlots;i++,b++)
	{
		int slot=constantBufferResourceSlots[i];
		vk::DescriptorSetLayoutBinding &binding=layout_bindings[b];
		vk::ShaderStageFlags stageFlags;
		if(f&&f->usesConstantBufferSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eFragment;
		if(v&&v->usesConstantBufferSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eVertex;
		if(c&&c->usesConstantBufferSlot(slot))
			stageFlags|=vk::ShaderStageFlagBits::eCompute;
		binding.setBinding(GenerateConstantBufferSlot(slot))
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setStageFlags(stageFlags)
				.setPImmutableSamplers(nullptr);
	}

	auto descriptor_layout = vk::DescriptorSetLayoutCreateInfo().setBindingCount(b).setPBindings(layout_bindings);

	result = vulkanDevice->createDescriptorSetLayout(&descriptor_layout, nullptr, &mDescLayout);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	SetVulkanName(renderPlatform,&mDescLayout,base::QuickFormat("%s Descriptor layout",name.c_str()));

	auto pPipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo().setSetLayoutCount(1).setPSetLayouts(&mDescLayout);

	result = vulkanDevice->createPipelineLayout(&pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	SetVulkanName(renderPlatform,&mPipelineLayout,base::QuickFormat("%s EffectPass Pipeline layout",name.c_str()));
}

void EffectPass::Initialize(vk::DescriptorSet &descriptorSet)
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vk::DescriptorSetLayout		descLayout[3];
	for(int i=0;i<kNumBuffers;i++)
	{
		descLayout[i]=mDescLayout;
	}
	
	vk::DescriptorSetAllocateInfo alloc_info =	vk::DescriptorSetAllocateInfo()
													.setDescriptorPool(mDescriptorPool)
													.setDescriptorSetCount(1)
													.setPSetLayouts(descLayout);
	auto result = vulkanDevice->allocateDescriptorSets(&alloc_info,&descriptorSet);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	SetVulkanName(renderPlatform,&descriptorSet,base::QuickFormat("%s Descriptor set",name.c_str()));
}


void EffectPass::InitializePipeline(crossplatform::DeviceContext &deviceContext,RenderPassPipeline *renderPassPipeline,crossplatform::PixelFormat pixelFormat)
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vk::PipelineCacheCreateInfo pipelineCacheInfo;
	vk::Result result = vulkanDevice->createPipelineCache(&pipelineCacheInfo, nullptr, &renderPassPipeline->mPipelineCache);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	SetVulkanName(renderPlatform,&renderPassPipeline->mPipelineCache,base::QuickFormat("%s EffectPass mPipelineCache",name.c_str()));

    vulkan::Shader* v   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
    vulkan::Shader* f   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
    vulkan::Shader* c   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	if(c)
	{
		vk::PipelineShaderStageCreateInfo shaderStageInfo =
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eCompute).setModule(c->mShader).setPName(c->entryPoint.c_str());
		vk::ComputePipelineCreateInfo computePipelineCreateInfo= vk::ComputePipelineCreateInfo()
																		.setLayout(mPipelineLayout)
																		.setStage(shaderStageInfo);
		//computePipelineCreateInfo	.setFlags(vk::PipelineCreateFlagBits::eDispatchBase);
		vulkanDevice->createComputePipelines(renderPassPipeline->mPipelineCache,1,&computePipelineCreateInfo, nullptr, &renderPassPipeline->mPipeline);
		SetVulkanName(renderPlatform,&renderPassPipeline->mPipeline,base::QuickFormat("%s EffectPass compute mPipeline",name.c_str()));
	}
	else
	{
		auto *rp=vulkanRenderPlatform->GetActiveVulkanRenderPass(deviceContext);
		if(!rp)
		{
			auto color_reference = vk::AttachmentReference().setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
			auto depth_reference = vk::AttachmentReference().setAttachment(1).setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			auto subpass = vk::SubpassDescription()
							.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

			subpass		.setInputAttachmentCount(0)
						.setPInputAttachments(nullptr)
						.setColorAttachmentCount(1)
						.setPColorAttachments(&color_reference);
		
			vk::AttachmentDescription attachments[2] = { vk::AttachmentDescription()
																  .setFormat(vulkan::RenderPlatform::ToVulkanFormat(pixelFormat))
																  .setSamples(vk::SampleCountFlagBits::e1)
																  .setLoadOp(vk::AttachmentLoadOp::eDontCare)
																  .setStoreOp(vk::AttachmentStoreOp::eStore)
																  .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
																  .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
																  .setInitialLayout(vk::ImageLayout::eUndefined)
																  .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
															  vk::AttachmentDescription()
																  .setFormat(vk::Format::eD32Sfloat)
																  .setSamples(vk::SampleCountFlagBits::e1)
																  .setLoadOp(vk::AttachmentLoadOp::eDontCare)
																  .setStoreOp(vk::AttachmentStoreOp::eDontCare)
																  .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
																  .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
																  .setInitialLayout(vk::ImageLayout::eUndefined)
																  .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal) };

			auto rp_info = vk::RenderPassCreateInfo()
				.setAttachmentCount(1)
				.setPAttachments(attachments)
				.setSubpassCount(1)
				.setPSubpasses(&subpass)
				.setDependencyCount(0)
				.setPDependencies(nullptr);

			result = vulkanDevice->createRenderPass(&rp_info, nullptr, &renderPassPipeline->mRenderPass);
			SIMUL_ASSERT(result == vk::Result::eSuccess);
			SetVulkanName(renderPlatform,&renderPassPipeline->mRenderPass,base::QuickFormat("%s EffectPass mRenderPass",name.c_str()));
		}
		
		vk::PipelineShaderStageCreateInfo shaderStageInfo[2] = {
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eVertex).setModule(v->mShader).setPName(v->entryPoint.c_str()),
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eFragment).setModule(f->mShader).setPName(f->entryPoint.c_str())
		};
		
		vk::PipelineViewportStateCreateInfo viewportInfo			= vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);
		
		vk::PolygonMode polygonMode				= vk::PolygonMode::eFill;
		vk::CullModeFlags cullModeFlags	= vk::CullModeFlagBits::eNone;
		vk::FrontFace frontFace					= vk::FrontFace::eCounterClockwise;
		if (rasterizerState)
		{
			polygonMode = vulkan::RenderPlatform::toVulkanPolygonMode(rasterizerState->desc.rasterizer.polygonMode);
			cullModeFlags = vulkan::RenderPlatform::toVulkanCullFace(rasterizerState->desc.rasterizer.cullFaceMode);
			if(rasterizerState->desc.rasterizer.frontFace == crossplatform::FrontFace::FRONTFACE_CLOCKWISE)
				frontFace = vk::FrontFace::eClockwise;

		}

		vk::PipelineRasterizationStateCreateInfo rasterizationInfo	= vk::PipelineRasterizationStateCreateInfo()
																		.setDepthClampEnable(VK_FALSE)
																		.setRasterizerDiscardEnable(VK_FALSE)
																		.setPolygonMode(polygonMode)
																		.setCullMode(cullModeFlags)
																		.setFrontFace(frontFace)
																		.setDepthBiasEnable(VK_FALSE)
																		.setLineWidth(1.0f);
		vk::PipelineMultisampleStateCreateInfo multisampleInfo		= vk::PipelineMultisampleStateCreateInfo();
		vk::StencilOpState stencilOp								= vk::StencilOpState().setFailOp(vk::StencilOp::eKeep).setPassOp(vk::StencilOp::eKeep).setCompareOp(vk::CompareOp::eAlways);
		vk::PipelineDepthStencilStateCreateInfo depthStencilInfo	= vk::PipelineDepthStencilStateCreateInfo()
																		.setDepthTestEnable(VK_FALSE)
																		.setDepthWriteEnable(VK_FALSE)
																		.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
																		.setDepthBoundsTestEnable(VK_FALSE)
																		.setStencilTestEnable(VK_FALSE)
																		.setFront(stencilOp)
																		.setBack(stencilOp);
		if(depthStencilState)
		{
			depthStencilInfo	.setDepthTestEnable(depthStencilState->desc.depth.test)
																		.setDepthWriteEnable(depthStencilState->desc.depth.write)
																		.setDepthCompareOp(vulkanRenderPlatform->toVulkanComparison(depthStencilState->desc.depth.comparison))
																		.setDepthBoundsTestEnable(VK_FALSE)
																		.setStencilTestEnable(VK_FALSE)
																		.setFront(stencilOp)
																		.setBack(stencilOp);
		}

		
		vk::DynamicState dynamicStates[2]							= { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		auto dynamicStateInfo										= vk::PipelineDynamicStateCreateInfo().setPDynamicStates(dynamicStates).setDynamicStateCount(2);
	
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo	= vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleStrip);
		if(topology!=crossplatform::Topology::UNDEFINED)
		{
			inputAssemblyInfo.setTopology(vulkan::RenderPlatform::toVulkanTopology(topology));
			//	rasterizerState->type;
		}
		vk::PipelineColorBlendStateCreateInfo colorBlendInfo		= vk::PipelineColorBlendStateCreateInfo();
        colorBlendInfo.setLogicOpEnable(false);
        colorBlendInfo.setLogicOp (vk::LogicOp::eCopy);
		colorBlendInfo.setBlendConstants({ 1.0f,1.0f,1.0f,1.0f});
		vk::PipelineColorBlendAttachmentState colorBlendAttachments[1] = {	vk::PipelineColorBlendAttachmentState()};
		if(blendState)
		{
			for(int i=0;i<1;i++)
			{
				auto &b=colorBlendAttachments[i];
				auto &c=blendState->desc.blend.RenderTarget[i];
				b.setBlendEnable(c.blendOperationAlpha!=crossplatform::BLEND_OP_NONE||c.blendOperation!=crossplatform::BLEND_OP_NONE);
				b.setAlphaBlendOp(vulkan::RenderPlatform::toVulkanBlendOperation(c.blendOperationAlpha));
				b.setColorBlendOp(vulkan::RenderPlatform::toVulkanBlendOperation(c.blendOperation));
				b.setColorWriteMask(vk::ColorComponentFlagBits::eR
									| vk::ColorComponentFlagBits::eG
									|vk::ColorComponentFlagBits::eB
									| vk::ColorComponentFlagBits::eA);
				b.setDstAlphaBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.DestBlendAlpha));
				b.setDstColorBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.DestBlend));
				b.setSrcAlphaBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.SrcBlendAlpha));
				b.setSrcColorBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.SrcBlend));
			}
		}
		colorBlendInfo.setAttachmentCount(1).setPAttachments(colorBlendAttachments);
		
		// from the vertex shader's layout:
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vk::VertexInputBindingDescription bindingDescription ;
		bindingDescription.setBinding (0);
		bindingDescription.setStride( v->layout.GetStructSize());
		bindingDescription.setInputRate(vk::VertexInputRate::eVertex);

		const auto &layoutDesc=v->layout.GetDesc();
		vk::VertexInputAttributeDescription *vertexInputs=nullptr;
		if(layoutDesc.size())
			vertexInputs=new vk::VertexInputAttributeDescription[layoutDesc.size()];
		int slot=0;
		for(auto desc:layoutDesc)
		{
			vertexInputs[slot].binding = 0;
			vertexInputs[slot].location = slot;
			vertexInputs[slot].format = RenderPlatform::ToVulkanFormat(desc.format);
			vertexInputs[slot].offset = desc.alignedByteOffset;
			slot++;
		}
		
		if(layoutDesc.size())
		{
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)layoutDesc.size();
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.pVertexAttributeDescriptions = vertexInputs;
		}

	
		vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo	= vk::GraphicsPipelineCreateInfo()
																		.setStageCount(2)
																		.setPStages(shaderStageInfo)
																		.setPVertexInputState(&vertexInputInfo)
																		.setPInputAssemblyState(&inputAssemblyInfo)
																		.setPViewportState(&viewportInfo)
																		.setPRasterizationState(&rasterizationInfo)
																		.setPMultisampleState(&multisampleInfo)
																		.setPDepthStencilState(&depthStencilInfo)
																		.setPColorBlendState(&colorBlendInfo)
																		.setPDynamicState(&dynamicStateInfo)
																		.setLayout(mPipelineLayout)
																		.setRenderPass(rp?*rp:renderPassPipeline->mRenderPass);
		vulkanDevice->createGraphicsPipelines(renderPassPipeline->mPipelineCache,1,&graphicsPipelineCreateInfo, nullptr, &renderPassPipeline->mPipeline);
		SetVulkanName(renderPlatform,&renderPassPipeline->mPipeline,base::QuickFormat("%s EffectPass renderPass Pipeline",name.c_str()));
		if(vertexInputs)
			delete []vertexInputs;
	}
}

void EffectPass::Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) 
{
	auto rPlat = (vulkan::RenderPlatform*)renderPlatform;
	auto curFrameIndex = rPlat->GetIdx();
	// If new frame, update current frame index and reset the apply count
	if (mLastFrameIndex != deviceContext.frame_number)
	{
		mCurApplyCount = 0;
		mInternalFrameIndex++;
		mInternalFrameIndex=mInternalFrameIndex%3;
		i_desc[mInternalFrameIndex]=mDescriptorSets[mInternalFrameIndex].begin();
		mLastFrameIndex = deviceContext.frame_number;
	}
	if(!initialized)
		Initialize();
	if(i_desc[mInternalFrameIndex]==mDescriptorSets[mInternalFrameIndex].end())
	{
		//must insert a new descriptor:
		mDescriptorSets[mInternalFrameIndex].push_back(vk::DescriptorSet());
		i_desc[mInternalFrameIndex]=mDescriptorSets[mInternalFrameIndex].end();
		i_desc[mInternalFrameIndex]--;
		Initialize(*i_desc[mInternalFrameIndex]);
		SIMUL_ASSERT(((VkDescriptorSet)*(i_desc[mInternalFrameIndex]))!=nullptr);
	}
	ApplyContextState(deviceContext,*i_desc[mInternalFrameIndex]);
	mCurApplyCount++;
	if(i_desc[mInternalFrameIndex]!=mDescriptorSets[mInternalFrameIndex].end())
		i_desc[mInternalFrameIndex]++;
}

void EffectPass::SetTextureHandles(crossplatform::DeviceContext & deviceContext)
{
}

vk::RenderPass &EffectPass::GetVulkanRenderPass(crossplatform::DeviceContext & deviceContext,crossplatform::PixelFormat pixelFormat)
{
	auto *rp=vulkanRenderPlatform->GetActiveVulkanRenderPass(deviceContext);
	if(rp)
		return *rp;
	const auto &p=mRenderPasses.find(pixelFormat);
	if(p==mRenderPasses.end())
	{
		InitializePipeline(deviceContext,&mRenderPasses[pixelFormat],pixelFormat);
	}

	return mRenderPasses[pixelFormat].mRenderPass;
}

void EffectPass::MapTexturesToUBO(crossplatform::Effect* curEffect)
{
    char kTexHandleUbo []  = "_TextureHandles_X";
	char ext[]={'v','h','d','g','p','c'};
	// Different buffer object for each shader type.
	for(int i=0;i<crossplatform::ShaderType::SHADERTYPE_COUNT;i++)
	{
		kTexHandleUbo[16]=ext[i];
    }
}
