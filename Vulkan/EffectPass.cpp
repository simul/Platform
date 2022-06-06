#include <vulkan/vulkan.hpp>
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Vulkan/EffectPass.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/PlatformConstantBuffer.h"
#include "Platform/Vulkan/PlatformStructuredBuffer.h"
#include "Platform/Vulkan/Texture.h"
#if INCLUDE_FRAMEWORK_VULKAN
#include <Platform/Vulkan/Framework_Vulkan.h>
#endif
using namespace platform;
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
	vulkan::RenderPlatform *rp=(vulkan::RenderPlatform *)renderPlatform;
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	
	if(!vulkanDevice)
		return;
	for(auto i:mRenderPasses)
	{
		rp->PushToReleaseManager(i.second.mRenderPass);
		rp->PushToReleaseManager(i.second.mPipeline);
		rp->PushToReleaseManager(i.second.mPipelineCache);
	}
	mRenderPasses.clear();

	rp->PushToReleaseManager(mPipelineLayout);
	rp->PushToReleaseManager(mDescLayout);
	rp->PushToReleaseManager(mDescriptorPool);
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
	if(descriptorSet.operator VkDescriptorSet()==0)
	{
		SIMUL_BREAK("Null Descriptorset!");
	}
    crossplatform::ContextState* cs = &deviceContext.contextState;
	
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;
	if(!commandBuffer)
		return;
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
		auto res=effect->GetShaderResourceAtSlot(slot);
		crossplatform::ShaderResourceType requiredType=crossplatform::ShaderResourceType::UNKNOWN;
		if(texture&&res&&ta.resourceType!=res->shaderResourceType)
		{
			requiredType=res->shaderResourceType;
			ta.resourceType=requiredType;
			texture=nullptr;
		}
		if(!texture||!texture->IsValid())
		{
			// We really don't want to have to do this, but Vulkan GLSL can't eliminate unused textures in compilation:
			if(ta.resourceType==crossplatform::ShaderResourceType::UNKNOWN)
			{
				ta.resourceType=requiredType;
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
		{
			// probably not actually used in the shader.
			num_descr--;
			b--;
			continue;
		}
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
		if(!sb)
		{
			// probably not actually used in the shader.
			num_descr--;
			b--;
			continue;
		}
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
		// TODO: this is overkill, synchronizing all uav's
		renderPlatform->ResourceBarrierUAV(deviceContext,sb);
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
		{
			num_descr--;
			b--;
			SIMUL_BREAK_ONCE("Possibly missing constant buffer");
			continue;
		}
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
	if (num_descr)
	{
		for (int i = 0; i < num_descr; i++)
		{
			vk::WriteDescriptorSet& write = writes[i];
			bool no_res = write.pImageInfo == nullptr && write.pBufferInfo == nullptr && write.pTexelBufferView == nullptr;
			
			if (no_res)
			{
				SIMUL_CERR << "VkWriteDescriptorSet (Binding = " << write.dstBinding << ") in pass '"
					<< name.c_str() << "' has no valid resource associated with it." << std::endl;
				SIMUL_BREAK("VkWriteDescriptorSet error.");
			}
		}
		vulkanDevice->updateDescriptorSets(num_descr, writes, 0, nullptr);
	}

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
				//SIMUL_BREAK_ONCE("Not all rw texture slots are assigned.");
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
	crossplatform::GraphicsDeviceContext *graphicsDeviceContext=deviceContext.AsGraphicsDeviceContext();
	RenderPassHash  hashval=0 ;
	RenderPassPipeline *renderPassPipeline=nullptr;
	if(c)
	{
		const auto &p=mRenderPasses.find(hashval);
		if(p==mRenderPasses.end())
		{
			renderPassPipeline=&mRenderPasses[hashval];
			InitializePipeline(deviceContext,renderPassPipeline,crossplatform::PixelFormat::UNKNOWN, crossplatform::Topology::UNDEFINED);
		}
		else
			renderPassPipeline=&(mRenderPasses[hashval]);
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute,renderPassPipeline->mPipeline);
		if(descriptorSet)
			commandBuffer->bindDescriptorSets( vk::PipelineBindPoint::eCompute, mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	}
	else
	{
		crossplatform::PixelFormat pixelFormat=crossplatform::PixelFormat::UNKNOWN;
		pixelFormat=vulkanRenderPlatform->GetActivePixelFormat(*graphicsDeviceContext);
		crossplatform::Topology appliedTopology = topology;
		if (cs->topology != crossplatform::Topology::UNDEFINED)
			appliedTopology = cs->topology;
		if (appliedTopology == crossplatform::Topology::UNDEFINED)
			appliedTopology = crossplatform::Topology::TRIANGLESTRIP;
		hashval = MakeRenderPassHash(pixelFormat, appliedTopology, cs->currentLayout, blendState, depthStencilState, rasterizerState);
		const auto &p=mRenderPasses.find(hashval);
		if(p==mRenderPasses.end())
		{
			renderPassPipeline=&mRenderPasses[hashval];
			InitializePipeline(deviceContext,renderPassPipeline,pixelFormat, appliedTopology, blendState, depthStencilState, rasterizerState);
		}
		else
			renderPassPipeline=&(mRenderPasses[hashval]);
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics,renderPassPipeline->mPipeline);
		if(descriptorSet)
			commandBuffer->bindDescriptorSets( vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

		// Now figure out the layout business for the rendertargets:
		crossplatform::TargetsAndViewport *tv;
		if(graphicsDeviceContext->targetStack.size())
			tv=graphicsDeviceContext->targetStack.top();
		else
			tv=&(graphicsDeviceContext->defaultTargetsAndViewport);
	#if 1
		for(int i=0;i<tv->num;i++)
		{
			auto &tt= tv->textureTargets[i];
			if(tv->textureTargets[i].texture)
				((vulkan::Texture*)tt.texture)->SetLayout(*graphicsDeviceContext,vk::ImageLayout::eColorAttachmentOptimal,tt.layer,tt.mip);
		}
		if(tv->depthTarget.texture&&depthStencilState)
		{
			auto& dt = tv->depthTarget;
			if(depthStencilState->desc.depth.write)
				((vulkan::Texture*)tv->depthTarget.texture)->SetLayout(*graphicsDeviceContext,vk::ImageLayout::eDepthStencilAttachmentOptimal,dt.layer,dt.mip);
			else if(depthStencilState->desc.depth.test)
				((vulkan::Texture*)tv->depthTarget.texture)->SetLayout(*graphicsDeviceContext,vk::ImageLayout::eDepthStencilReadOnlyOptimal,dt.layer,dt.mip);
		}
	#endif
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
	vk::Result result;
	int b=0;
	// Create the "Descriptor Pool":
	if(num_descr)
	{
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

		result = vulkanDevice->createDescriptorPool(&descriptor_pool, nullptr, &mDescriptorPool);
		SIMUL_VK_CHECK(result);
		SetVulkanName(renderPlatform,&mDescriptorPool,platform::core::QuickFormat("%s Descriptor pool",name.c_str()));
		delete [] poolSizes;
		vulkan::Shader* v   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
		vulkan::Shader* f   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
		vulkan::Shader* c   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

	// Create the "Descriptor Set Layout":

		layout_bindings =new vk::DescriptorSetLayoutBinding[num_descr];
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
	}
	else
	{
		SIMUL_COUT<<"No inputs to shader"<<std::endl;
	}
	auto descriptor_layout = vk::DescriptorSetLayoutCreateInfo().setBindingCount(b);
	if(layout_bindings)
		descriptor_layout=descriptor_layout.setPBindings(layout_bindings);

	result = vulkanDevice->createDescriptorSetLayout(&descriptor_layout, nullptr, &mDescLayout);
	SIMUL_VK_CHECK(result);
	SetVulkanName(renderPlatform,&mDescLayout,this->name+" descriptor set Layout");
	SetVulkanName(renderPlatform,&mDescLayout,platform::core::QuickFormat("%s Descriptor layout",name.c_str()));

	auto pPipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo().setSetLayoutCount(1).setPSetLayouts(&mDescLayout);

	result = vulkanDevice->createPipelineLayout(&pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout);
	SIMUL_VK_CHECK(result);
	SetVulkanName(renderPlatform,&mPipelineLayout,platform::core::QuickFormat("%s EffectPass Pipeline layout",name.c_str()));
}

void EffectPass::Initialize(vk::DescriptorSet &descriptorSet)
{
	// Of course, only need to do this if there are ANY inputs.
	if(mDescLayout.operator VkDescriptorSetLayout()==nullptr)
		return;
		
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vk::DescriptorSetLayout		descLayout[3];
	for(int i=0;i<kNumBuffers;i++)
	{
		descLayout[i]=mDescLayout;
	}
	
	vk::DescriptorSetAllocateInfo alloc_info =	vk::DescriptorSetAllocateInfo()
													.setDescriptorSetCount(1)
													.setPSetLayouts(descLayout);
	if(mDescriptorPool)
	{
		alloc_info=alloc_info.setDescriptorPool(mDescriptorPool);
		auto result = vulkanDevice->allocateDescriptorSets(&alloc_info,&descriptorSet);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		SetVulkanName(renderPlatform,&descriptorSet,platform::core::QuickFormat("%s Descriptor set",name.c_str()));
	}
	else
	{
//		SIMUL_COUT<<"No descriptor pool because no inputs."<<std::endl;
	}
}
#define VK_ALLOCATOR nullptr
void EffectPass::CreateTestPipeline(vk::Device *device,RenderPassPipeline *renderPassPipeline,vulkan::Shader* v,vulkan::Shader* f,vk::RenderPass* rp)
{
#if INCLUDE_FRAMEWORK_VULKAN
   /* pipeline->rop = parms->rop;
    pipeline->program = parms->program;
    pipeline->geometry = parms->geometry;
    pipeline->vertexAttributeCount = 0;
    pipeline->vertexBindingCount = 0;
	
    InitVertexAttributes(
        false,
        parms->geometry->layout,
        parms->geometry->vertexCount,
        parms->geometry->vertexAttribsFlags,
        parms->program->vertexAttribsFlags,
        pipeline->vertexAttributes,
        &pipeline->vertexAttributeCount,
        pipeline->vertexBindings,
        &pipeline->vertexBindingCount,
        pipeline->vertexBindingOffsets);
		
    pipeline->firstInstanceBinding = pipeline->vertexBindingCount;

    InitVertexAttributes(
        true,
        parms->geometry->layout,
        parms->geometry->instanceCount,
        parms->geometry->instanceAttribsFlags,
        parms->program->vertexAttribsFlags,
        pipeline->vertexAttributes,
        &pipeline->vertexAttributeCount,
        pipeline->vertexBindings,
        &pipeline->vertexBindingCount,
        pipeline->vertexBindingOffsets);
    pipeline->vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipeline->vertexInputState.pNext = NULL;
    pipeline->vertexInputState.flags = 0;
    pipeline->vertexInputState.vertexBindingDescriptionCount = pipeline->vertexBindingCount;
    pipeline->vertexInputState.pVertexBindingDescriptions = pipeline->vertexBindings;
    pipeline->vertexInputState.vertexAttributeDescriptionCount = pipeline->vertexAttributeCount;
    pipeline->vertexInputState.pVertexAttributeDescriptions = pipeline->vertexAttributes;

    pipeline->inputAssemblyState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline->inputAssemblyState.pNext = NULL;
    pipeline->inputAssemblyState.flags = 0;
    pipeline->inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline->inputAssemblyState.primitiveRestartEnable = VK_FALSE;
	
		*/
    VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo;
    tessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellationStateCreateInfo.pNext = NULL;
    tessellationStateCreateInfo.flags = 0;
    tessellationStateCreateInfo.patchControlPoints = 0;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = NULL;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = NULL;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = NULL;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = NULL;
    rasterizationStateCreateInfo.flags = 0;
    // NOTE: If the depth clamping feature is not enabled, depthClampEnable must be VK_FALSE.
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCreateInfo.frontFace = VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = NULL;
    multisampleStateCreateInfo.flags = 0;
    multisampleStateCreateInfo.rasterizationSamples =        (VkSampleCountFlagBits)VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.minSampleShading = 1.0f;
    multisampleStateCreateInfo.pSampleMask = NULL;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.pNext = NULL;
    depthStencilStateCreateInfo.flags = 0;
    depthStencilStateCreateInfo.depthTestEnable =  VK_FALSE;
    depthStencilStateCreateInfo.depthWriteEnable =  VK_FALSE;
    depthStencilStateCreateInfo.depthCompareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.front.failOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.passOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilStateCreateInfo.minDepthBounds = 0.0f;
    depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachementState[1];
    colorBlendAttachementState[0].blendEnable =  VK_FALSE;
    colorBlendAttachementState[0].srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE;
    colorBlendAttachementState[0].dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE;
    colorBlendAttachementState[0].colorBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
    colorBlendAttachementState[0].srcAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE;
    colorBlendAttachementState[0].dstAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE;
    colorBlendAttachementState[0].alphaBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
    colorBlendAttachementState[0].colorWriteMask =
        ( VK_COLOR_COMPONENT_R_BIT ) |
        (VK_COLOR_COMPONENT_G_BIT ) |
        ( VK_COLOR_COMPONENT_B_BIT) |
        ( VK_COLOR_COMPONENT_A_BIT);

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = NULL;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_CLEAR;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = colorBlendAttachementState;
    colorBlendStateCreateInfo.blendConstants[0] = 1.0f;
    colorBlendStateCreateInfo.blendConstants[1] = 1.0f;
    colorBlendStateCreateInfo.blendConstants[2] = 1.0f;
    colorBlendStateCreateInfo.blendConstants[3] = 1.0f;

    VkDynamicState dynamicStateEnables[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo;
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.pNext = NULL;
    pipelineDynamicStateCreateInfo.flags = 0;
		VkDynamicState dynamicStates[2]							= { VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT, VkDynamicState::VK_DYNAMIC_STATE_SCISSOR };
    pipelineDynamicStateCreateInfo.dynamicStateCount = 2;
    pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates;
	
	// create some fake shaders:
	
	#define GLSL_VERSION "440 core" // maintain precision decorations: "310 es"
	#define GLSL_EXTENSIONS                             \
		"#extension GL_EXT_shader_io_blocks : enable\n" \
		"#extension GL_ARB_enhanced_layouts : enable\n"
	
	static const unsigned int colorOnlyVertexProgramSPIRV[] = {
		// 7.11.3057
		0x07230203,0x00010000,0x00080007,0x0000002e,0x00000000,0x00020011,0x00000001,0x0006000b,
		0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
		0x000a000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000a,0x00000018,0x0000001c,
		0x0000002a,0x0000002c,0x00030003,0x00000002,0x000001b8,0x00070004,0x415f4c47,0x655f4252,
		0x6e61686e,0x5f646563,0x6f79616c,0x00737475,0x00070004,0x455f4c47,0x735f5458,0x65646168,
		0x6f695f72,0x6f6c625f,0x00736b63,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00060005,
		0x00000008,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000008,0x00000000,
		0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x0000000a,0x00000000,0x00060005,0x0000000e,
		0x6e656353,0x74614d65,0x65636972,0x00000073,0x00060006,0x0000000e,0x00000000,0x77656956,
		0x7274614d,0x00007869,0x00080006,0x0000000e,0x00000001,0x6a6f7250,0x69746365,0x614d6e6f,
		0x78697274,0x00000000,0x00030005,0x00000010,0x00000000,0x00060005,0x00000018,0x74726576,
		0x72547865,0x66736e61,0x006d726f,0x00060005,0x0000001c,0x74726576,0x6f507865,0x69746973,
		0x00006e6f,0x00060005,0x0000002a,0x67617266,0x746e656d,0x6f6c6f43,0x00000072,0x00050005,
		0x0000002c,0x74726576,0x6f437865,0x00726f6c,0x00050048,0x00000008,0x00000000,0x0000000b,
		0x00000000,0x00030047,0x00000008,0x00000002,0x00040048,0x0000000e,0x00000000,0x00000005,
		0x00050048,0x0000000e,0x00000000,0x00000023,0x00000000,0x00050048,0x0000000e,0x00000000,
		0x00000007,0x00000010,0x00040048,0x0000000e,0x00000001,0x00000005,0x00050048,0x0000000e,
		0x00000001,0x00000023,0x00000040,0x00050048,0x0000000e,0x00000001,0x00000007,0x00000010,
		0x00030047,0x0000000e,0x00000002,0x00040047,0x00000010,0x00000022,0x00000000,0x00040047,
		0x00000010,0x00000021,0x00000000,0x00040047,0x00000018,0x0000001e,0x00000002,0x00040047,
		0x0000001c,0x0000001e,0x00000000,0x00030047,0x0000002a,0x00000000,0x00040047,0x0000002a,
		0x0000001e,0x00000000,0x00040047,0x0000002c,0x0000001e,0x00000001,0x00020013,0x00000002,
		0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,
		0x00000006,0x00000004,0x0003001e,0x00000008,0x00000007,0x00040020,0x00000009,0x00000003,
		0x00000008,0x0004003b,0x00000009,0x0000000a,0x00000003,0x00040015,0x0000000b,0x00000020,
		0x00000001,0x0004002b,0x0000000b,0x0000000c,0x00000000,0x00040018,0x0000000d,0x00000007,
		0x00000004,0x0004001e,0x0000000e,0x0000000d,0x0000000d,0x00040020,0x0000000f,0x00000002,
		0x0000000e,0x0004003b,0x0000000f,0x00000010,0x00000002,0x0004002b,0x0000000b,0x00000011,
		0x00000001,0x00040020,0x00000012,0x00000002,0x0000000d,0x00040020,0x00000017,0x00000001,
		0x0000000d,0x0004003b,0x00000017,0x00000018,0x00000001,0x00040017,0x0000001a,0x00000006,
		0x00000003,0x00040020,0x0000001b,0x00000001,0x0000001a,0x0004003b,0x0000001b,0x0000001c,
		0x00000001,0x0004002b,0x00000006,0x0000001e,0x3dcccccd,0x0004002b,0x00000006,0x00000020,
		0x3f800000,0x00040020,0x00000028,0x00000003,0x00000007,0x0004003b,0x00000028,0x0000002a,
		0x00000003,0x00040020,0x0000002b,0x00000001,0x00000007,0x0004003b,0x0000002b,0x0000002c,
		0x00000001,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,
		0x00050041,0x00000012,0x00000013,0x00000010,0x00000011,0x0004003d,0x0000000d,0x00000014,
		0x00000013,0x00050041,0x00000012,0x00000015,0x00000010,0x0000000c,0x0004003d,0x0000000d,
		0x00000016,0x00000015,0x0004003d,0x0000000d,0x00000019,0x00000018,0x0004003d,0x0000001a,
		0x0000001d,0x0000001c,0x0005008e,0x0000001a,0x0000001f,0x0000001d,0x0000001e,0x00050051,
		0x00000006,0x00000021,0x0000001f,0x00000000,0x00050051,0x00000006,0x00000022,0x0000001f,
		0x00000001,0x00050051,0x00000006,0x00000023,0x0000001f,0x00000002,0x00070050,0x00000007,
		0x00000024,0x00000021,0x00000022,0x00000023,0x00000020,0x00050091,0x00000007,0x00000025,
		0x00000019,0x00000024,0x00050091,0x00000007,0x00000026,0x00000016,0x00000025,0x00050091,
		0x00000007,0x00000027,0x00000014,0x00000026,0x00050041,0x00000028,0x00000029,0x0000000a,
		0x0000000c,0x0003003e,0x00000029,0x00000027,0x0004003d,0x00000007,0x0000002d,0x0000002c,
		0x0003003e,0x0000002a,0x0000002d,0x000100fd,0x00010038
	};
	static const unsigned int ColourOnlyVertexProgramSPIRV[] = {
		0x07230203,0x00010000,0x0008000A,0x00000075,0x00000000,0x00020011,0x00000001,0x0006000B,
		0x00000001,0x4C534C47,0x6474732E,0x3035342E,0x00000000,0x0003000E,0x00000000,0x00000001,
		0x000B000F,0x00000000,0x00000004,0x6E69616D,0x00000000,0x0000003C,0x00000045,0x0000004A,
		0x00000070,0x00000072,0x00000074,0x00030003,0x00000002,0x000001B8,0x00070004,0x415F4C47,
		0x655F4252,0x6E61686E,0x5F646563,0x6F79616C,0x00737475,0x000B0004,0x455F4C47,0x735F5458,
		0x6C706D61,0x656C7265,0x745F7373,0x75747865,0x665F6572,0x74636E75,0x736E6F69,0x00000000,
		0x00070004,0x455F4C47,0x735F5458,0x65646168,0x6F695F72,0x6F6C625F,0x00736B63,0x000A0004,
		0x475F4C47,0x4C474F4F,0x70635F45,0x74735F70,0x5F656C79,0x656E696C,0x7269645F,0x69746365,
		0x00006576,0x00040005,0x00000004,0x6E69616D,0x00000000,0x00040005,0x00000007,0x6E4F6469,
		0x0000796C,0x00060006,0x00000007,0x00000000,0x74726576,0x695F7865,0x00000064,0x00060005,
		0x0000000B,0x56736F70,0x65747265,0x74754F78,0x00747570,0x00060006,0x0000000B,0x00000000,
		0x736F5068,0x6F697469,0x0000006E,0x000D0005,0x0000000E,0x72746C55,0x6D695361,0x46656C70,
		0x736C6C75,0x65657263,0x7473286E,0x74637572,0x4F64692D,0x2D796C6E,0x3B313175,0x00000000,
		0x00030005,0x0000000D,0x00004E49,0x00040005,0x00000014,0x73736F70,0x00000000,0x00030005,
		0x00000025,0x00736F70,0x00030005,0x0000002C,0x0054554F,0x00030005,0x0000003A,0x00004E49,
		0x00060005,0x0000003C,0x565F6C67,0x65747265,0x646E4978,0x00007865,0x00030005,0x00000040,
		0x00007470,0x00040005,0x00000041,0x61726170,0x0000006D,0x00050005,0x00000045,0x736F5068,
		0x6F697469,0x0000006E,0x00060005,0x00000048,0x505F6C67,0x65567265,0x78657472,0x00000000,
		0x00060006,0x00000048,0x00000000,0x505F6C67,0x7469736F,0x006E6F69,0x00030005,0x0000004A,
		0x00000000,0x00060005,0x0000005B,0x53636377,0x6C706D61,0x74537265,0x00657461,0x00060005,
		0x0000005C,0x53636D77,0x6C706D61,0x74537265,0x00657461,0x00080005,0x0000005D,0x70617277,
		0x7272694D,0x6153726F,0x656C706D,0x61745372,0x00006574,0x00060005,0x0000005E,0x53636D63,
		0x6C706D61,0x74537265,0x00657461,0x00070005,0x0000005F,0x70617277,0x706D6153,0x5372656C,
		0x65746174,0x00000000,0x00060005,0x00000060,0x53637777,0x6C706D61,0x74537265,0x00657461,
		0x00060005,0x00000061,0x53637763,0x6C706D61,0x74537265,0x00657461,0x00070005,0x00000062,
		0x6D616C63,0x6D615370,0x72656C70,0x74617453,0x00000065,0x00080005,0x00000063,0x70617277,
		0x6D616C43,0x6D615370,0x72656C70,0x74617453,0x00000065,0x00070005,0x00000064,0x706D6173,
		0x5372656C,0x65746174,0x7261654E,0x00747365,0x00080005,0x00000065,0x4E637777,0x65726165,
		0x61537473,0x656C706D,0x61745372,0x00006574,0x00080005,0x00000066,0x4E636D63,0x65726165,
		0x61537473,0x656C706D,0x61745372,0x00006574,0x00080005,0x00000067,0x706D6173,0x5372656C,
		0x65746174,0x7261654E,0x57747365,0x00706172,0x00090005,0x00000068,0x706D6173,0x5372656C,
		0x65746174,0x7261654E,0x43747365,0x706D616C,0x00000000,0x00070005,0x00000069,0x65627563,
		0x706D6153,0x5372656C,0x65746174,0x00000000,0x00060005,0x0000006B,0x6E656353,0x74614D65,
		0x65636972,0x00000073,0x00060006,0x0000006B,0x00000000,0x77656956,0x7274614D,0x00007869,
		0x00080006,0x0000006B,0x00000001,0x6A6F7250,0x69746365,0x614D6E6F,0x78697274,0x00000000,
		0x00030005,0x0000006D,0x00000000,0x00060005,0x00000070,0x74726576,0x6F507865,0x69746973,
		0x00006E6F,0x00050005,0x00000072,0x74726576,0x6F437865,0x00726F6C,0x00060005,0x00000074,
		0x74726576,0x72547865,0x66736E61,0x006D726F,0x00040047,0x0000003C,0x0000000B,0x0000002A,
		0x00040047,0x00000045,0x0000001E,0x00000001,0x00050048,0x00000048,0x00000000,0x0000000B,
		0x00000000,0x00030047,0x00000048,0x00000002,0x00040047,0x0000005B,0x00000022,0x00000000,
		0x00040047,0x0000005B,0x00000021,0x0000012D,0x00040047,0x0000005C,0x00000022,0x00000000,
		0x00040047,0x0000005C,0x00000021,0x0000012E,0x00040047,0x0000005D,0x00000022,0x00000000,
		0x00040047,0x0000005D,0x00000021,0x0000012F,0x00040047,0x0000005E,0x00000022,0x00000000,
		0x00040047,0x0000005E,0x00000021,0x00000131,0x00040047,0x0000005F,0x00000022,0x00000000,
		0x00040047,0x0000005F,0x00000021,0x00000132,0x00040047,0x00000060,0x00000022,0x00000000,
		0x00040047,0x00000060,0x00000021,0x00000133,0x00040047,0x00000061,0x00000022,0x00000000,
		0x00040047,0x00000061,0x00000021,0x00000134,0x00040047,0x00000062,0x00000022,0x00000000,
		0x00040047,0x00000062,0x00000021,0x00000135,0x00040047,0x00000063,0x00000022,0x00000000,
		0x00040047,0x00000063,0x00000021,0x00000136,0x00040047,0x00000064,0x00000022,0x00000000,
		0x00040047,0x00000064,0x00000021,0x00000137,0x00040047,0x00000065,0x00000022,0x00000000,
		0x00040047,0x00000065,0x00000021,0x00000138,0x00040047,0x00000066,0x00000022,0x00000000,
		0x00040047,0x00000066,0x00000021,0x00000139,0x00040047,0x00000067,0x00000022,0x00000000,
		0x00040047,0x00000067,0x00000021,0x0000013A,0x00040047,0x00000068,0x00000022,0x00000000,
		0x00040047,0x00000068,0x00000021,0x0000013B,0x00040047,0x00000069,0x00000022,0x00000000,
		0x00040047,0x00000069,0x00000021,0x00000130,0x00040048,0x0000006B,0x00000000,0x00000005,
		0x00050048,0x0000006B,0x00000000,0x00000023,0x00000000,0x00050048,0x0000006B,0x00000000,
		0x00000007,0x00000010,0x00040048,0x0000006B,0x00000001,0x00000005,0x00050048,0x0000006B,
		0x00000001,0x00000023,0x00000040,0x00050048,0x0000006B,0x00000001,0x00000007,0x00000010,
		0x00030047,0x0000006B,0x00000002,0x00040047,0x0000006D,0x00000022,0x00000000,0x00040047,
		0x0000006D,0x00000021,0x00000000,0x00040047,0x00000070,0x0000001E,0x00000000,0x00040047,
		0x00000072,0x0000001E,0x00000001,0x00040047,0x00000074,0x0000001E,0x00000002,0x00020013,
		0x00000002,0x00030021,0x00000003,0x00000002,0x00040015,0x00000006,0x00000020,0x00000000,
		0x0003001E,0x00000007,0x00000006,0x00040020,0x00000008,0x00000007,0x00000007,0x00030016,
		0x00000009,0x00000020,0x00040017,0x0000000A,0x00000009,0x00000004,0x0003001E,0x0000000B,
		0x0000000A,0x00040021,0x0000000C,0x0000000B,0x00000008,0x00040017,0x00000010,0x00000009,
		0x00000002,0x0004002B,0x00000006,0x00000011,0x00000004,0x0004001C,0x00000012,0x00000010,
		0x00000011,0x00040020,0x00000013,0x00000007,0x00000012,0x00040015,0x00000015,0x00000020,
		0x00000001,0x0004002B,0x00000015,0x00000016,0x00000000,0x0004002B,0x00000009,0x00000017,
		0x3F800000,0x0004002B,0x00000009,0x00000018,0xBF800000,0x0005002C,0x00000010,0x00000019,
		0x00000017,0x00000018,0x00040020,0x0000001A,0x00000007,0x00000010,0x0004002B,0x00000015,
		0x0000001C,0x00000001,0x0005002C,0x00000010,0x0000001D,0x00000017,0x00000017,0x0004002B,
		0x00000015,0x0000001F,0x00000002,0x0005002C,0x00000010,0x00000020,0x00000018,0x00000018,
		0x0004002B,0x00000015,0x00000022,0x00000003,0x0005002C,0x00000010,0x00000023,0x00000018,
		0x00000017,0x00040020,0x00000026,0x00000007,0x00000006,0x00040020,0x0000002B,0x00000007,
		0x0000000B,0x0004002B,0x00000009,0x0000002E,0x00000000,0x00040020,0x00000032,0x00000007,
		0x0000000A,0x0004002B,0x00000006,0x00000034,0x00000002,0x00040020,0x00000035,0x00000007,
		0x00000009,0x00040020,0x0000003B,0x00000001,0x00000015,0x0004003B,0x0000003B,0x0000003C,
		0x00000001,0x00040020,0x00000044,0x00000003,0x0000000A,0x0004003B,0x00000044,0x00000045,
		0x00000003,0x0003001E,0x00000048,0x0000000A,0x00040020,0x00000049,0x00000003,0x00000048,
		0x0004003B,0x00000049,0x0000004A,0x00000003,0x0004002B,0x00000006,0x0000004B,0x00000000,
		0x0004002B,0x00000006,0x0000004E,0x00000001,0x0004002B,0x00000006,0x00000054,0x00000003,
		0x0002001A,0x00000059,0x00040020,0x0000005A,0x00000000,0x00000059,0x0004003B,0x0000005A,
		0x0000005B,0x00000000,0x0004003B,0x0000005A,0x0000005C,0x00000000,0x0004003B,0x0000005A,
		0x0000005D,0x00000000,0x0004003B,0x0000005A,0x0000005E,0x00000000,0x0004003B,0x0000005A,
		0x0000005F,0x00000000,0x0004003B,0x0000005A,0x00000060,0x00000000,0x0004003B,0x0000005A,
		0x00000061,0x00000000,0x0004003B,0x0000005A,0x00000062,0x00000000,0x0004003B,0x0000005A,
		0x00000063,0x00000000,0x0004003B,0x0000005A,0x00000064,0x00000000,0x0004003B,0x0000005A,
		0x00000065,0x00000000,0x0004003B,0x0000005A,0x00000066,0x00000000,0x0004003B,0x0000005A,
		0x00000067,0x00000000,0x0004003B,0x0000005A,0x00000068,0x00000000,0x0004003B,0x0000005A,
		0x00000069,0x00000000,0x00040018,0x0000006A,0x0000000A,0x00000004,0x0004001E,0x0000006B,
		0x0000006A,0x0000006A,0x00040020,0x0000006C,0x00000002,0x0000006B,0x0004003B,0x0000006C,
		0x0000006D,0x00000002,0x00040017,0x0000006E,0x00000009,0x00000003,0x00040020,0x0000006F,
		0x00000001,0x0000006E,0x0004003B,0x0000006F,0x00000070,0x00000001,0x00040020,0x00000071,
		0x00000001,0x0000000A,0x0004003B,0x00000071,0x00000072,0x00000001,0x00040020,0x00000073,
		0x00000001,0x0000006A,0x0004003B,0x00000073,0x00000074,0x00000001,0x00050036,0x00000002,
		0x00000004,0x00000000,0x00000003,0x000200F8,0x00000005,0x0004003B,0x00000008,0x0000003A,
		0x00000007,0x0004003B,0x0000002B,0x00000040,0x00000007,0x0004003B,0x00000008,0x00000041,
		0x00000007,0x0004003D,0x00000015,0x0000003D,0x0000003C,0x0004007C,0x00000006,0x0000003E,
		0x0000003D,0x00050041,0x00000026,0x0000003F,0x0000003A,0x00000016,0x0003003E,0x0000003F,
		0x0000003E,0x0004003D,0x00000007,0x00000042,0x0000003A,0x0003003E,0x00000041,0x00000042,
		0x00050039,0x0000000B,0x00000043,0x0000000E,0x00000041,0x0003003E,0x00000040,0x00000043,
		0x00050041,0x00000032,0x00000046,0x00000040,0x00000016,0x0004003D,0x0000000A,0x00000047,
		0x00000046,0x0003003E,0x00000045,0x00000047,0x00060041,0x00000035,0x0000004C,0x00000040,
		0x00000016,0x0000004B,0x0004003D,0x00000009,0x0000004D,0x0000004C,0x00060041,0x00000035,
		0x0000004F,0x00000040,0x00000016,0x0000004E,0x0004003D,0x00000009,0x00000050,0x0000004F,
		0x0004007F,0x00000009,0x00000051,0x00000050,0x00060041,0x00000035,0x00000052,0x00000040,
		0x00000016,0x00000034,0x0004003D,0x00000009,0x00000053,0x00000052,0x00060041,0x00000035,
		0x00000055,0x00000040,0x00000016,0x00000054,0x0004003D,0x00000009,0x00000056,0x00000055,
		0x00070050,0x0000000A,0x00000057,0x0000004D,0x00000051,0x00000053,0x00000056,0x00050041,
		0x00000044,0x00000058,0x0000004A,0x00000016,0x0003003E,0x00000058,0x00000057,0x000100FD,
		0x00010038,0x00050036,0x0000000B,0x0000000E,0x00000000,0x0000000C,0x00030037,0x00000008,
		0x0000000D,0x000200F8,0x0000000F,0x0004003B,0x00000013,0x00000014,0x00000007,0x0004003B,
		0x0000001A,0x00000025,0x00000007,0x0004003B,0x0000002B,0x0000002C,0x00000007,0x00050041,
		0x0000001A,0x0000001B,0x00000014,0x00000016,0x0003003E,0x0000001B,0x00000019,0x00050041,
		0x0000001A,0x0000001E,0x00000014,0x0000001C,0x0003003E,0x0000001E,0x0000001D,0x00050041,
		0x0000001A,0x00000021,0x00000014,0x0000001F,0x0003003E,0x00000021,0x00000020,0x00050041,
		0x0000001A,0x00000024,0x00000014,0x00000022,0x0003003E,0x00000024,0x00000023,0x00050041,
		0x00000026,0x00000027,0x0000000D,0x00000016,0x0004003D,0x00000006,0x00000028,0x00000027,
		0x00050041,0x0000001A,0x00000029,0x00000014,0x00000028,0x0004003D,0x00000010,0x0000002A,
		0x00000029,0x0003003E,0x00000025,0x0000002A,0x0004003D,0x00000010,0x0000002D,0x00000025,
		0x00050051,0x00000009,0x0000002F,0x0000002D,0x00000000,0x00050051,0x00000009,0x00000030,
		0x0000002D,0x00000001,0x00070050,0x0000000A,0x00000031,0x0000002F,0x00000030,0x0000002E,
		0x00000017,0x00050041,0x00000032,0x00000033,0x0000002C,0x00000016,0x0003003E,0x00000033,
		0x00000031,0x00060041,0x00000035,0x00000036,0x0000002C,0x00000016,0x00000034,0x0003003E,
		0x00000036,0x0000002E,0x0004003D,0x0000000B,0x00000037,0x0000002C,0x000200FE,0x00000037,
		0x00010038,};
static const char colorOnlyVertexProgramGLSL[] =
    "#version " GLSL_VERSION "\n"
    GLSL_EXTENSIONS
    "layout( std140, binding = 0 ) uniform SceneMatrices\n"
    "{\n"
    "	layout( offset =   0 ) mat4 ViewMatrix;\n"
    "	layout( offset =  64 ) mat4 ProjectionMatrix;\n"
    "};\n"
    "layout( location = 0 ) in vec3 vertexPosition;\n"
    "layout( location = 1 ) in vec4 vertexColor;\n"
    "layout( location = 2 ) in mat4 vertexTransform;\n"
    "layout( location = 0 ) out lowp vec4 fragmentColor;\n"
    "out gl_PerVertex { vec4 gl_Position; };\n"
    "void main( void )\n"
    "{\n"
    "	gl_Position = ProjectionMatrix * ( ViewMatrix * vec4( 1.0,1.0,0.1, 1.0 ) );\n"
    "	fragmentColor = vec4(1.0,0,0,0);\n"
    "}\n";

	static const unsigned int colorOnlyFragmentProgramSPIRV[] = {
		// 7.11.3057
		0x07230203,0x00010000,0x00080007,0x0000000d,0x00000000,0x00020011,0x00000001,0x0006000b,
		0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
		0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x00030010,
		0x00000004,0x00000007,0x00030003,0x00000002,0x000001b8,0x00070004,0x415f4c47,0x655f4252,
		0x6e61686e,0x5f646563,0x6f79616c,0x00737475,0x00070004,0x455f4c47,0x735f5458,0x65646168,
		0x6f695f72,0x6f6c625f,0x00736b63,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,
		0x00000009,0x4374756f,0x726f6c6f,0x00000000,0x00060005,0x0000000b,0x67617266,0x746e656d,
		0x6f6c6f43,0x00000072,0x00030047,0x00000009,0x00000000,0x00040047,0x00000009,0x0000001e,
		0x00000000,0x00030047,0x0000000b,0x00000000,0x00040047,0x0000000b,0x0000001e,0x00000000,
		0x00030047,0x0000000c,0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
		0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,
		0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040020,
		0x0000000a,0x00000001,0x00000007,0x0004003b,0x0000000a,0x0000000b,0x00000001,0x00050036,
		0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000007,
		0x0000000c,0x0000000b,0x0003003e,0x00000009,0x0000000c,0x000100fd,0x00010038
	};
	static const unsigned int ColourOnlyFragmentProgramSPIRV[] = {
		0x07230203,0x00010000,0x0008000A,0x00000023,0x00000000,0x00020011,0x00000001,0x0006000B,
		0x00000001,0x4C534C47,0x6474732E,0x3035342E,0x00000000,0x0003000E,0x00000000,0x00000001,
		0x0007000F,0x00000004,0x00000004,0x6E69616D,0x00000000,0x0000000E,0x00000022,0x00030010,
		0x00000004,0x00000007,0x00030003,0x00000002,0x000001C2,0x00070004,0x415F4C47,0x655F4252,
		0x6E61686E,0x5F646563,0x6F79616C,0x00737475,0x000B0004,0x455F4C47,0x735F5458,0x6C706D61,
		0x656C7265,0x745F7373,0x75747865,0x665F6572,0x74636E75,0x736E6F69,0x00000000,0x00070004,
		0x455F4C47,0x735F5458,0x65646168,0x6F695F72,0x6F6C625F,0x00736B63,0x000A0004,0x475F4C47,
		0x4C474F4F,0x70635F45,0x74735F70,0x5F656C79,0x656E696C,0x7269645F,0x69746365,0x00006576,
		0x00040005,0x00000004,0x6E69616D,0x00000000,0x00030005,0x00000009,0x00736572,0x00050005,
		0x0000000E,0x4374756F,0x726F6C6F,0x00000000,0x00060005,0x00000012,0x53636377,0x6C706D61,
		0x74537265,0x00657461,0x00060005,0x00000013,0x53636D77,0x6C706D61,0x74537265,0x00657461,
		0x00080005,0x00000014,0x70617277,0x7272694D,0x6153726F,0x656C706D,0x61745372,0x00006574,
		0x00060005,0x00000015,0x53636D63,0x6C706D61,0x74537265,0x00657461,0x00070005,0x00000016,
		0x70617277,0x706D6153,0x5372656C,0x65746174,0x00000000,0x00060005,0x00000017,0x53637777,
		0x6C706D61,0x74537265,0x00657461,0x00060005,0x00000018,0x53637763,0x6C706D61,0x74537265,
		0x00657461,0x00070005,0x00000019,0x6D616C63,0x6D615370,0x72656C70,0x74617453,0x00000065,
		0x00080005,0x0000001A,0x70617277,0x6D616C43,0x6D615370,0x72656C70,0x74617453,0x00000065,
		0x00070005,0x0000001B,0x706D6173,0x5372656C,0x65746174,0x7261654E,0x00747365,0x00080005,
		0x0000001C,0x4E637777,0x65726165,0x61537473,0x656C706D,0x61745372,0x00006574,0x00080005,
		0x0000001D,0x4E636D63,0x65726165,0x61537473,0x656C706D,0x61745372,0x00006574,0x00080005,
		0x0000001E,0x706D6173,0x5372656C,0x65746174,0x7261654E,0x57747365,0x00706172,0x00090005,
		0x0000001F,0x706D6173,0x5372656C,0x65746174,0x7261654E,0x43747365,0x706D616C,0x00000000,
		0x00070005,0x00000020,0x65627563,0x706D6153,0x5372656C,0x65746174,0x00000000,0x00050005,
		0x00000022,0x736F5068,0x6F697469,0x0000006E,0x00030047,0x0000000E,0x00000000,0x00040047,
		0x0000000E,0x0000001E,0x00000000,0x00040047,0x00000012,0x00000022,0x00000000,0x00040047,
		0x00000012,0x00000021,0x0000012D,0x00040047,0x00000013,0x00000022,0x00000000,0x00040047,
		0x00000013,0x00000021,0x0000012E,0x00040047,0x00000014,0x00000022,0x00000000,0x00040047,
		0x00000014,0x00000021,0x0000012F,0x00040047,0x00000015,0x00000022,0x00000000,0x00040047,
		0x00000015,0x00000021,0x00000131,0x00040047,0x00000016,0x00000022,0x00000000,0x00040047,
		0x00000016,0x00000021,0x00000132,0x00040047,0x00000017,0x00000022,0x00000000,0x00040047,
		0x00000017,0x00000021,0x00000133,0x00040047,0x00000018,0x00000022,0x00000000,0x00040047,
		0x00000018,0x00000021,0x00000134,0x00040047,0x00000019,0x00000022,0x00000000,0x00040047,
		0x00000019,0x00000021,0x00000135,0x00040047,0x0000001A,0x00000022,0x00000000,0x00040047,
		0x0000001A,0x00000021,0x00000136,0x00040047,0x0000001B,0x00000022,0x00000000,0x00040047,
		0x0000001B,0x00000021,0x00000137,0x00040047,0x0000001C,0x00000022,0x00000000,0x00040047,
		0x0000001C,0x00000021,0x00000138,0x00040047,0x0000001D,0x00000022,0x00000000,0x00040047,
		0x0000001D,0x00000021,0x00000139,0x00040047,0x0000001E,0x00000022,0x00000000,0x00040047,
		0x0000001E,0x00000021,0x0000013A,0x00040047,0x0000001F,0x00000022,0x00000000,0x00040047,
		0x0000001F,0x00000021,0x0000013B,0x00040047,0x00000020,0x00000022,0x00000000,0x00040047,
		0x00000020,0x00000021,0x00000130,0x00040047,0x00000022,0x0000001E,0x00000001,0x00020013,
		0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,
		0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000007,0x00000007,0x0004002B,
		0x00000006,0x0000000A,0x3F800000,0x0004002B,0x00000006,0x0000000B,0x00000000,0x0007002C,
		0x00000007,0x0000000C,0x0000000A,0x0000000B,0x0000000B,0x0000000B,0x00040020,0x0000000D,
		0x00000003,0x00000007,0x0004003B,0x0000000D,0x0000000E,0x00000003,0x0002001A,0x00000010,
		0x00040020,0x00000011,0x00000000,0x00000010,0x0004003B,0x00000011,0x00000012,0x00000000,
		0x0004003B,0x00000011,0x00000013,0x00000000,0x0004003B,0x00000011,0x00000014,0x00000000,
		0x0004003B,0x00000011,0x00000015,0x00000000,0x0004003B,0x00000011,0x00000016,0x00000000,
		0x0004003B,0x00000011,0x00000017,0x00000000,0x0004003B,0x00000011,0x00000018,0x00000000,
		0x0004003B,0x00000011,0x00000019,0x00000000,0x0004003B,0x00000011,0x0000001A,0x00000000,
		0x0004003B,0x00000011,0x0000001B,0x00000000,0x0004003B,0x00000011,0x0000001C,0x00000000,
		0x0004003B,0x00000011,0x0000001D,0x00000000,0x0004003B,0x00000011,0x0000001E,0x00000000,
		0x0004003B,0x00000011,0x0000001F,0x00000000,0x0004003B,0x00000011,0x00000020,0x00000000,
		0x00040020,0x00000021,0x00000001,0x00000007,0x0004003B,0x00000021,0x00000022,0x00000001,
		0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200F8,0x00000005,0x0004003B,
		0x00000008,0x00000009,0x00000007,0x0003003E,0x00000009,0x0000000C,0x0004003D,0x00000007,
		0x0000000F,0x00000009,0x0003003E,0x0000000E,0x0000000F,0x000100FD,0x00010038,};
static const char colorOnlyFragmentProgramGLSL[] =
    "#version " GLSL_VERSION "\n"
    GLSL_EXTENSIONS
    "layout( location = 0 ) in lowp vec4 fragmentColor;\n"
    "layout( location = 0 ) out lowp vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "	outColor = fragmentColor;\n"
    "}\n";
	ovrVkGraphicsProgram program;
	
	enum {
		PROGRAM_UNIFORM_SCENE_MATRICES,
	};

	static ovrVkProgramParm colorOnlyProgramParms[] = {
		{OVR_PROGRAM_STAGE_FLAG_VERTEX,
		 OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM,
		 OVR_PROGRAM_PARM_ACCESS_READ_ONLY,
		 PROGRAM_UNIFORM_SCENE_MATRICES,
		 "SceneMatrices",
		 0},
	};
    ovrVkGraphicsProgram_Create(
        *device,
        &program,
         ColourOnlyVertexProgramSPIRV,
         sizeof(ColourOnlyVertexProgramSPIRV),
        ColourOnlyFragmentProgramSPIRV,
        sizeof(ColourOnlyFragmentProgramSPIRV),
        colorOnlyProgramParms,
        ARRAY_SIZE(colorOnlyProgramParms),
        DefaultVertexAttributeLayout,
        OVR_VERTEX_ATTRIBUTE_FLAG_POSITION | OVR_VERTEX_ATTRIBUTE_FLAG_COLOR |
            OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM);
		
	VkPipelineShaderStageCreateInfo shaderStageInfo[2];
	shaderStageInfo[0]= {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,program.vertexShaderModule,"main",nullptr};
	//shaderStageInfo[0]= {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,v->mShader,v->entryPoint.c_str(),nullptr};
	shaderStageInfo[1]= {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,program.fragmentShaderModule,"main",nullptr};
	
	VkPipelineVertexInputStateCreateInfo vertexInputInfo={};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = NULL;
	vertexInputInfo.flags = 0;
	VkVertexInputAttributeDescription vertexInputAttributeDescriptions[6];
	VkVertexInputBindingDescription vertexBindingDescriptions[3];
	
	vertexInputAttributeDescriptions[0].location=0;
	vertexInputAttributeDescriptions[0].binding	=0;
	vertexInputAttributeDescriptions[0].format	=VK_FORMAT_R32G32B32_SFLOAT;
	vertexInputAttributeDescriptions[0].offset	=0;
	vertexInputAttributeDescriptions[1].location =  1;
	vertexInputAttributeDescriptions[1].binding =  1;
	vertexInputAttributeDescriptions[1].format =  VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexInputAttributeDescriptions[1].offset =  0;
	vertexInputAttributeDescriptions[2].location =  2;
	vertexInputAttributeDescriptions[2].binding =  2;
	vertexInputAttributeDescriptions[2].format =  VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexInputAttributeDescriptions[2].offset =  0;
	vertexInputAttributeDescriptions[3].location =  3;
	vertexInputAttributeDescriptions[3].binding =  2;
	vertexInputAttributeDescriptions[3].format =  VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexInputAttributeDescriptions[3].offset =  16;
	vertexInputAttributeDescriptions[4].location =  4;
	vertexInputAttributeDescriptions[4].binding =  2;
	vertexInputAttributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexInputAttributeDescriptions[4].offset =  32;
	vertexInputAttributeDescriptions[5].location =  5;
	vertexInputAttributeDescriptions[5].binding =  2;
	vertexInputAttributeDescriptions[5].format =  VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexInputAttributeDescriptions[5].offset =  48;
	vertexBindingDescriptions[0].binding =  0;
	vertexBindingDescriptions[0].stride =  12;
	vertexBindingDescriptions[0].inputRate =  VK_VERTEX_INPUT_RATE_VERTEX;
	vertexBindingDescriptions[1].binding =  1;
	vertexBindingDescriptions[1].stride =  16;
	vertexBindingDescriptions[1].inputRate =  VK_VERTEX_INPUT_RATE_VERTEX;
	vertexBindingDescriptions[2].binding =  2;
	vertexBindingDescriptions[2].stride =  64;
	vertexBindingDescriptions[2].inputRate =  VK_VERTEX_INPUT_RATE_INSTANCE;

	vertexInputInfo.pVertexAttributeDescriptions=vertexInputAttributeDescriptions;
	vertexInputInfo.pVertexBindingDescriptions=vertexBindingDescriptions;
	vertexInputInfo.vertexAttributeDescriptionCount=6;
	vertexInputInfo.vertexBindingDescriptionCount=3;
	
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.pNext = NULL;
    inputAssemblyInfo.flags = 0;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.pNext = NULL;
    graphicsPipelineCreateInfo.flags = 0;
    graphicsPipelineCreateInfo.stageCount = 2;
    graphicsPipelineCreateInfo.pStages = shaderStageInfo;
    graphicsPipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
    graphicsPipelineCreateInfo.pTessellationState = &tessellationStateCreateInfo;
    graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState =NULL;
    graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
    graphicsPipelineCreateInfo.layout =program.parmLayout.pipelineLayout;
    graphicsPipelineCreateInfo.renderPass = (rp&&(*rp))?*rp:renderPassPipeline->mRenderPass;
    graphicsPipelineCreateInfo.subpass = 0;
    graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    graphicsPipelineCreateInfo.basePipelineIndex = 0;
	
	VkPipeline				mPipeline;
    VkResult res=vkCreateGraphicsPipelines(
		*device,
		renderPassPipeline->mPipelineCache,
		1,
		&graphicsPipelineCreateInfo,
		VK_ALLOCATOR,
		&mPipeline);
#endif
}

void EffectPass::InitializePipeline(crossplatform::DeviceContext &deviceContext,RenderPassPipeline *renderPassPipeline,crossplatform::PixelFormat pixelFormat
	,crossplatform::Topology topology, const crossplatform::RenderState *blendState
	,const crossplatform::RenderState *depthStencilState
	,const crossplatform::RenderState *rasterizerState)
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vk::PipelineCacheCreateInfo pipelineCacheInfo;
	vk::Result result = vulkanDevice->createPipelineCache(&pipelineCacheInfo, nullptr, &renderPassPipeline->mPipelineCache);
	SIMUL_VK_CHECK(result);
	SetVulkanName(renderPlatform,&renderPassPipeline->mPipelineCache,platform::core::QuickFormat("%s EffectPass mPipelineCache",name.c_str()));

    vulkan::Shader* v   =(vulkan::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
    vulkan::Shader* f   =(vulkan::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
    vulkan::Shader* c   =(vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	if(c)
	{
		vk::PipelineShaderStageCreateInfo shaderStageInfo =
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eCompute).setModule(c->mShader).setPName(c->entryPoint.c_str());
		vk::ComputePipelineCreateInfo computePipelineCreateInfo= vk::ComputePipelineCreateInfo()
																		.setLayout(mPipelineLayout)
																		.setStage(shaderStageInfo);
		//computePipelineCreateInfo	.setFlags(vk::PipelineCreateFlagBits::eDispatchBase);
		SIMUL_VK_CHECK(vulkanDevice->createComputePipelines(renderPassPipeline->mPipelineCache,1,&computePipelineCreateInfo, nullptr, &renderPassPipeline->mPipeline));
		SetVulkanName(renderPlatform,&renderPassPipeline->mPipeline,platform::core::QuickFormat("%s EffectPass compute mPipeline",name.c_str()));
	}
	else
	{
		auto *graphicsDeviceContext=deviceContext.AsGraphicsDeviceContext();
		crossplatform::TargetsAndViewport* tv;
		if (graphicsDeviceContext->targetStack.size())
			tv = graphicsDeviceContext->targetStack.top();
		else
			tv = &(graphicsDeviceContext->defaultTargetsAndViewport);
		int num_RT = tv->num;

		vk::RenderPass* rp=vulkanRenderPlatform->GetActiveVulkanRenderPass(*graphicsDeviceContext);
		bool rebuildRenderPass = false; //Check for valid pointer and underlying VkHandle.
		if (rp)
			rebuildRenderPass = !(*rp);
		else
			rebuildRenderPass = true;
		bool colour_write=true;
		bool depth_write=(tv->depthTarget.texture!=nullptr&&depthStencilState->desc.depth.write);
		if(rebuildRenderPass)
		{
			auto color_reference = vk::AttachmentReference().setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
			auto depth_reference = vk::AttachmentReference().setAttachment(1).setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			auto subpass = vk::SubpassDescription()
							.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

			subpass		.setInputAttachmentCount(0)
						.setPInputAttachments(nullptr)
						.setColorAttachmentCount(colour_write?1:0)
						.setPColorAttachments(colour_write?&color_reference:nullptr);
		
			vk::AttachmentDescription attachments[2];
			int a=0;
			if(colour_write)
				attachments[a++].setFormat(vulkan::RenderPlatform::ToVulkanFormat(pixelFormat))
																  .setSamples(vk::SampleCountFlagBits::e1)
																  .setLoadOp(vk::AttachmentLoadOp::eLoad)
																  .setStoreOp(vk::AttachmentStoreOp::eStore)
																  .setInitialLayout(vk::ImageLayout::eUndefined)
																  .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
			if(depth_write)
				attachments[a++].setFormat(vk::Format::eD32Sfloat).setSamples(vk::SampleCountFlagBits::e1)
																.setLoadOp(vk::AttachmentLoadOp::eLoad)
																.setStoreOp(vk::AttachmentStoreOp::eStore)
																.setStencilLoadOp(vk::AttachmentLoadOp::eLoad)
																.setStencilStoreOp(vk::AttachmentStoreOp::eStore)
																.setInitialLayout(vk::ImageLayout::eUndefined)
																.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			auto rp_info = vk::RenderPassCreateInfo()
				.setAttachmentCount(a)
				.setPAttachments(attachments)
				.setSubpassCount(1)
				.setPSubpasses(&subpass)
				.setDependencyCount(0)
				.setPDependencies(nullptr);

			result = vulkanDevice->createRenderPass(&rp_info, nullptr, &renderPassPipeline->mRenderPass);
			SIMUL_VK_CHECK(result);
			SetVulkanName(renderPlatform,&renderPassPipeline->mRenderPass,platform::core::QuickFormat("%s EffectPass mRenderPass",name.c_str()));
		}
		
		vk::PipelineShaderStageCreateInfo shaderStageInfo[2] = {
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eVertex).setModule(v->mShader).setPName(v->entryPoint.c_str()),
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eFragment).setModule(f->mShader).setPName(f->entryPoint.c_str())
		};
		
		vk::PipelineViewportStateCreateInfo viewportInfo			= vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);
		
		vk::PolygonMode polygonMode				= vk::PolygonMode::eFill;
		vk::CullModeFlags cullModeFlags			= vk::CullModeFlagBits::eNone;
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
			depthStencilInfo.setDepthTestEnable(depthStencilState->desc.depth.test)
																		.setDepthWriteEnable(depthStencilState->desc.depth.write)
																		.setDepthCompareOp(vulkanRenderPlatform->toVulkanComparison(depthStencilState->desc.depth.comparison))
																		.setDepthBoundsTestEnable(VK_FALSE)
																		.setStencilTestEnable(VK_FALSE)
																		.setFront(stencilOp)
																		.setBack(stencilOp);
		}

		
		vk::DynamicState dynamicStates[2]							= { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		auto dynamicStateInfo										= vk::PipelineDynamicStateCreateInfo().setPDynamicStates(dynamicStates).setDynamicStateCount(2);
	
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo	= vk::PipelineInputAssemblyStateCreateInfo().setTopology(platform::vulkan::RenderPlatform::toVulkanTopology(topology));
		if(topology!=crossplatform::Topology::UNDEFINED)
		{
			inputAssemblyInfo.setTopology(vulkan::RenderPlatform::toVulkanTopology(topology));
			//	rasterizerState->type;
		}
		vk::PipelineColorBlendStateCreateInfo colorBlendInfo		= vk::PipelineColorBlendStateCreateInfo();
        colorBlendInfo.setLogicOpEnable(false);// NOTE: normal blending disables the "Logic Op", it's not needed and interferes with blending.
        colorBlendInfo.setLogicOp (vk::LogicOp::eCopy);
		colorBlendInfo.setBlendConstants({ 1.0f,1.0f,1.0f,1.0f});
		std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(num_RT);
		if(blendState)
		{
			for(int i=0;i<num_RT;i++)
			{
				auto &b=colorBlendAttachments[i];
				auto &c=blendState->desc.blend.RenderTarget[0];
				b.setBlendEnable(c.blendOperationAlpha!=crossplatform::BLEND_OP_NONE||c.blendOperation!=crossplatform::BLEND_OP_NONE);
				b.setAlphaBlendOp(vulkan::RenderPlatform::toVulkanBlendOperation(c.blendOperationAlpha));
				b.setColorBlendOp(vulkan::RenderPlatform::toVulkanBlendOperation(c.blendOperation));
				b.setColorWriteMask((vk::ColorComponentFlagBits)c.RenderTargetWriteMask
								/*ColorComponentFlagBits::eR
									| vk::ColorComponentFlagBits::eG
									| vk::ColorComponentFlagBits::eB
									| vk::ColorComponentFlagBits::eA*/);
				b.setDstAlphaBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.DestBlendAlpha));
				b.setDstColorBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.DestBlend));
				b.setSrcAlphaBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.SrcBlendAlpha));
				b.setSrcColorBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.SrcBlend));
			}
		}
		colorBlendInfo.setAttachmentCount(num_RT).setPAttachments(colorBlendAttachments.data());
		
		// from the vertex shader's layout:
		const auto *layout=deviceContext.contextState.currentLayout;
		if(!layout)
			layout=&v->layout;
		vk::VertexInputAttributeDescription *vertexInputs=nullptr;
		const auto &layoutDesc=layout->GetDesc();
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		if(layoutDesc.size())
		{
			vk::VertexInputBindingDescription bindingDescription ;
			bindingDescription.setBinding (0);
			bindingDescription.setStride(layout->GetStructSize());
			bindingDescription.setInputRate(vk::VertexInputRate::eVertex);

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
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)layoutDesc.size();
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.pVertexAttributeDescriptions = vertexInputs;
		}
		else
		{
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.vertexAttributeDescriptionCount =0;
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
																		.setRenderPass((rp&&(*rp))?*rp:renderPassPipeline->mRenderPass);
		vk::Result res=vulkanDevice->createGraphicsPipelines(renderPassPipeline->mPipelineCache,1,&graphicsPipelineCreateInfo, nullptr, &renderPassPipeline->mPipeline);
		if(res!=vk::Result::eSuccess)
		{
			CreateTestPipeline(vulkanDevice,renderPassPipeline,v,f,rp);
			SIMUL_VK_CHECK(res);
			std::cerr<<"vk::Result="<<platform::vulkan::RenderPlatform::VulkanResultString(res)<<std::endl;
			std::cerr<<"Failed to create pipeline."<<std::endl;
		}
		SetVulkanName(renderPlatform,&renderPassPipeline->mPipeline,platform::core::QuickFormat("%s EffectPass renderPass Pipeline",name.c_str()));
		if(vertexInputs)
			delete [] vertexInputs;
	}
}

void EffectPass::Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) 
{
	auto rPlat = (vulkan::RenderPlatform*)renderPlatform;
	// If new frame, update current frame index and reset the apply count
	if (mLastFrameIndex != deviceContext.GetFrameNumber())
	{
		mCurApplyCount = 0;
		mInternalFrameIndex++;
		mInternalFrameIndex=mInternalFrameIndex%3;
		i_desc[mInternalFrameIndex]=mDescriptorSets[mInternalFrameIndex].begin();
		mLastFrameIndex = deviceContext.GetFrameNumber();
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
		// Could Totally be null if no inputs to the shader.
		//SIMUL_ASSERT(((VkDescriptorSet)*(i_desc[mInternalFrameIndex]))!=nullptr);
	}
	ApplyContextState(deviceContext,*i_desc[mInternalFrameIndex]);
	mCurApplyCount++;
	if(i_desc[mInternalFrameIndex]!=mDescriptorSets[mInternalFrameIndex].end())
		i_desc[mInternalFrameIndex]++;
}

void EffectPass::SetTextureHandles(crossplatform::DeviceContext & deviceContext)
{
}

RenderPassHash EffectPass::GetHash(crossplatform::PixelFormat pixelFormat, crossplatform::Topology topology, const crossplatform::Layout* layout)
{
	RenderPassHash  hashval = MakeRenderPassHash(pixelFormat,topology,layout,blendState,depthStencilState,rasterizerState);
	return hashval;
}

RenderPassHash EffectPass::MakeRenderPassHash(crossplatform::PixelFormat pixelFormat, crossplatform::Topology topology,
	const crossplatform::Layout *layout,
	const crossplatform::RenderState *blendState, const crossplatform::RenderState *depthStencilState, const crossplatform::RenderState *rasterizerState)
{
	unsigned long long hashval = (unsigned long long)pixelFormat * 1000 + (unsigned long long)topology;
	if(layout)
	{
		const auto &lDesc=layout->GetDesc();
		for(const auto &l:lDesc)
		{
			hashval+=((unsigned long long)l.format)*3279;
		}
	}
	if(blendState)
	{
		hashval+=blendState->desc.blend.AlphaToCoverageEnable?2048:0;
		hashval+=blendState->desc.blend.IndependentBlendEnable?4096:0;
		hashval+=blendState->desc.blend.numRTs*375;
		for(int i=0;i<blendState->desc.blend.numRTs;i++)
		{
			hashval+=(int)(blendState->desc.blend.RenderTarget[i].blendOperation)*136;
			hashval+=(int)(blendState->desc.blend.RenderTarget[i].blendOperationAlpha)*754;
			hashval+=(int)(blendState->desc.blend.RenderTarget[i].SrcBlend)*46346;
			hashval+=(int)(blendState->desc.blend.RenderTarget[i].DestBlend)*45684;
			hashval+=(int)(blendState->desc.blend.RenderTarget[i].SrcBlendAlpha)*23472;
			hashval+=(int)(blendState->desc.blend.RenderTarget[i].DestBlendAlpha)*65468;
			hashval+=(int)(blendState->desc.blend.RenderTarget[i].RenderTargetWriteMask)*2374;
		}
	}
	if(depthStencilState)
	{
		hashval+=depthStencilState->desc.depth.comparison*1359781;
		hashval+=depthStencilState->desc.depth.test*135163;
		hashval+=depthStencilState->desc.depth.write*1375;
	}
	if(rasterizerState)
	{
		hashval+=rasterizerState->desc.rasterizer.cullFaceMode*1359781;
		hashval+=rasterizerState->desc.rasterizer.frontFace*4264;
		hashval+=rasterizerState->desc.rasterizer.polygonMode*235;
		hashval+=rasterizerState->desc.rasterizer.polygonOffsetMode*129046;
	}
	return hashval;
}

vk::RenderPass &EffectPass::GetVulkanRenderPass(crossplatform::GraphicsDeviceContext & deviceContext)
{
	crossplatform::ContextState* cs = &deviceContext.contextState;
	crossplatform::PixelFormat pixelFormat=vulkan::RenderPlatform::GetActivePixelFormat(deviceContext);
	crossplatform::Topology topology=cs->topology;
	auto *rp=vulkanRenderPlatform->GetActiveVulkanRenderPass(deviceContext);
	if(rp)
		return *rp;
	RenderPassHash  hashval = MakeRenderPassHash(pixelFormat,topology,cs->currentLayout);
	const auto &p=mRenderPasses.find(hashval);
	if(p==mRenderPasses.end())
	{
		InitializePipeline(deviceContext,&mRenderPasses[hashval],pixelFormat,topology);
	}

	return mRenderPasses[hashval].mRenderPass;
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
