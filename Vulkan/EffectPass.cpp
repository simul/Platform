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

EffectPass::EffectPass(crossplatform::RenderPlatform* r, crossplatform::Effect* e) 
	:crossplatform::EffectPass(r, e), m_LastFrameIndex(0), m_InternalFrameIndex(0), m_CurrentApplyCount(0)
{
	for (int i = 0; i < s_DescriptorSetCount; i++)
		m_DescriptorSets_It[i] = m_DescriptorSets[i].begin();
}

EffectPass::~EffectPass()
{
	InvalidateDeviceObjects();
}

void EffectPass::InvalidateDeviceObjects()
{
	vulkan::RenderPlatform* rp = (vulkan::RenderPlatform*)renderPlatform;
	vk::Device* vulkanDevice = renderPlatform->AsVulkanDevice();

	if (!rp || !vulkanDevice)
		return;

	for (auto i : m_RenderPasses)
	{
		rp->PushToReleaseManager(i.second.renderPass);
		rp->PushToReleaseManager(i.second.pipeline);
		rp->PushToReleaseManager(i.second.pipelineCache);
	}
	m_RenderPasses.clear();

	rp->PushToReleaseManager(m_PipelineLayout);
	rp->PushToReleaseManager(m_DescriptorSetLayout);
	rp->PushToReleaseManager(m_DescriptorPool);
	for (int i = 0; i < s_DescriptorSetCount; i++)
	{
		m_DescriptorSets[i].clear();
		m_DescriptorSets_It[i] = m_DescriptorSets[i].begin();
	}

	renderPlatform = nullptr;
	m_Initialized = false;
}
void EffectPass::Apply(crossplatform::DeviceContext& deviceContext, bool asCompute)
{
	// If new frame, update current frame index and reset the apply count
	if (m_LastFrameIndex != renderPlatform->GetFrameNumber())
	{
		m_CurrentApplyCount = 0;
		m_InternalFrameIndex++;
		m_InternalFrameIndex = m_InternalFrameIndex % s_DescriptorSetCount;
		m_DescriptorSets_It[m_InternalFrameIndex] = m_DescriptorSets[m_InternalFrameIndex].begin();
		m_LastFrameIndex = renderPlatform->GetFrameNumber();
	}
	if (!m_Initialized)
		 CreateDescriptorPoolAndSetLayoutAndPipelineLayout();

	if (m_DescriptorSets_It[m_InternalFrameIndex] == m_DescriptorSets[m_InternalFrameIndex].end())
	{
		//must insert a new descriptor:
		m_DescriptorSets[m_InternalFrameIndex].push_back(vk::DescriptorSet());
		m_DescriptorSets_It[m_InternalFrameIndex] = m_DescriptorSets[m_InternalFrameIndex].end();
		m_DescriptorSets_It[m_InternalFrameIndex]--;
		AllocateDescriptorSets(*m_DescriptorSets_It[m_InternalFrameIndex]);
		//std::cout << "New Descriptor Set addded: 0x" << std::hex << (void*)((*m_DescriptorSets_It[m_InternalFrameIndex]).operator VkDescriptorSet()) << std::dec << std::endl;
	}

	ApplyContextState(deviceContext, *m_DescriptorSets_It[m_InternalFrameIndex]);

	m_CurrentApplyCount++;
	if (m_DescriptorSets_It[m_InternalFrameIndex] != m_DescriptorSets[m_InternalFrameIndex].end())
		m_DescriptorSets_It[m_InternalFrameIndex]++;
}

void EffectPass::ApplyContextState(crossplatform::DeviceContext& deviceContext, vk::DescriptorSet& descriptorSet)
{
	crossplatform::ContextState* cs = &deviceContext.contextState;
	vulkan::Shader* c = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
	vk::Device* vulkanDevice = renderPlatform->AsVulkanDevice();

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

	int numDescriptors = numResourceSlots + numRwResourceSlots
		+ numSbResourceSlots
		+ numRwSbResourceSlots
		+ numSamplerResourceSlots
		+ numConstantBufferResourceSlots;

	vk::WriteDescriptorSet* writes = new vk::WriteDescriptorSet[numDescriptors];

	cs->textureSlots = 0;
	cs->rwTextureSlots = 0;
	cs->bufferSlots = 0;
	cs->rwTextureSlotsForSB = 0;
	cs->textureSlotsForSB = 0;

	vk::DescriptorImageInfo descriptorImageInfo[32];
	int b = 0;
	for (int i = 0; i < numResourceSlots; i++, b++)
	{
		int slot = resourceSlots[i];
		vk::WriteDescriptorSet& write = writes[b];
		write.setDstSet(descriptorSet);
		crossplatform::TextureAssignment& ta = cs->textureAssignmentMap[slot];
		vk::ImageView* vkImageView;
		vulkan::Texture* texture = (vulkan::Texture*)(ta.texture);
		const crossplatform::ShaderResource* res = effect->GetShaderResourceAtSlot(slot);
		crossplatform::ShaderResourceType requiredType = crossplatform::ShaderResourceType::UNKNOWN;
		if (texture && res && ta.resourceType != res->shaderResourceType)
		{
			requiredType = res->shaderResourceType;
			ta.resourceType = requiredType;
			texture = nullptr;
		}
		if (!texture)
		{
			// We really don't want to have to do this, but Vulkan GLSL can't eliminate unused textures in compilation:
			if (ta.resourceType == crossplatform::ShaderResourceType::UNKNOWN)
			{
				ta.resourceType = requiredType;
			}
			texture = ((vulkan::RenderPlatform*)renderPlatform)->GetDummyTexture(ta.resourceType);
		}
		if (texture)
		{
			if (!texture->IsValid())
			{
				// We really don't want to have to do this, but Vulkan GLSL can't eliminate unused textures in compilation:
				if (ta.resourceType == crossplatform::ShaderResourceType::UNKNOWN)
				{
					ta.resourceType = requiredType;
				}
				texture = ((vulkan::RenderPlatform*)renderPlatform)->GetDummyTexture(ta.resourceType);
			}
		}
		if (!texture)
		{
			SIMUL_CERR_ONCE << "Texture is null.\n";
			return;
		}
		if (!texture->IsValid())
		{
			SIMUL_CERR_ONCE << "Texture is invalid.\n";
			return;
		}
		texture->FinishLoading(deviceContext);
		texture->SetLayout(deviceContext, vk::ImageLayout::eShaderReadOnlyOptimal, ta.index, ta.mip);
		write.setDstBinding(GenerateTextureSlot(slot));
		write.setDescriptorCount(1);
		vkImageView = texture->AsVulkanImageView(ta.resourceType, ta.index, ta.mip);
		if (vkImageView)
			descriptorImageInfo[i].setImageView(*vkImageView);
		descriptorImageInfo[i].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		if (!m_VideoSource)
		{
			write.setDescriptorType(vk::DescriptorType::eSampledImage);
		}
		else
		{
			// video texture:
			write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
			descriptorImageInfo[i].setSampler(vulkanRenderPlatform->GetSamplerYcbcr());
		}
		write.setPImageInfo(&descriptorImageInfo[i]);
		cs->textureSlots |= 1 << slot;
	}
	vk::DescriptorImageInfo img_desc[8];
	for (int i = 0; i < numRwResourceSlots; i++, b++)
	{
		int slot = rwResourceSlots[i];
		vk::WriteDescriptorSet& write = writes[b];
		write.setDstSet(descriptorSet);
		crossplatform::TextureAssignment& ta = cs->rwTextureAssignmentMap[slot];
		vk::ImageView* vkImageView;
		vulkan::Texture* texture = (vulkan::Texture*)(ta.texture);
		if (texture && texture->IsValid())
		{
			vkImageView = texture->AsVulkanImageView(ta.resourceType, ta.index, ta.mip, true);
			texture->SetLayout(deviceContext, vk::ImageLayout::eGeneral, ta.index, ta.mip);
		}
		else
		{
			// probably not actually used in the shader.
			numDescriptors--;
			b--;
			continue;
		}
		write.setDstBinding(GenerateTextureWriteSlot(slot));
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageImage);
		if (vkImageView)
			img_desc[i].setImageView(*vkImageView);
		img_desc[i].setImageLayout(vk::ImageLayout::eGeneral);
		write.setPImageInfo(&img_desc[i]);

		cs->rwTextureSlots |= 1 << slot;
	}
	vk::DescriptorBufferInfo sbInfo[8];
	for (int i = 0; i < numSbResourceSlots; i++, b++)
	{
		int slot = sbResourceSlots[i];
		vk::WriteDescriptorSet& write = writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateTextureSlot(slot));
		crossplatform::PlatformStructuredBuffer* sb = cs->applyStructuredBuffers[slot];
		if (!sb)
		{
			SIMUL_CERR << "No structured buffer found\n";
		}
		else
		{
			vulkan::PlatformStructuredBuffer* psb = (vulkan::PlatformStructuredBuffer*)sb;
			psb->ActualApply(deviceContext, this, slot, false);
			vk::Buffer* vkBuffer = psb->GetLastBuffer();
			write.setDescriptorCount(1);
			write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			SIMUL_ASSERT(vkBuffer != nullptr);
			if (vkBuffer)
			{
				sbInfo[i].setOffset(psb->GetLastOffset()).setRange(psb->GetSize()).setBuffer(*vkBuffer);
				write.setPBufferInfo(&sbInfo[i]);
			}
		}
	}
	vk::DescriptorBufferInfo rwSbInfo[8];
	for (int i = 0; i < numRwSbResourceSlots; i++, b++)
	{
		int slot = rwSbResourceSlots[i];
		vk::WriteDescriptorSet& write = writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateTextureWriteSlot(slot));
		crossplatform::PlatformStructuredBuffer* sb = cs->applyRwStructuredBuffers[slot];
		if (!sb)
		{
			// probably not actually used in the shader.
			numDescriptors--;
			b--;
			continue;
		}
		vulkan::PlatformStructuredBuffer* psb = (vulkan::PlatformStructuredBuffer*)sb;
		psb->ActualApply(deviceContext, this, slot, true);
		vk::Buffer* vkBuffer = psb->GetLastBuffer();
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
		SIMUL_ASSERT(vkBuffer != nullptr);
		if (vkBuffer)
		{
			rwSbInfo[i].setOffset(psb->GetLastOffset()).setRange(psb->GetSize()).setBuffer(*vkBuffer);
			write.setPBufferInfo(&rwSbInfo[i]);
		}
		// TODO: this is overkill, synchronizing all uav's
		renderPlatform->ResourceBarrierUAV(deviceContext, sb);
	}
	vk::DescriptorImageInfo samplerInfo[16];
	for (int i = 0; i < numSamplerResourceSlots; i++, b++)
	{
		int slot = samplerResourceSlots[i];
		vk::WriteDescriptorSet& write = writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateSamplerSlot(slot));
		crossplatform::SamplerState* ss = nullptr;
		if (deviceContext.contextState.samplerStateOverrides.size() > 0 && deviceContext.contextState.samplerStateOverrides.HasValue(slot))
		{
			ss = deviceContext.contextState.samplerStateOverrides[slot];
		}
		if (!ss)
		{
			ss = effect->GetSamplers()[slot];
		}
		vk::Sampler* vkSampler = ss->AsVulkanSampler();
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eSampler);
		SIMUL_ASSERT(vkSampler != nullptr);
		if (vkSampler)
		{
			samplerInfo[i].setSampler(*vkSampler);
			write.setPImageInfo(&samplerInfo[i]);
		}

	}
	vk::DescriptorBufferInfo bufferInfo[8];
	for (int i = 0; i < numConstantBufferResourceSlots; i++, b++)
	{
		int slot = constantBufferResourceSlots[i];
		crossplatform::ConstantBufferBase* cb = cs->applyBuffers[slot];
		if (!cb)
		{
			numDescriptors--;
			b--;
			SIMUL_BREAK_ONCE("Possibly missing constant buffer");
			continue;
		}
		vulkan::PlatformConstantBuffer* pcb = (vulkan::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		vk::WriteDescriptorSet& write = writes[b];
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateConstantBufferSlot(slot));
		pcb->ActualApply(deviceContext, this, 0);
		vk::Buffer* vkBuffer = pcb->GetLastBuffer();
		vk::DeviceSize vkDeviceSize = pcb->GetSize();
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
		SIMUL_ASSERT(vkBuffer != nullptr);
		if (vkBuffer)
		{
			bufferInfo[i].setOffset(pcb->GetLastOffset())
				.setRange(vkDeviceSize)
				.setBuffer(*vkBuffer);
			write.setPBufferInfo(&bufferInfo[i]);
		}
		cs->bufferSlots |= (1 << slot);
	}
	if (numDescriptors)
	{
		for (int i = 0; i < numDescriptors; i++)
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
		vulkanDevice->updateDescriptorSets(numDescriptors, writes, 0, nullptr);
	}
	delete[] writes;

	// Now verify that ALL resource are set:
	static bool error_checking = true;
	if (error_checking)
	{
		unsigned required_slots = GetTextureSlots();
		if ((cs->textureSlots & required_slots) != required_slots)
		{
			static int count = 10;
			count--;
			if (count > 0)
			{
				SIMUL_CERR << "Not all texture slots are assigned:" << std::endl;
				unsigned missing_slots = required_slots & (~cs->textureSlots);
				for (unsigned i = 0; i < 32; i++)
				{
					unsigned slot = 1 << i;
					if (slot & missing_slots)
					{
						std::string name;
						if (cs->currentEffect)
							name = cs->currentEffect->GetTextureForSlot(i);
						SIMUL_CERR << "\tSlot " << i << ": " << name.c_str() << ", was not set." << std::endl;
					}
				}
				SIMUL_BREAK_ONCE("Many API's require all used textures to have valid data.");
			}
		}
		unsigned required_rw_slots = GetRwTextureSlots();
		if ((cs->rwTextureSlots & required_rw_slots) != required_rw_slots)
		{
			static int count = 10;
			count--;
			if (count > 0)
			{
				//SIMUL_BREAK_ONCE("Not all rw texture slots are assigned.");
				required_rw_slots = required_rw_slots & (~cs->rwTextureSlots);
				for (unsigned i = 0; i < 32; i++)
				{
					unsigned slot = 1 << i;
					if (slot & required_rw_slots)
					{
						std::string name;
						if (cs->currentEffect)
							name = cs->currentEffect->GetTextureForSlot(1000 + i);
						SIMUL_CERR << "RW Slot " << i << " was not set (" << name.c_str() << ")." << std::endl;
					}
				}
			}
		}
	}
		
	m_DescriptorSet = descriptorSet;

	crossplatform::GraphicsDeviceContext* graphicsDeviceContext = deviceContext.AsGraphicsDeviceContext();
	RenderPassHash hashval = 0;
	RenderPassPipeline* renderPassPipeline = nullptr;
	if (c)
	{
		const auto& p = m_RenderPasses.find(hashval);
		if (p == m_RenderPasses.end())
		{
			renderPassPipeline = &m_RenderPasses[hashval];
			InitializePipeline(*graphicsDeviceContext, renderPassPipeline, crossplatform::PixelFormat::UNKNOWN, crossplatform::Topology::UNDEFINED);
		}
		else
			renderPassPipeline = &(m_RenderPasses[hashval]);
	}
	else
	{
		crossplatform::PixelFormat pixelFormat = crossplatform::PixelFormat::UNKNOWN;
		pixelFormat = vulkanRenderPlatform->GetActivePixelFormat(*graphicsDeviceContext);
		crossplatform::Topology appliedTopology = topology;
		if (cs->topology != crossplatform::Topology::UNDEFINED)
			appliedTopology = cs->topology;
		if (appliedTopology == crossplatform::Topology::UNDEFINED)
			appliedTopology = crossplatform::Topology::TRIANGLESTRIP;

		hashval = MakeRenderPassHash(pixelFormat, appliedTopology, cs->currentLayout, blendState, depthStencilState, rasterizerState, multiview);
		const auto& p = m_RenderPasses.find(hashval);
		if (p == m_RenderPasses.end())
		{
			renderPassPipeline = &m_RenderPasses[hashval];
			InitializePipeline(*graphicsDeviceContext, renderPassPipeline, pixelFormat, appliedTopology, blendState, depthStencilState, rasterizerState, multiview);
		}
		else
			renderPassPipeline = &(m_RenderPasses[hashval]);

		// Now figure out the layout business for the rendertargets:
		crossplatform::TargetsAndViewport* tv;
		if (graphicsDeviceContext->targetStack.size())
			tv = graphicsDeviceContext->targetStack.top();
		else
			tv = &(graphicsDeviceContext->defaultTargetsAndViewport);

		for (int i = 0; i < tv->num; i++)
		{
			crossplatform::TargetsAndViewport::TextureTarget& tt = tv->textureTargets[i];
			if (tt.texture)
			{
				Texture* texture = (Texture*)tt.texture;
				bool allLayers = texture->NumFaces() == tt.layerCount;
				texture->SetLayout(*graphicsDeviceContext, vk::ImageLayout::eColorAttachmentOptimal, allLayers ? -1 : tt.layer, tt.mip);
			}
		}
		if (tv->depthTarget.texture && depthStencilState)
		{
			crossplatform::TargetsAndViewport::TextureTarget& dt = tv->depthTarget;
			const bool& depthWrite = depthStencilState->desc.depth.write;
			const bool& depthTest = depthStencilState->desc.depth.test;
			if (dt.texture && (depthWrite || depthTest))
			{
				Texture* texture = (Texture*)dt.texture;
				bool allLayers = texture->NumFaces() == dt.layerCount;
				vk::ImageLayout layout = depthWrite ? vk::ImageLayout::eDepthStencilAttachmentOptimal : depthTest ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eUndefined;
				texture->SetLayout(*graphicsDeviceContext, layout, allLayers ? -1 : dt.layer, allLayers ? -1 :dt.mip);
			}
		}
	}
}

void EffectPass::CreateDescriptorPoolAndSetLayoutAndPipelineLayout()
{
	vk::Device* vulkanDevice = renderPlatform->AsVulkanDevice();
	m_Initialized = true;
	int swapchainImageCount = SIMUL_VULKAN_FRAME_LAG + 1;
	
	// TODO: This is super-inefficient:
	int numDescriptors = numResourceSlots
		+ numRwResourceSlots
		+ numSbResourceSlots
		+ numRwSbResourceSlots
		+ numSamplerResourceSlots
		+ numConstantBufferResourceSlots
		+ numResourceSlots;
	vk::Result result;

	vk::DescriptorSetLayoutBinding* layoutBindings = nullptr;
	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCI;

	if (numDescriptors > 0)
	{
		// Create the "Descriptor Pool":
		{
			vk::DescriptorPoolSize* poolSizes = new vk::DescriptorPoolSize[numDescriptors];
			int poolSizeIdx = 0;
			static int countPerFrame = 256;
			if (numResourceSlots)
			{
				vk::DescriptorType type = m_VideoSource ? vk::DescriptorType::eCombinedImageSampler : vk::DescriptorType::eSampledImage; //Video needs Combined image sampler
				poolSizes[poolSizeIdx++].setType(type).setDescriptorCount(countPerFrame * swapchainImageCount * numResourceSlots);
			}
			if (numSamplerResourceSlots)
			{
				poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eSampler).setDescriptorCount(countPerFrame * swapchainImageCount * numSamplerResourceSlots);
			}
			if (numSbResourceSlots + numRwSbResourceSlots)
			{
				poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(countPerFrame * swapchainImageCount * (numSbResourceSlots + numRwSbResourceSlots));
			}
			if (numRwResourceSlots)
			{
				poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eStorageImage).setDescriptorCount(countPerFrame * swapchainImageCount * numRwResourceSlots);
			}
			if (numConstantBufferResourceSlots)
			{
				poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(countPerFrame * swapchainImageCount * numConstantBufferResourceSlots);
			}
			const vk::DescriptorPoolCreateInfo descriptorPoolCI = vk::DescriptorPoolCreateInfo().setMaxSets(swapchainImageCount * countPerFrame).setPoolSizeCount(poolSizeIdx).setPPoolSizes(poolSizes);
			result = vulkanDevice->createDescriptorPool(&descriptorPoolCI, nullptr, &m_DescriptorPool);
			SIMUL_VK_CHECK(result);
			SetVulkanName(renderPlatform, m_DescriptorPool, platform::core::QuickFormat("%s Descriptor pool", name.c_str()));
			delete[] poolSizes;
		}

		// Set up the "Descriptor Set Layout CreateInfo":
		{
			layoutBindings = new vk::DescriptorSetLayoutBinding[numDescriptors];
			int bindingIndex = 0;
			for (int i = 0; i < numResourceSlots; i++, bindingIndex++)
			{
				int slot = resourceSlots[i];
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesTextureSlot);
				binding.setBinding(GenerateTextureSlot(slot))
					.setDescriptorType(m_VideoSource ? vk::DescriptorType::eCombinedImageSampler : vk::DescriptorType::eSampledImage)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags);
				if (m_VideoSource)
				{
					m_ImmutableSamplers.clear();
					m_ImmutableSamplers.push_back(vulkanRenderPlatform->GetSamplerYcbcr());
					binding.setPImmutableSamplers(m_ImmutableSamplers.data());
				}
			}
			for (int i = 0; i < numRwResourceSlots; i++, bindingIndex++)
			{
				int slot = rwResourceSlots[i];
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesRwTextureSlot);
				binding.setBinding(GenerateTextureWriteSlot(slot))
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
			}
			for (int i = 0; i < numSbResourceSlots; i++, bindingIndex++)
			{
				int slot = sbResourceSlots[i];
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesTextureSlotForSB);
				binding.setBinding(GenerateTextureSlot(slot))
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
			}
			for (int i = 0; i < numRwSbResourceSlots; i++, bindingIndex++)
			{
				int slot = rwSbResourceSlots[i];
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesRwTextureSlotForSB);
				binding.setBinding(GenerateTextureWriteSlot(slot))
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
			}
			for (int i = 0; i < numSamplerResourceSlots; i++, bindingIndex++)
			{
				int slot = samplerResourceSlots[i];
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesSamplerSlot);
				binding.setBinding(GenerateSamplerSlot(slot))
					.setDescriptorType(vk::DescriptorType::eSampler)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
			}
			for (int i = 0; i < numConstantBufferResourceSlots; i++, bindingIndex++)
			{
				int slot = constantBufferResourceSlots[i];
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesConstantBufferSlot);
				binding.setBinding(GenerateConstantBufferSlot(slot))
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
			}
			
			descriptorSetLayoutCI.setBindingCount(bindingIndex).setPBindings(layoutBindings);
		}
	}

	//Override Desciptor Set Layout CreateInfo for Video Source?
	if (m_VideoSource)
	{
		std::vector<vk::Sampler> samplers = { vulkanRenderPlatform->GetSamplerYcbcr() };
		vk::DescriptorSetLayoutBinding binding = vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment)
			.setImmutableSamplers(samplers);
		vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(1)
			.setBindings(binding);
	}

	// Create the "Descriptor Set Layout":
	result = vulkanDevice->createDescriptorSetLayout(&descriptorSetLayoutCI, nullptr, &m_DescriptorSetLayout);
	SIMUL_VK_CHECK(result);
	SetVulkanName(renderPlatform, m_DescriptorSetLayout, platform::core::QuickFormat("%s Descriptor layout", name.c_str()));

	// Create the "Pipeline Layout":
	vk::PipelineLayoutCreateInfo pipelineLayoutCI = vk::PipelineLayoutCreateInfo().setSetLayoutCount(1).setPSetLayouts(&m_DescriptorSetLayout);
	result = vulkanDevice->createPipelineLayout(&pipelineLayoutCI, nullptr, &m_PipelineLayout);
	SIMUL_VK_CHECK(result);
	SetVulkanName(renderPlatform, m_PipelineLayout, platform::core::QuickFormat("%s EffectPass Pipeline layout", name.c_str()));
	
	delete[] layoutBindings;
	layoutBindings = nullptr;
}

void EffectPass::AllocateDescriptorSets(vk::DescriptorSet& descriptorSet)
{
	// Of course, only need to do this if there are ANY inputs.
	if (!m_DescriptorSetLayout)
		return;

	vk::Device* vulkanDevice = renderPlatform->AsVulkanDevice();
	vk::DescriptorSetLayout descriptorSetLayout[s_DescriptorSetCount];

	for (int i = 0; i < s_DescriptorSetCount; i++)
	{
		descriptorSetLayout[i] = m_DescriptorSetLayout;
	}
	vk::DescriptorSetAllocateInfo alloc_info = vk::DescriptorSetAllocateInfo()
		.setDescriptorSetCount(1)
		.setPSetLayouts(descriptorSetLayout);
	
	if (m_DescriptorPool)
	{
		alloc_info = alloc_info.setDescriptorPool(m_DescriptorPool);
		vk::Result result = vulkanDevice->allocateDescriptorSets(&alloc_info, &descriptorSet);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		SetVulkanName(renderPlatform, descriptorSet, platform::core::QuickFormat("%s Descriptor set", name.c_str()));
	}
}

int EffectPass::GenerateSamplerSlot(int s, bool offset)
{
	if (offset)
		return s + 300;
	return s;
}

int EffectPass::GenerateTextureSlot(int s, bool offset)
{
	if (offset)
		return s + 100;
	return s;
}

int EffectPass::GenerateTextureWriteSlot(int s, bool offset)
{
	if (offset)
		return s + 200;
	return s;
}

int EffectPass::GenerateConstantBufferSlot(int s, bool offset)
{
	return s;
}

vk::ShaderStageFlags EffectPass::GetShaderFlagsForSlot(int slot, bool(platform::crossplatform::Shader::*pfn)(int) const)
{
	vulkan::Shader* v = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
	vulkan::Shader* f = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
	vulkan::Shader* c = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

	vk::ShaderStageFlags stageFlags;
	if (f && (f->*pfn)(slot))
		stageFlags |= vk::ShaderStageFlagBits::eFragment;
	if (v && (v->*pfn)(slot))
		stageFlags |= vk::ShaderStageFlagBits::eVertex;
	if (c && (c->*pfn)(slot))
		stageFlags |= vk::ShaderStageFlagBits::eCompute;

	return stageFlags;
}

void EffectPass::InitializePipeline(crossplatform::GraphicsDeviceContext& deviceContext, RenderPassPipeline* renderPassPipeline, crossplatform::PixelFormat pixelFormat,
	crossplatform::Topology topology, const crossplatform::RenderState* blendState, const crossplatform::RenderState* depthStencilState, const crossplatform::RenderState* rasterizerState, bool multiview)
{
	vk::Device* vulkanDevice = renderPlatform->AsVulkanDevice();
	vk::PipelineCacheCreateInfo pipelineCacheInfo;
	vk::Result result = vulkanDevice->createPipelineCache(&pipelineCacheInfo, nullptr, &renderPassPipeline->pipelineCache);
	SIMUL_VK_CHECK(result);
	SetVulkanName(renderPlatform, renderPassPipeline->pipelineCache, platform::core::QuickFormat("%s EffectPass mPipelineCache", name.c_str()));

	vulkan::Shader* v = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
	vulkan::Shader* f = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
	vulkan::Shader* c = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

	if (c)
	{
		vk::PipelineShaderStageCreateInfo shaderStageInfo =
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eCompute).setModule(c->mShader).setPName(c->entryPoint.c_str());
		vk::ComputePipelineCreateInfo computePipelineCreateInfo = vk::ComputePipelineCreateInfo()
			.setLayout(m_PipelineLayout)
			.setStage(shaderStageInfo);
		SIMUL_VK_CHECK(vulkanDevice->createComputePipelines(renderPassPipeline->pipelineCache, 1, &computePipelineCreateInfo, nullptr, &renderPassPipeline->pipeline));
		SetVulkanName(renderPlatform, renderPassPipeline->pipeline, platform::core::QuickFormat("%s EffectPass compute mPipeline", name.c_str()));
	}
	else
	{
		crossplatform::GraphicsDeviceContext* graphicsDeviceContext = deviceContext.AsGraphicsDeviceContext();
		crossplatform::TargetsAndViewport* tv;
		if (graphicsDeviceContext->targetStack.size())
			tv = graphicsDeviceContext->targetStack.top();
		else
			tv = &(graphicsDeviceContext->defaultTargetsAndViewport);
		int num_RT = tv->num;

		bool colour_write = true;
		bool depth_write = (tv->depthTarget.texture != nullptr && depthStencilState->desc.depth.write);
		bool depth_read = (tv->depthTarget.texture != nullptr && depthStencilState->desc.depth.test);
		crossplatform::PixelFormat depthFormat = (depth_write || depth_read) ? crossplatform::PixelFormat::D_32_FLOAT : crossplatform::PixelFormat::UNKNOWN;

		vulkanRenderPlatform->CreateVulkanRenderpass(deviceContext, renderPassPipeline->renderPass, num_RT, &pixelFormat,
			depthFormat, depth_read, depth_write, false, 1, multiview);
		SetVulkanName(renderPlatform, renderPassPipeline->renderPass, platform::core::QuickFormat("%s EffectPass mRenderPass", name.c_str()));

		vk::PipelineShaderStageCreateInfo shaderStageInfo[2] = {
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eVertex).setModule(v->mShader).setPName(v->entryPoint.c_str()),
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eFragment).setModule(f->mShader).setPName(f->entryPoint.c_str())
		};

		vk::PipelineViewportStateCreateInfo viewportInfo = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);

		vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
		vk::CullModeFlags cullModeFlags = vk::CullModeFlagBits::eNone;
		vk::FrontFace frontFace = vk::FrontFace::eCounterClockwise;
		if (rasterizerState)
		{
			polygonMode = vulkan::RenderPlatform::toVulkanPolygonMode(rasterizerState->desc.rasterizer.polygonMode);
			cullModeFlags = vulkan::RenderPlatform::toVulkanCullFace(rasterizerState->desc.rasterizer.cullFaceMode);
			if (rasterizerState->desc.rasterizer.frontFace == crossplatform::FrontFace::FRONTFACE_CLOCKWISE)
				frontFace = vk::FrontFace::eClockwise;

		}

		vk::PipelineRasterizationStateCreateInfo rasterizationInfo = vk::PipelineRasterizationStateCreateInfo()
			.setDepthClampEnable(VK_FALSE)
			.setRasterizerDiscardEnable(VK_FALSE)
			.setPolygonMode(polygonMode)
			.setCullMode(cullModeFlags)
			.setFrontFace(frontFace)
			.setDepthBiasEnable(VK_FALSE)
			.setLineWidth(1.0f);
		vk::PipelineMultisampleStateCreateInfo multisampleInfo = vk::PipelineMultisampleStateCreateInfo();
		vk::StencilOpState stencilOp = vk::StencilOpState().setFailOp(vk::StencilOp::eKeep).setPassOp(vk::StencilOp::eKeep).setCompareOp(vk::CompareOp::eAlways);
		vk::PipelineDepthStencilStateCreateInfo depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo()
			.setDepthTestEnable(VK_FALSE)
			.setDepthWriteEnable(VK_FALSE)
			.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
			.setDepthBoundsTestEnable(VK_FALSE)
			.setStencilTestEnable(VK_FALSE)
			.setFront(stencilOp)
			.setBack(stencilOp);
		if (depthStencilState)
		{
			depthStencilInfo.setDepthTestEnable(depthStencilState->desc.depth.test)
				.setDepthWriteEnable(depthStencilState->desc.depth.write)
				.setDepthCompareOp(vulkanRenderPlatform->toVulkanComparison(depthStencilState->desc.depth.comparison))
				.setDepthBoundsTestEnable(VK_FALSE)
				.setStencilTestEnable(VK_FALSE)
				.setFront(stencilOp)
				.setBack(stencilOp);
		}

		vk::DynamicState dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo().setPDynamicStates(dynamicStates).setDynamicStateCount(2);

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo().setTopology(platform::vulkan::RenderPlatform::toVulkanTopology(topology));
		if (topology != crossplatform::Topology::UNDEFINED)
		{
			inputAssemblyInfo.setTopology(vulkan::RenderPlatform::toVulkanTopology(topology));
		}
		vk::PipelineColorBlendStateCreateInfo colorBlendInfo = vk::PipelineColorBlendStateCreateInfo();
		colorBlendInfo.setLogicOpEnable(false);// NOTE: normal blending disables the "Logic Op", it's not needed and interferes with blending.
		colorBlendInfo.setLogicOp(vk::LogicOp::eCopy);
		colorBlendInfo.setBlendConstants({ 1.0f,1.0f,1.0f,1.0f });
		std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(num_RT);
		if (blendState)
		{
			for (int i = 0; i < num_RT; i++)
			{
				auto& b = colorBlendAttachments[i];
				auto& c = blendState->desc.blend.RenderTarget[0];
				b.setBlendEnable(c.blendOperationAlpha != crossplatform::BLEND_OP_NONE || c.blendOperation != crossplatform::BLEND_OP_NONE);
				b.setAlphaBlendOp(vulkan::RenderPlatform::toVulkanBlendOperation(c.blendOperationAlpha));
				b.setColorBlendOp(vulkan::RenderPlatform::toVulkanBlendOperation(c.blendOperation));
				b.setColorWriteMask((vk::ColorComponentFlagBits)c.RenderTargetWriteMask);
				b.setDstAlphaBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.DestBlendAlpha));
				b.setDstColorBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.DestBlend));
				b.setSrcAlphaBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.SrcBlendAlpha));
				b.setSrcColorBlendFactor(vulkan::RenderPlatform::toVulkanBlendFactor(c.SrcBlend));
			}
		}
		colorBlendInfo.setAttachmentCount(num_RT).setPAttachments(colorBlendAttachments.data());

		// from the vertex shader's layout:
		const auto* layout = deviceContext.contextState.currentLayout;
		if (!layout)
			layout = &v->layout;
		vk::VertexInputAttributeDescription* vertexInputs = nullptr;
		const auto& layoutDesc = layout->GetDesc();
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vk::VertexInputBindingDescription bindingDescription;
		if (layoutDesc.size())
		{
			bindingDescription.setBinding(0);
			bindingDescription.setStride(layout->GetStructSize());
			bindingDescription.setInputRate(vk::VertexInputRate::eVertex);

			if (layoutDesc.size())
				vertexInputs = new vk::VertexInputAttributeDescription[layoutDesc.size()];
			int slot = 0;
			for (auto desc : layoutDesc)
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
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
		}

		vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo = vk::GraphicsPipelineCreateInfo()
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
			.setLayout(m_PipelineLayout)
			.setRenderPass(renderPassPipeline->renderPass);

		vk::Result res = vulkanDevice->createGraphicsPipelines(renderPassPipeline->pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &renderPassPipeline->pipeline);
		if (res != vk::Result::eSuccess)
		{
			SIMUL_VK_CHECK(res);
			std::cerr << "vk::Result=" << platform::vulkan::RenderPlatform::VulkanResultString(res) << std::endl;
			std::cerr << "Failed to create pipeline." << std::endl;
		}
		SetVulkanName(renderPlatform, renderPassPipeline->pipeline, platform::core::QuickFormat("%s EffectPass renderPass Pipeline", name.c_str()));
		if (vertexInputs)
			delete[] vertexInputs;
	}
}

EffectPass::RenderPassPipeline& EffectPass::GetRenderPassPipeline(crossplatform::GraphicsDeviceContext& deviceContext)
{
	crossplatform::ContextState* cs = &deviceContext.contextState;
	crossplatform::PixelFormat pixelFormat = vulkan::RenderPlatform::GetActivePixelFormat(deviceContext);
	crossplatform::Topology topology = cs->topology;

	RenderPassHash hashval = MakeRenderPassHash(pixelFormat, topology, cs->currentLayout, blendState, depthStencilState, rasterizerState, multiview);
	const auto& p = m_RenderPasses.find(hashval);
	if (p == m_RenderPasses.end())
	{
		InitializePipeline(deviceContext, &m_RenderPasses[hashval], pixelFormat, topology, blendState, depthStencilState, rasterizerState, multiview);
	}

	return m_RenderPasses[hashval];
}

RenderPassHash EffectPass::GetHash(crossplatform::PixelFormat pixelFormat, crossplatform::Topology topology, const crossplatform::Layout* layout)
{
	RenderPassHash hashval = MakeRenderPassHash(pixelFormat, topology, layout, blendState, depthStencilState, rasterizerState, multiview);
	return hashval;
}

RenderPassHash EffectPass::MakeRenderPassHash(crossplatform::PixelFormat pixelFormat, crossplatform::Topology topology, const crossplatform::Layout* layout,
	const crossplatform::RenderState* blendState, const crossplatform::RenderState* depthStencilState, const crossplatform::RenderState* rasterizerState, bool multiview)
{
	unsigned long long hashval = (unsigned long long)pixelFormat * 1000 + (unsigned long long)topology;
	if (layout)
	{
		const auto& lDesc = layout->GetDesc();
		for (const auto& l : lDesc)
		{
			hashval += ((unsigned long long)l.format) * 3279;
		}
	}
	if (blendState)
	{
		hashval += blendState->desc.blend.AlphaToCoverageEnable ? 2048 : 0;
		hashval += blendState->desc.blend.IndependentBlendEnable ? 4096 : 0;
		hashval += blendState->desc.blend.numRTs * 375;
		for (int i = 0; i < blendState->desc.blend.numRTs; i++)
		{
			hashval += (int)(blendState->desc.blend.RenderTarget[i].blendOperation) * 136;
			hashval += (int)(blendState->desc.blend.RenderTarget[i].blendOperationAlpha) * 754;
			hashval += (int)(blendState->desc.blend.RenderTarget[i].SrcBlend) * 46346;
			hashval += (int)(blendState->desc.blend.RenderTarget[i].DestBlend) * 45684;
			hashval += (int)(blendState->desc.blend.RenderTarget[i].SrcBlendAlpha) * 23472;
			hashval += (int)(blendState->desc.blend.RenderTarget[i].DestBlendAlpha) * 65468;
			hashval += (int)(blendState->desc.blend.RenderTarget[i].RenderTargetWriteMask) * 2374;
		}
	}
	if (depthStencilState)
	{
		hashval += depthStencilState->desc.depth.comparison * 1359781;
		hashval += depthStencilState->desc.depth.test * 135163;
		hashval += depthStencilState->desc.depth.write * 1375;
	}
	if (rasterizerState)
	{
		hashval += rasterizerState->desc.rasterizer.cullFaceMode * 1359781;
		hashval += rasterizerState->desc.rasterizer.frontFace * 4264;
		hashval += rasterizerState->desc.rasterizer.polygonMode * 235;
		hashval += rasterizerState->desc.rasterizer.polygonOffsetMode * 129046;
	}
	if (multiview)
	{
		hashval += 54925873;
	}
	return hashval;
}