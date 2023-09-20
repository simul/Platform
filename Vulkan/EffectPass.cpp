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

	size_t numDescriptors 
		= collectedResourceSlots.size()
		+ collectedRwResourceSlots.size()
		+ collectedSbResourceSlots.size()
		+ collectedRwSbResourceSlots.size()
		+ collectedSamplerResourceSlots.size()
		+ collectedConstantBufferResourceSlots.size();

	std::vector<vk::WriteDescriptorSet> writes;
	writes.reserve(numDescriptors);

	crossplatform::Slots textureSlots = {};
	crossplatform::Slots rwTextureSlots = {};
	crossplatform::Slots textureSlotsForSB = {};
	crossplatform::Slots rwTextureSlotsForSB = {};
	crossplatform::Slots samplerSlots = {};
	crossplatform::Slots bufferSlots = {};

	std::vector<vk::DescriptorImageInfo> textureInfos;
	textureInfos.reserve(collectedResourceSlots.size());
	for (const int &slot : collectedResourceSlots)
	{
		crossplatform::TextureAssignment& ta = cs->textureAssignmentMap[slot];
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
			if (ta.resourceType == crossplatform::ShaderResourceType::UNKNOWN && res)
			{
				requiredType = res->shaderResourceType;
				ta.resourceType = requiredType;
			}
			texture = ((vulkan::RenderPlatform*)renderPlatform)->GetDummyTexture(ta.resourceType);
			ta.subresource = {};
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
		if (!(texture && texture->IsValid()))
		{
			SIMUL_CERR_ONCE << "Texture is null.\n";
			return;
		}
		texture->FinishLoading(deviceContext);
		texture->SetLayout(deviceContext, vk::ImageLayout::eShaderReadOnlyOptimal, ta.subresource);
		vk::ImageView *vkImageView= texture->AsVulkanImageView({ta.resourceType, ta.subresource});

		vk::WriteDescriptorSet write = {};
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateTextureSlot(slot));
		write.setDstArrayElement(0);
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eSampledImage);
		SIMUL_ASSERT(vkImageView != nullptr);
		if (vkImageView)
		{
			vk::DescriptorImageInfo textureInfo;
			textureInfo.setImageView(*vkImageView);
			textureInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			//Video Texture:
			if (m_VideoSource)
			{
				write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
				textureInfo.setSampler(vulkanRenderPlatform->GetSamplerYcbcr());
			}
			textureInfos.push_back(textureInfo);
			write.setPImageInfo(&textureInfos.back());
		}
		writes.push_back(write);

		textureSlots.set(slot);
	}

	std::vector<vk::DescriptorImageInfo> rwTextureInfos;
	rwTextureInfos.reserve(collectedRwResourceSlots.size());
	for (const int &slot : collectedRwResourceSlots)
	{
		crossplatform::TextureAssignment& ta = cs->rwTextureAssignmentMap[slot];
		vulkan::Texture* texture = (vulkan::Texture*)(ta.texture);
		if (!(texture && texture->IsValid()))
		{
			SIMUL_ASSERT_WARN(!(texture && texture->IsValid()), "No valid buffer for RWTexture. No texture set.");
			continue;
		}
		vk::ImageView *vkImageView = texture->AsVulkanImageView({ta.resourceType, ta.subresource});
		texture->SetLayout(deviceContext, vk::ImageLayout::eGeneral, ta.subresource);
		
		vk::WriteDescriptorSet write = {};
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateTextureWriteSlot(slot));
		write.setDstArrayElement(0);
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageImage);
		SIMUL_ASSERT(vkImageView != nullptr);
		if (vkImageView)
		{
			vk::DescriptorImageInfo rwTextureInfo;
			rwTextureInfo.setImageView(*vkImageView);
			rwTextureInfo.setImageLayout(vk::ImageLayout::eGeneral);
			rwTextureInfos.push_back(rwTextureInfo);
			write.setPImageInfo(&rwTextureInfos.back());
		}
		writes.push_back(write);

		rwTextureSlots.set(slot);
	}

	std::vector<vk::DescriptorBufferInfo> sbInfos;
	sbInfos.reserve(collectedSbResourceSlots.size());
	for (const int &slot : collectedSbResourceSlots)
	{
		crossplatform::PlatformStructuredBuffer* sb = cs->applyStructuredBuffers[slot];
		if (!sb)
		{
			SIMUL_CERR << "No structured buffer found for slot " << slot << " in pass " << name.c_str() << "\n";
			SIMUL_BREAK("No valid buffer for Structured Buffer.");
		}
		vulkan::PlatformStructuredBuffer* psb = (vulkan::PlatformStructuredBuffer*)sb;
		psb->ActualApply(deviceContext, this, slot, false);
		vk::Buffer* vkBuffer = psb->GetLastBuffer();
		
		vk::WriteDescriptorSet write = {};
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateTextureSlot(slot));
		write.setDstArrayElement(0);
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
		SIMUL_ASSERT(vkBuffer != nullptr);
		if (vkBuffer)
		{
			vk::DescriptorBufferInfo sbInfo = {};
			sbInfo.setBuffer(*vkBuffer);
			sbInfo.setOffset(psb->GetLastOffset());
			sbInfo.setRange(psb->GetSize());
			sbInfos.push_back(sbInfo);
			write.setPBufferInfo(&sbInfos.back());
		}
		writes.push_back(write);

		textureSlotsForSB.set(slot);
	}

	std::vector<vk::DescriptorBufferInfo> rwSbInfos;
	rwSbInfos.reserve(collectedRwSbResourceSlots.size());
	for (const int &slot : collectedRwSbResourceSlots)
	{
		crossplatform::PlatformStructuredBuffer* sb = cs->applyRwStructuredBuffers[slot];
		if (!sb)
		{
			SIMUL_ASSERT_WARN(!sb, "No valid buffer for RWStructureBuffer. No buffer set.");
			continue;
		}
		vulkan::PlatformStructuredBuffer* psb = (vulkan::PlatformStructuredBuffer*)sb;
		psb->ActualApply(deviceContext, this, slot, true);
		vk::Buffer* vkBuffer = psb->GetLastBuffer();
		
		vk::WriteDescriptorSet write = {};
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateTextureWriteSlot(slot));
		write.setDstArrayElement(0);
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
		SIMUL_ASSERT(vkBuffer != nullptr);
		if (vkBuffer)
		{
			vk::DescriptorBufferInfo rwSbInfo = {};
			rwSbInfo.setBuffer(*vkBuffer);
			rwSbInfo.setOffset(psb->GetLastOffset());
			rwSbInfo.setRange(psb->GetSize());
			rwSbInfos.push_back(rwSbInfo);
			write.setPBufferInfo(&rwSbInfos.back());
		}
		writes.push_back(write);

		// TODO: this is overkill, synchronizing all uav's
		renderPlatform->ResourceBarrierUAV(deviceContext, sb);
		rwTextureSlotsForSB.set(slot);
	}

	std::vector<vk::DescriptorImageInfo> samplerInfos;
	samplerInfos.reserve(collectedSamplerResourceSlots.size());
	for (const int &slot : collectedSamplerResourceSlots)
	{
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
		
		vk::WriteDescriptorSet write = {};
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateSamplerSlot(slot));
		write.setDstArrayElement(0);
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eSampler);
		SIMUL_ASSERT(vkSampler != nullptr);
		if (vkSampler)
		{
			vk::DescriptorImageInfo samplerInfo;
			samplerInfo.setSampler(*vkSampler);
			samplerInfos.push_back(samplerInfo);
			write.setPImageInfo(&samplerInfos.back());
		}
		writes.push_back(write);

		samplerSlots.set(slot);
	}

	std::vector<vk::DescriptorBufferInfo> constantBufferInfos;
	constantBufferInfos.reserve(collectedConstantBufferResourceSlots.size());
	for (const int &slot : collectedConstantBufferResourceSlots)
	{
		crossplatform::ConstantBufferBase* cb = cs->applyBuffers[slot];
		if (!cb)
		{
			SIMUL_BREAK("No valid buffer for ConstantBuffer.");
		}
		vulkan::PlatformConstantBuffer* pcb = (vulkan::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		pcb->ActualApply(deviceContext, this, 0);
		vk::Buffer* vkBuffer = pcb->GetLastBuffer();

		vk::WriteDescriptorSet write = {};
		write.setDstSet(descriptorSet);
		write.setDstBinding(GenerateConstantBufferSlot(slot));
		write.setDstArrayElement(0);
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
		SIMUL_ASSERT(vkBuffer != nullptr);
		if (vkBuffer)
		{
			vk::DescriptorBufferInfo constantBufferInfo = {};
			constantBufferInfo.setBuffer(*vkBuffer);
			constantBufferInfo.setOffset(pcb->GetLastOffset());
			constantBufferInfo.setRange(pcb->GetSize());
			constantBufferInfos.push_back(constantBufferInfo);
			write.setPBufferInfo(&constantBufferInfos.back());
		}
		writes.push_back(write);

		bufferSlots.set(slot);
	}

	for (const vk::WriteDescriptorSet& write : writes)
	{
		bool no_res = write.pImageInfo == nullptr && write.pBufferInfo == nullptr && write.pTexelBufferView == nullptr;
		if (no_res)
		{
			SIMUL_CERR << "VkWriteDescriptorSet (Binding = " << write.dstBinding << ") in pass '"
				<< name.c_str() << "' has no valid resource associated with it." << std::endl;
			SIMUL_BREAK("VkWriteDescriptorSet error.");
		}
	}
	vulkanDevice->updateDescriptorSets((uint32_t)writes.size(), writes.data(), 0, nullptr);

	// Now verify that ALL resource are set:
	static bool error_checking = true;
	if (error_checking)
	{
		CheckSlots(GetTextureSlots(), textureSlots, 128, "Texture");
		CheckSlots(GetRwTextureSlots(), rwTextureSlots, 16, "RWTexture");
		CheckSlots(GetStructuredBufferSlots(), textureSlotsForSB, 128, "Structured Buffer");
		CheckSlots(GetRwStructuredBufferSlots(), rwTextureSlotsForSB, 16, "RWStructured Buffer");
		CheckSlots(GetSamplerSlots(), samplerSlots, 16, "Sampler");
		CheckSlots(GetConstantBufferSlots(), bufferSlots, 14, "Constant Buffers");
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
			InitializePipeline(*graphicsDeviceContext, renderPassPipeline, crossplatform::PixelFormat::UNKNOWN, 0, crossplatform::Topology::UNDEFINED);
		}
		else
			renderPassPipeline = &(m_RenderPasses[hashval]);
	}
	else
	{
		crossplatform::PixelFormat pixelFormat = crossplatform::PixelFormat::UNKNOWN;
		int numOfSamples = 1;
		pixelFormat =  vulkan::RenderPlatform::GetActivePixelFormat(*graphicsDeviceContext);
		numOfSamples = vulkan::RenderPlatform::GetActiveNumOfSamples(*graphicsDeviceContext);
		crossplatform::Topology appliedTopology = topology;
		if (cs->topology != crossplatform::Topology::UNDEFINED)
			appliedTopology = cs->topology;
		if (appliedTopology == crossplatform::Topology::UNDEFINED)
			appliedTopology = crossplatform::Topology::TRIANGLESTRIP;

		hashval = MakeRenderPassHash(pixelFormat, numOfSamples, appliedTopology, cs->currentLayout, blendState, depthStencilState, rasterizerState, multiview);
		const auto& p = m_RenderPasses.find(hashval);
		if (p == m_RenderPasses.end())
		{
			renderPassPipeline = &m_RenderPasses[hashval];
			InitializePipeline(*graphicsDeviceContext, renderPassPipeline, pixelFormat, numOfSamples, appliedTopology, blendState, depthStencilState, rasterizerState, multiview);
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
				crossplatform::SubresourceRange subres = { tt.subresource.aspectMask, tt.subresource.mipLevel, uint32_t(1), tt.subresource.baseArrayLayer, tt.subresource.arrayLayerCount };
				texture->SetLayout(*graphicsDeviceContext, vk::ImageLayout::eColorAttachmentOptimal, subres);
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
				crossplatform::SubresourceRange subres = { dt.subresource.aspectMask, dt.subresource.mipLevel, uint32_t(1), dt.subresource.baseArrayLayer, dt.subresource.arrayLayerCount };
				vk::ImageLayout layout = depthWrite ? vk::ImageLayout::eDepthStencilAttachmentOptimal : depthTest ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eUndefined;
				texture->SetLayout(*graphicsDeviceContext, layout, subres);
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
	size_t numDescriptors 
		= collectedResourceSlots.size()
		+ collectedRwResourceSlots.size()
		+ collectedSbResourceSlots.size()
		+ collectedRwSbResourceSlots.size()
		+ collectedSamplerResourceSlots.size()
		+ collectedConstantBufferResourceSlots.size()
		+ collectedResourceSlots.size();
	vk::Result result;

	vk::DescriptorSetLayoutBinding* layoutBindings = nullptr;
	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCI;

	if (numDescriptors > 0)
	{
		// Create the "Descriptor Pool":
		{
			vk::DescriptorPoolSize* poolSizes = new vk::DescriptorPoolSize[numDescriptors];
			int poolSizeIdx = 0;
			static int countPerFrame = 1024;
			if (collectedResourceSlots.size())
			{
				vk::DescriptorType type = m_VideoSource ? vk::DescriptorType::eCombinedImageSampler : vk::DescriptorType::eSampledImage; //Video needs Combined image sampler
				poolSizes[poolSizeIdx++].setType(type).setDescriptorCount(countPerFrame * swapchainImageCount * (int)collectedResourceSlots.size());
			}
			if (collectedSamplerResourceSlots.size())
			{
				poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eSampler).setDescriptorCount(countPerFrame * swapchainImageCount * (int)collectedSamplerResourceSlots.size());
			}
			if (collectedSbResourceSlots.size() + collectedRwSbResourceSlots.size())
			{
				poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(countPerFrame * swapchainImageCount * (int)(collectedSbResourceSlots.size() + collectedRwSbResourceSlots.size()));
			}
			if (collectedRwResourceSlots.size())
			{
				poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eStorageImage).setDescriptorCount(countPerFrame * swapchainImageCount * (int)collectedRwResourceSlots.size());
			}
			if (collectedConstantBufferResourceSlots.size())
			{
				poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(countPerFrame * swapchainImageCount * (int)collectedConstantBufferResourceSlots.size());
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
			for (const int &slot : collectedResourceSlots)
			{
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
				bindingIndex++;
			}
			for (const int &slot : collectedRwResourceSlots)
			{
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesRwTextureSlot);
				binding.setBinding(GenerateTextureWriteSlot(slot))
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
				bindingIndex++;
			}
			for (const int &slot : collectedSbResourceSlots)
			{
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesTextureSlotForSB);
				binding.setBinding(GenerateTextureSlot(slot))
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
				bindingIndex++;
			}
			for (const int &slot : collectedRwSbResourceSlots)
			{
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesRwTextureSlotForSB);
				binding.setBinding(GenerateTextureWriteSlot(slot))
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
				bindingIndex++;
			}
			for (const int &slot : collectedSamplerResourceSlots)
			{
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesSamplerSlot);
				binding.setBinding(GenerateSamplerSlot(slot))
					.setDescriptorType(vk::DescriptorType::eSampler)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
				bindingIndex++;
			}
			for (const int &slot : collectedConstantBufferResourceSlots)
			{
				vk::DescriptorSetLayoutBinding& binding = layoutBindings[bindingIndex];
				vk::ShaderStageFlags stageFlags = GetShaderFlagsForSlot(slot, &Shader::usesConstantBufferSlot);
				binding.setBinding(GenerateConstantBufferSlot(slot))
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
				bindingIndex++;
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

void EffectPass::InitializePipeline(crossplatform::GraphicsDeviceContext& deviceContext, RenderPassPipeline* renderPassPipeline, crossplatform::PixelFormat pixelFormat, int numOfSamples,
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
			depthFormat, depth_read, depth_write, false, numOfSamples, multiview);
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
		vk::PipelineMultisampleStateCreateInfo multisampleInfo = vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(vk::SampleCountFlagBits(numOfSamples));
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
	int numOfSamples = vulkan::RenderPlatform::GetActiveNumOfSamples(deviceContext);
	crossplatform::Topology topology = cs->topology;

	RenderPassHash hashval = MakeRenderPassHash(pixelFormat, numOfSamples,topology, cs->currentLayout, blendState, depthStencilState, rasterizerState, multiview);
	const auto& p = m_RenderPasses.find(hashval);
	if (p == m_RenderPasses.end())
	{
		InitializePipeline(deviceContext, &m_RenderPasses[hashval], pixelFormat, numOfSamples, topology, blendState, depthStencilState, rasterizerState, multiview);
	}

	return m_RenderPasses[hashval];
}

RenderPassHash EffectPass::GetHash(crossplatform::PixelFormat pixelFormat, int numOfSamples, crossplatform::Topology topology, const crossplatform::Layout* layout)
{
	RenderPassHash hashval = MakeRenderPassHash(pixelFormat, numOfSamples, topology, layout, blendState, depthStencilState, rasterizerState, multiview);
	return hashval;
}

RenderPassHash EffectPass::MakeRenderPassHash(crossplatform::PixelFormat pixelFormat, int numOfSamples, crossplatform::Topology topology, const crossplatform::Layout* layout,
	const crossplatform::RenderState* blendState, const crossplatform::RenderState* depthStencilState, const crossplatform::RenderState* rasterizerState, bool multiview)
{
	unsigned long long hashval = (unsigned long long)pixelFormat * 1000 + (unsigned long long)numOfSamples * 10 + (unsigned long long)topology;
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