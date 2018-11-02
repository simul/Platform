#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/Vulkan/EffectPass.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"
#include "Simul/Platform/Vulkan/PlatformConstantBuffer.h"
#include "Simul/Platform/Vulkan/PlatformStructuredBuffer.h"

using namespace simul;
using namespace vulkan;

EffectPass::EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *e):
    crossplatform::EffectPass(r,e)
	,initialized(false),
    PassName("passname")
{
}

EffectPass::~EffectPass()
{
    InvalidateDeviceObjects();
}

void EffectPass::InvalidateDeviceObjects()
{
	vk::Device *device=renderPlatform->AsVulkanDevice();
	device->destroyRenderPass(mRenderPass, nullptr);

//	device->destroyPipeline(mPipeline, nullptr);
//	device->destroyPipelineCache(mPipelineCache, nullptr);
	device->destroyRenderPass(mRenderPass, nullptr);
	device->destroyPipelineLayout(mPipelineLayout, nullptr);
	device->destroyPipeline(mPipeline, nullptr);
	device->destroyDescriptorSetLayout(mDescLayout, nullptr);
}

void EffectPass::ApplyContextState(crossplatform::DeviceContext &deviceContext)
{
    crossplatform::ContextState* cs = &deviceContext.contextState;
	
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;
	if(!commandBuffer)
		return ;
	vk::Device *device	=renderPlatform->AsVulkanDevice();
	
    vulkan::Shader* c	= (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	
	//commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, 1,
	//	&swapchain_image_resources[current_buffer].descriptor_set, 0, nullptr);
	// Descriptor set is the chunk of data we send to the pass with pointers to textures, buffers etc.


	//auto buffer_info = vk::DescriptorBufferInfo().setOffset(0).setRange(sizeof(struct vktexcube_vs_uniform));

	
	int num_descr=numResourceSlots+numRwResourceSlots
			+numSbResourceSlots
			+numRwSbResourceSlots
			+numSamplerResourcerSlots
			+numConstantBufferResourceSlots;

	vk::WriteDescriptorSet *writes=new vk::WriteDescriptorSet[num_descr];

	
	cs->textureSlots =0;
	cs->rwTextureSlots =0;
	cs->bufferSlots =0;
	cs->rwTextureSlotsForSB =0;
	cs->textureSlotsForSB =0;

	int b=0;
	for(int i=0;i<numResourceSlots;i++,b++)
	{
		int slot=resourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		auto &ta = cs->textureAssignmentMap[slot];
		vk::ImageView *t;
		if(ta.texture&&ta.texture->IsValid())
		{
			ta.texture->FinishLoading(deviceContext);
			t=ta.texture->AsVulkanImageView(ta.resourceType,ta.index,ta.mip);
		}
		else
			t=nullptr;
		write.setDstBinding(slot);
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eSampledImage);
		vk::DescriptorImageInfo tex_desc;
		if(t)
			tex_desc.setImageView(*t);
		tex_desc.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		write.setPImageInfo(&tex_desc);
		
		cs->textureSlots |= 1 << slot;
	}
	for(int i=0;i<numRwResourceSlots;i++,b++)
	{
		int slot=rwResourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		auto &ta = cs->rwTextureAssignmentMap[slot];
		vk::ImageView *t;
		if(ta.texture&&ta.texture->IsValid())
			t=ta.texture->AsVulkanImageView(ta.resourceType,ta.index,ta.mip);
		else
			t=nullptr;
		write.setDstBinding(slot);
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageImage);
		vk::DescriptorImageInfo tex_desc;
		if(t)
			tex_desc.setImageView(*t);
		tex_desc.setImageLayout(vk::ImageLayout::eGeneral);
		write.setPImageInfo(&tex_desc);
		
		cs->rwTextureSlots |= 1 << slot;
	}
	for(int i=0;i<numSbResourceSlots;i++,b++)
	{
		int slot=sbResourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(slot);
		crossplatform::PlatformStructuredBuffer *sb =cs->applyStructuredBuffers[slot];
		if(!sb)
		{
			SIMUL_CERR<<"No structured buffer found\n";
		}
		else
		{
			vulkan::PlatformStructuredBuffer *psb=(vulkan::PlatformStructuredBuffer*)sb;
			psb->ActualApply(deviceContext,this,0);
			vk::Buffer *bf=psb->GetLastBuffer();
			write.setDescriptorCount(1);
			write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			SIMUL_ASSERT(bf!=nullptr);
			if(bf)
			{
				auto buffer_info = vk::DescriptorBufferInfo().setOffset(psb->GetLastOffset()).setRange(psb->GetSize()).setBuffer(*bf);
				write.setPBufferInfo(&buffer_info);
			}
		}
	}
	for(int i=0;i<numRwSbResourceSlots;i++,b++)
	{
		int slot=rwSbResourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(slot);
		crossplatform::PlatformStructuredBuffer *sb =cs->applyRwStructuredBuffers[slot];

		
		vulkan::PlatformStructuredBuffer *psb=(vulkan::PlatformStructuredBuffer*)sb;
		psb->ActualApply(deviceContext,this,0);
		vk::Buffer *bf=psb->GetLastBuffer();
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageBufferDynamic);
		SIMUL_ASSERT(bf!=nullptr);
		if(bf)
		{
			auto buffer_info = vk::DescriptorBufferInfo().setOffset(psb->GetLastOffset()).setRange(psb->GetSize()).setBuffer(*bf);
			write.setPBufferInfo(&buffer_info);
		}
	}
	for(int i=0;i<numSamplerResourcerSlots;i++,b++)
	{
		int slot=samplerResourceSlots[i];
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(slot);
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
			vk::DescriptorImageInfo imageInfo;
			imageInfo.setSampler(*s);
			write.setPImageInfo(&imageInfo);
		}

	}
	for(int i=0;i<numConstantBufferResourceSlots;i++,b++)
	{
		int slot=constantBufferResourceSlots[i];
		crossplatform::ConstantBufferBase *cb=cs->applyBuffers[slot];
		vulkan::PlatformConstantBuffer *pcb=(vulkan::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		vk::WriteDescriptorSet &write=writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(slot);
		pcb->ActualApply(deviceContext,this,0);
		vk::Buffer *bf=pcb->GetLastBuffer();
		vk::DeviceSize deviceSize=pcb->GetSize();
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
		SIMUL_ASSERT(bf!=nullptr);
		if(bf)
		{
			auto buffer_info = vk::DescriptorBufferInfo().setOffset(pcb->GetLastOffset()).setRange(deviceSize).setBuffer(*bf);
			write.setPBufferInfo(&buffer_info);
		}
		cs->bufferSlots|=(1<<slot);
	}

	device->updateDescriptorSets(num_descr, writes, 0, nullptr);


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
	if(!c)
	{
		vk::Framebuffer *framebuffer=((vulkan::RenderPlatform*)renderPlatform)->GetCurrentVulkanFramebuffer(deviceContext);
		vk::ClearValue const clearValues[2] = { vk::ClearColorValue(std::array<float, 4>({{0.2f, 0.2f, 0.2f, 0.2f}})),
										   vk::ClearDepthStencilValue(1.0f, 0u) };
		vk::RenderPassBeginInfo renderPassBeginInfo=vk::RenderPassBeginInfo().setRenderPass(mRenderPass)
													.setFramebuffer(*framebuffer)
													.setClearValueCount(1)
													.setPClearValues(clearValues);
		commandBuffer->beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);
	}
	if(c)
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, mPipeline);
	else
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);
	
	commandBuffer->bindDescriptorSets( vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			  

}

void EffectPass::Initialize()
{
	InvalidateDeviceObjects();
	vk::Device *device=renderPlatform->AsVulkanDevice();
	
	int swapchainImageCount=SIMUL_VULKAN_FRAME_LAG+1;
	// Create the "Descriptor Pool":
	vk::DescriptorPoolSize const poolSizes[] = {
		vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(swapchainImageCount)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eSampledImage).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eSampler).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageBufferDynamic).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageImage).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(swapchainImageCount * 32)};

	auto const descriptor_pool =
		vk::DescriptorPoolCreateInfo().setMaxSets(swapchainImageCount).setPoolSizeCount(_countof(poolSizes)).setPPoolSizes(poolSizes);

	auto result = device->createDescriptorPool(&descriptor_pool, nullptr, &mDescriptorPool);


    vulkan::Shader* v   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
    vulkan::Shader* f   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
    vulkan::Shader* c   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

// Create the "Descriptor Set Layout":
	int num_descr=numResourceSlots+numRwResourceSlots
			+numSbResourceSlots
			+numRwSbResourceSlots
			+numSamplerResourcerSlots
			+numConstantBufferResourceSlots;

	vk::DescriptorSetLayoutBinding *layout_bindings =new vk::DescriptorSetLayoutBinding[num_descr];
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

		binding.setBinding(slot)
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

		binding.setBinding(slot)
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

		binding.setBinding(slot)
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
		binding.setBinding(slot)
				.setDescriptorType(vk::DescriptorType::eStorageBufferDynamic)
				.setDescriptorCount(1)
				.setStageFlags(stageFlags)
				.setPImmutableSamplers(nullptr);
	}
	for(int i=0;i<numSamplerResourcerSlots;i++,b++)
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
		binding.setBinding(slot)
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
		binding.setBinding(slot)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setStageFlags(stageFlags)
				.setPImmutableSamplers(nullptr);
	}

	auto descriptor_layout = vk::DescriptorSetLayoutCreateInfo().setBindingCount(b).setPBindings(layout_bindings);

	result = device->createDescriptorSetLayout(&descriptor_layout, nullptr, &mDescLayout);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	auto pPipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo().setSetLayoutCount(1).setPSetLayouts(&mDescLayout);

	result = device->createPipelineLayout(&pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	
	
	vk::PipelineCacheCreateInfo pipelineCacheInfo;
	result = device->createPipelineCache(&pipelineCacheInfo, nullptr, &mPipelineCache);
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	if(c)
	{
		vk::PipelineShaderStageCreateInfo shaderStageInfo =
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eCompute).setModule(c->mShader).setPName(c->entryPoint.c_str());
		vk::ComputePipelineCreateInfo computePipelineCreateInfo= vk::ComputePipelineCreateInfo()
																		.setLayout(mPipelineLayout)
																		.setStage(shaderStageInfo);
		//computePipelineCreateInfo	.setFlags(vk::PipelineCreateFlagBits::eDispatchBase);
		device->createComputePipelines(mPipelineCache,1,&computePipelineCreateInfo, nullptr, &mPipeline);
	}
	else
	{
		auto  color_reference = vk::AttachmentReference().setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);

		auto  depth_reference =
			vk::AttachmentReference().setAttachment(1).setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		auto subpass = vk::SubpassDescription()
						.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

		subpass		.setInputAttachmentCount(0)
					.setPInputAttachments(nullptr)
					.setColorAttachmentCount(1)
					.setPColorAttachments(&color_reference);

	/*	subpass		.setPResolveAttachments(nullptr)
					.setPDepthStencilAttachment(&depth_reference)
					.setPreserveAttachmentCount(0)
					.setPPreserveAttachments(nullptr);*/
		
		vk::AttachmentDescription attachments[2] = { vk::AttachmentDescription()
															  .setFormat(vk::Format::eB8G8R8A8Unorm)
															  .setSamples(vk::SampleCountFlagBits::e1)
															  .setLoadOp(vk::AttachmentLoadOp::eClear)
															  .setStoreOp(vk::AttachmentStoreOp::eStore)
															  .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
															  .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
															  .setInitialLayout(vk::ImageLayout::eUndefined)
															  .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
														  vk::AttachmentDescription()
															  .setFormat(vk::Format::eD32Sfloat)
															  .setSamples(vk::SampleCountFlagBits::e1)
															  .setLoadOp(vk::AttachmentLoadOp::eClear)
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

		result = device->createRenderPass(&rp_info, nullptr, &mRenderPass);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		
		vk::PipelineShaderStageCreateInfo shaderStageInfo[2] = {
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eVertex).setModule(v->mShader).setPName(v->entryPoint.c_str()),
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eFragment).setModule(f->mShader).setPName(f->entryPoint.c_str()) };
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo	= vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleList);
		vk::PipelineViewportStateCreateInfo viewportInfo			= vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);
		
		vk::PipelineRasterizationStateCreateInfo rasterizationInfo	= vk::PipelineRasterizationStateCreateInfo()
																		.setDepthClampEnable(VK_FALSE)
																		.setRasterizerDiscardEnable(VK_FALSE)
																		.setPolygonMode(vk::PolygonMode::eFill)
																		.setCullMode(vk::CullModeFlagBits::eBack)
																		.setFrontFace(vk::FrontFace::eCounterClockwise)
																		.setDepthBiasEnable(VK_FALSE)
																		.setLineWidth(1.0f);
		vk::PipelineMultisampleStateCreateInfo multisampleInfo		= vk::PipelineMultisampleStateCreateInfo();
		vk::StencilOpState stencilOp								= vk::StencilOpState().setFailOp(vk::StencilOp::eKeep).setPassOp(vk::StencilOp::eKeep).setCompareOp(vk::CompareOp::eAlways);
		vk::PipelineDepthStencilStateCreateInfo depthStencilInfo	= vk::PipelineDepthStencilStateCreateInfo()
																		.setDepthTestEnable(VK_TRUE)
																		.setDepthWriteEnable(VK_TRUE)
																		.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
																		.setDepthBoundsTestEnable(VK_FALSE)
																		.setStencilTestEnable(VK_FALSE)
																		.setFront(stencilOp)
																		.setBack(stencilOp);

		vk::PipelineColorBlendAttachmentState colorBlendAttachments[1] = { vk::PipelineColorBlendAttachmentState().setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
																																	vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };
		
		vk::PipelineColorBlendStateCreateInfo colorBlendInfo		= vk::PipelineColorBlendStateCreateInfo().setAttachmentCount(1).setPAttachments(colorBlendAttachments);
		vk::DynamicState dynamicStates[2]							= { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		auto dynamicStateInfo										= vk::PipelineDynamicStateCreateInfo().setPDynamicStates(dynamicStates).setDynamicStateCount(2);
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
																		.setRenderPass(mRenderPass);
		//computePipelineCreateInfo	.setFlags(vk::PipelineCreateFlagBits::eDispatchBase);
		device->createGraphicsPipelines(mPipelineCache,1,&graphicsPipelineCreateInfo, nullptr, &mPipeline);
	}
	
	vk::DescriptorSetAllocateInfo alloc_info =	vk::DescriptorSetAllocateInfo()
													.setDescriptorPool(mDescriptorPool)
													.setDescriptorSetCount(1)
													.setPSetLayouts(&mDescLayout);
	result = device->allocateDescriptorSets(&alloc_info, &descriptorSet);
	delete [] layout_bindings;
	initialized=true;
}

void EffectPass::Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) 
{
    // Create the program:
    if (!initialized)
    {
		Initialize();
    }
	ApplyContextState(deviceContext);

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
}

void EffectPass::SetTextureHandles(crossplatform::DeviceContext & deviceContext)
{
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
