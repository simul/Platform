#include "Platform/Vulkan/ShaderBindingTable.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/Effect.h"
#include <vulkan/vulkan.hpp>

using namespace platform;
using namespace vulkan;

ShaderBindingTable::ShaderBindingTable()
	:SBTResources( { 
		{crossplatform::ShaderRecord::Type::RAYGEN, nullptr},
		{crossplatform::ShaderRecord::Type::MISS, nullptr},
		{crossplatform::ShaderRecord::Type::HIT_GROUP, nullptr},
		{crossplatform::ShaderRecord::Type::CALLABLE, nullptr}
	} ) {}

ShaderBindingTable::~ShaderBindingTable()
{
	InvalidateDeviceObjects();
}

void ShaderBindingTable::RestoreDeviceObjects(crossplatform::RenderPlatform* r)
{
	device = reinterpret_cast<vulkan::RenderPlatform*>(r)->AsVulkanDevice();
}

void ShaderBindingTable::InvalidateDeviceObjects()
{
	for (auto& sbtRes : SBTResources)
	{
		device->destroyBuffer(sbtRes.second);
		sbtRes.second = VK_NULL_HANDLE;
	}
	SBTResources.clear();
	SBTResourceAllocations.clear();

	device = nullptr;
}

std::map<crossplatform::ShaderRecord::Type, std::vector<crossplatform::ShaderRecord::Handle>> ShaderBindingTable::GetShaderHandlesFromEffectPass(crossplatform::RenderPlatform* renderPlatform, crossplatform::EffectPass* pass)
{
	std::map<crossplatform::ShaderRecord::Type, std::vector<crossplatform::ShaderRecord::Handle>> result;

	RestoreDeviceObjects(renderPlatform);

#if PLATFORM_SUPPORT_VULKAN_RAYTRACING
	EffectPass* vulkanPass = reinterpret_cast<EffectPass*>(pass);
	if (!vulkanPass)
	{
		SIMUL_BREAK_INTERNAL("No valid EffectPass.");
	}
	else
	{
		// Shader Handles
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties;
		reinterpret_cast<RenderPlatform*>(renderPlatform)->FillPhysicalDeviceProperties2ExtensionStructure(rayTracingPipelineProperties);
		uint32_t vkHandleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
		uint32_t vkHandleSizeAligned = rayTracingPipelineProperties.shaderGroupHandleAlignment;

		const uint32_t& handleSize = vkHandleSize;
		const size_t handleSizeAligned = MakeAlignedSize(vkHandleSize, vkHandleSizeAligned);
		const size_t shaderGroupHandleDataSize = vulkanPass->m_ShaderGroupInfos.size() * handleSizeAligned;

		crossplatform::GraphicsDeviceContext graphicsDeviceContext;
		const EffectPass::RenderPassPipeline& renderPassPipeline = vulkanPass->GetRenderPassPipeline(graphicsDeviceContext);

		shaderGroupHandles.resize(shaderGroupHandleDataSize);
		DispatchLoaderDynamic d;
		d.vkGetRayTracingShaderGroupHandlesKHR = platform::vulkan::vkGetRayTracingShaderGroupHandlesKHR;
		SIMUL_VK_CHECK(device->getRayTracingShaderGroupHandlesKHR(renderPassPipeline.pipeline, 0, static_cast<uint32_t>(vulkanPass->m_ShaderGroupInfos.size()), shaderGroupHandleDataSize, shaderGroupHandles.data(), d));

		// We need to bundles the handles together by type.
		// Get the indices per type.
		size_t raygenCount = 0, missCount = 0, hitCount = 0, callableCount = 0;
		std::map<crossplatform::ShaderRecord::Type, std::vector<size_t>> shaderGroupIndicesPerType;
		size_t idx = 0;
		for (auto& shaderGroupInfo : vulkanPass->m_ShaderGroupInfos)
		{
			if (shaderGroupInfo.type == vk::RayTracingShaderGroupTypeKHR::eGeneral)
			{
				const vk::ShaderStageFlagBits& vkStage = vulkanPass->m_ShaderStageInfos[shaderGroupInfo.generalShader].stage;
				if (vkStage == vk::ShaderStageFlagBits::eRaygenKHR && raygenCount == 0)
				{
					shaderGroupIndicesPerType[crossplatform::ShaderRecord::Type::RAYGEN].push_back(idx);
					raygenCount++;
				}
				else if (vkStage == vk::ShaderStageFlagBits::eMissKHR)
				{
					shaderGroupIndicesPerType[crossplatform::ShaderRecord::Type::MISS].push_back(idx);
					missCount++;
				}
				else if (vkStage == vk::ShaderStageFlagBits::eCallableKHR)
				{
					shaderGroupIndicesPerType[crossplatform::ShaderRecord::Type::CALLABLE].push_back(idx);
					callableCount++;
				}
				else
					continue;
			}
			else
			{
				shaderGroupIndicesPerType[crossplatform::ShaderRecord::Type::HIT_GROUP].push_back(idx);
				hitCount++;
			}
			idx++;
		}

		// Copy Shader handles to the new memory in order.
		for (uint32_t i = 0; i < 4; i++)
		{
			crossplatform::ShaderRecord::Type type = crossplatform::ShaderRecord::Type(i);

			const std::vector<size_t>& shaderGroupIndices = shaderGroupIndicesPerType[type];
			const size_t& handleCount = shaderGroupIndices.size();

			std::vector<crossplatform::ShaderRecord::Handle>& handles = result[type];
			handles.reserve(handleCount);

			for (const size_t& shaderGroupIndex : shaderGroupIndices)
			{
				handles.push_back(shaderGroupHandles.data() + (shaderGroupIndex * handleSize));
			}
		}
	}
#endif

	return result;
}

void ShaderBindingTable::BuildShaderBindingTableResources(crossplatform::RenderPlatform* renderPlatform)
{
#if PLATFORM_SUPPORT_VULKAN_RAYTRACING
	const char* names[4] = {
		"ShaderBinidngTable_RayGen",
		"ShaderBinidngTable_Miss",
		"ShaderBinidngTable_HitGroup",
		"ShaderBinidngTable_Callable"
	};

	for (auto& sbtRes : shaderBindingTableResources)
	{
		if (sbtRes.second.size())
		{
			reinterpret_cast<RenderPlatform*>(renderPlatform)->CreateVulkanBuffer(nullptr,
				static_cast<uint32_t>(sbtRes.second.size()), vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, 
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
				SBTResources[sbtRes.first], SBTResourceAllocations[sbtRes.first], names[(size_t)sbtRes.first]);

			void* mappedData = nullptr;
			SIMUL_VK_CHECK((vk::Result)vmaMapMemory(SBTResourceAllocations[sbtRes.first].allocator, SBTResourceAllocations[sbtRes.first].allocation, &mappedData));
			if (mappedData)
			{
				memcpy(mappedData, sbtRes.second.data(), sbtRes.second.size());
				vmaUnmapMemory(SBTResourceAllocations[sbtRes.first].allocator, SBTResourceAllocations[sbtRes.first].allocation);
			}
		}
	}
#endif
}
