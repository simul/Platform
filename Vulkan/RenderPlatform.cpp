﻿//This file alone implements VMA, include only the header file elsewhere.
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/Texture.h"
#include "Platform/Vulkan/Effect.h"
#include "Platform/Vulkan/Buffer.h"
#include "Platform/Vulkan/Framebuffer.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Core/DefaultFileLoader.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/Vulkan/Texture.h"
#include "Platform/Vulkan/DisplaySurface.h"
#include "DeviceManager.h"
#include "Platform/Core/StringFunctions.h"
#include <vulkan/vulkan.hpp>
#include <cstdint>

#pragma optimize("", off)
#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

bool platform::vulkan::debugUtilsSupported=false;
bool platform::vulkan::debugMarkerSupported=false;

namespace platform::vulkan
{
	static PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = nullptr;
	static PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT = nullptr;
	static PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT = nullptr;
	static PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT = nullptr;
}

using namespace platform;
using namespace vulkan;

RenderPlatform::RenderPlatform():
	mDummy2D(nullptr)
	,mDummy3D(nullptr)
{
	mirrorY	 = false;
	mirrorY2	= false;
	mirrorYText = false;
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

vk::Device *RenderPlatform::AsVulkanDevice()
{
	return vulkanDevice;
}

const char* RenderPlatform::GetName()const
{
	return "Vulkan";
}

void RenderPlatform::RestoreDeviceObjects(void *vkDevice_vkInstance_gpu)
{
	ERRNO_BREAK
	void **ptr = (void **)vkDevice_vkInstance_gpu;
	vulkanDevice = (vk::Device *)ptr[0];
	vulkanInstance = (vk::Instance *)ptr[1];
	vulkanGpu = (vk::PhysicalDevice *)ptr[2];
	immediateContext.platform_context = nullptr;

	//Set up VMA CPU and GPU allicators
	VmaAllocatorCreateInfo allocatorCreateInfo;
	allocatorCreateInfo.flags = 0;
	allocatorCreateInfo.physicalDevice = *vulkanGpu;
	allocatorCreateInfo.device = *vulkanDevice;
	allocatorCreateInfo.preferredLargeHeapBlockSize = mCPUPreferredBlockSize = 256 * 1048576;
	allocatorCreateInfo.pAllocationCallbacks = nullptr;
	allocatorCreateInfo.pDeviceMemoryCallbacks = nullptr;
	allocatorCreateInfo.pHeapSizeLimit = nullptr;
	allocatorCreateInfo.pVulkanFunctions = nullptr;
	allocatorCreateInfo.instance = *vulkanInstance;
	allocatorCreateInfo.vulkanApiVersion = 0;
	allocatorCreateInfo.pTypeExternalMemoryHandleTypes = nullptr;
	SIMUL_VK_CHECK((vk::Result)vmaCreateAllocator(&allocatorCreateInfo, &mCPUAllocator));

	allocatorCreateInfo.preferredLargeHeapBlockSize = mGPUPreferredBlockSize = 256 * 1048576;
	SIMUL_VK_CHECK((vk::Result)vmaCreateAllocator(&allocatorCreateInfo, &mGPUAllocator));

	crossplatform::RenderPlatform::RestoreDeviceObjects(nullptr);

	// Load debug markers PFNs.
	if (debugUtilsSupported)
	{
		vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vulkanInstance->getProcAddr("vkCmdBeginDebugUtilsLabelEXT");
		vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vulkanInstance->getProcAddr("vkCmdEndDebugUtilsLabelEXT");
	}
	if (debugMarkerSupported)
	{
		vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)vulkanInstance->getProcAddr("vkCmdDebugMarkerBeginEXT");
		vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)vulkanInstance->getProcAddr("vkCmdDebugMarkerEndEXT");
	}

	// Check feature support.
	renderingFeatures = crossplatform::RenderingFeatures::None;
	if (CheckDeviceExtension(VK_KHR_MULTIVIEW_EXTENSION_NAME))
		renderingFeatures = (crossplatform::RenderingFeatures)((uint32_t)renderingFeatures | (uint32_t)crossplatform::RenderingFeatures::Multiview);


	// Vulkan Video decoding support:
#if PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR
	if (CheckDeviceExtension(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME))
	{
		vk::SamplerCreateInfo samplerCreateInfo = vk::SamplerCreateInfo();
		samplerCreateInfo
			.setMagFilter(vk::Filter::eNearest)
			.setMinFilter(vk::Filter::eNearest)
			.setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
			.setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
			.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
			.setAnisotropyEnable(false)
			.setUnnormalizedCoordinates(false)
			.setPNext(GetSamplerYcbcrConversionInfo());
		SIMUL_VK_CHECK(vulkanDevice->createSampler(&samplerCreateInfo, nullptr, &vulkanSamplerYcbcr));
		SetVulkanName(this, vulkanSamplerYcbcr, "Vulkan Sampler Ycbcr");
	}
#endif

	// Create the three shared resource group layouts
	size_t totalNumDescriptors=0;
	for(uint8_t i=0;i<crossplatform::PER_PASS_RESOURCE_GROUP;i++)
	{
		int countPerFrame = resourceGroupCountPerFrame[i];
		CreateDescriptorPool(i, countPerFrame);
		const crossplatform::ResourceGroupLayout &resourceGroupLayout = resourceGroupLayouts[i];
		size_t numConstantBufferResourceSlots = resourceGroupLayout.GetNumConstantBuffers();
		size_t numReadOnlyResourceSlots = resourceGroupLayout.GetNumReadOnlyResources();
		size_t numDescriptors = resourceGroupLayout.GetNumResources();

		vk::Result result;
		vk::DescriptorSetLayoutBinding *layoutBindings = nullptr;
		vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCI;

		if (numDescriptors > 0)
		{
		// Set up the "Descriptor Set Layout CreateInfo":
			layoutBindings = new vk::DescriptorSetLayoutBinding[numDescriptors];
			
			int slot=0;
			int binding_index=0;
			for (int j = 0; j < numConstantBufferResourceSlots; j++, binding_index++)
			{
				while(slot<64&&!resourceGroupLayout.UsesConstantBufferSlot(slot))
					slot++;
				if(slot>=64)
					break;
				vk::DescriptorSetLayoutBinding &binding = layoutBindings[j];
				vk::ShaderStageFlags stageFlags = (vk::ShaderStageFlags)(VK_SHADER_STAGE_ALL);
				binding.setBinding(GenerateConstantBufferSlot(slot))
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
				slot++;
			}
			slot = 0;
			for (int j = 0; j < numReadOnlyResourceSlots; j++,binding_index++)
			{
				while (slot < 64 && !resourceGroupLayout.UsesReadOnlyResourceSlot(slot))
					slot++;
				if (slot >= 64)
					break;
				vk::DescriptorSetLayoutBinding &binding = layoutBindings[binding_index];
				vk::ShaderStageFlags stageFlags = (vk::ShaderStageFlags)(VK_SHADER_STAGE_ALL);
				binding.setBinding(GenerateTextureSlot(slot))
					.setDescriptorType(vk::DescriptorType::eSampledImage)
					.setDescriptorCount(1)
					.setStageFlags(stageFlags)
					.setPImmutableSamplers(nullptr);
				slot++;
			}

			descriptorSetLayoutCI.setBindingCount((uint32_t)numDescriptors).setPBindings(layoutBindings);
		}

		// Create the "Descriptor Set Layout":
		result = vulkanDevice->createDescriptorSetLayout(&descriptorSetLayoutCI, nullptr, &descriptorSetLayouts[i]);
		SIMUL_VK_CHECK(result);
		SetVulkanName(this, descriptorSetLayouts[i], fmt::format("Descriptor layout for octave {0}", i));
	}
	for (int g= 0; g < 3; g++)
	for (int i = 0; i < s_DescriptorSetCount; i++)
		m_DescriptorSets_It[g][i] = m_DescriptorSets[g][i].begin();
}

void RenderPlatform::CreateDescriptorPool(int g, int countPerFrame)
{
	const crossplatform::ResourceGroupLayout &resourceGroupLayout = resourceGroupLayouts[g];
	size_t numConstantBufferResourceSlots = resourceGroupLayout.GetNumConstantBuffers();
	size_t numReadOnlyResourceSlots = resourceGroupLayout.GetNumReadOnlyResources();
	size_t numDescriptors = resourceGroupLayout.GetNumResources();
	if (numDescriptors > 0)
	{
		vk::DescriptorPoolSize *poolSizes = new vk::DescriptorPoolSize[numDescriptors];
		int poolSizeIdx = 0;
		if (numConstantBufferResourceSlots)
		{
			poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(countPerFrame * swapchainImageCount * (int)numConstantBufferResourceSlots);
		}
		if (numReadOnlyResourceSlots)
		{
			poolSizes[poolSizeIdx++].setType(vk::DescriptorType::eSampledImage).setDescriptorCount(countPerFrame * swapchainImageCount * (int)numReadOnlyResourceSlots);
		}
		const vk::DescriptorPoolCreateInfo descriptorPoolCI = vk::DescriptorPoolCreateInfo().setMaxSets(swapchainImageCount * countPerFrame).setPoolSizeCount(poolSizeIdx).setPPoolSizes(poolSizes);
		vk::Result result = vulkanDevice->createDescriptorPool(&descriptorPoolCI, nullptr, &mDescriptorPools[g]);
		SIMUL_VK_CHECK(result);
		SetVulkanName(this, mDescriptorPools[g], fmt::format("Descriptor pool octave {0}", g));
		delete[] poolSizes;
	}
}

vk::SamplerYcbcrConversionInfo* RenderPlatform::GetSamplerYcbcrConversionInfo()
{
#if PLATFORM_SUPPORT_VULKAN_SAMPLER_YCBCR
	static bool init=false;
	if(!init && CheckDeviceExtension(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME))
	{
		init=true;
		vk::SamplerYcbcrConversionCreateInfo samplerYcbcrConversionCreateInfo;
		samplerYcbcrConversionCreateInfo.setFormat(vk::Format::eUndefined);
		samplerYcbcrConversionCreateInfo.setYcbcrModel(vk::SamplerYcbcrModelConversion::eYcbcr709);
		samplerYcbcrConversionCreateInfo.setYcbcrRange(vk::SamplerYcbcrRange::eItuNarrow);
		samplerYcbcrConversionCreateInfo.setXChromaOffset(vk::ChromaLocation::eMidpoint);
		samplerYcbcrConversionCreateInfo.setYChromaOffset(vk::ChromaLocation::eMidpoint);
		vk::ComponentMapping comp(vk::ComponentSwizzle::eIdentity,vk::ComponentSwizzle::eIdentity,vk::ComponentSwizzle::eIdentity,vk::ComponentSwizzle::eIdentity);
		samplerYcbcrConversionCreateInfo.setComponents(comp);
		samplerYcbcrConversionCreateInfo.setForceExplicitReconstruction(false);
		samplerYcbcrConversionCreateInfo.setChromaFilter(vk::Filter::eNearest);

	#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		vk::ExternalFormatANDROID externalFormat;
		externalFormat.externalFormat = 506;
		if (CheckDeviceExtension(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME))
			samplerYcbcrConversionCreateInfo.setPNext(&externalFormat);
	#endif

		vk::SamplerYcbcrConversion samplerYcbcrConversion = vulkanDevice->createSamplerYcbcrConversion(samplerYcbcrConversionCreateInfo);
		samplerYcbcrConversionInfo.conversion = samplerYcbcrConversion;
	}
#endif
	return &samplerYcbcrConversionInfo;
}

void RenderPlatform::InvalidateDeviceObjects()
{
	if (!vulkanDevice)
		return;
	
	vulkanDevice->waitIdle();

	for (int g = 0; g < 3; g++)
	for (int i = 0; i < s_DescriptorSetCount; i++)
	{
		m_DescriptorSets[g][i].clear();
		m_DescriptorSets_It[g][i] = m_DescriptorSets[g][i].begin();
	}

	for (auto& i : mFramebuffers)
	{
		vulkanDevice->destroyFramebuffer(i.second, nullptr);
	}
	mFramebuffers.clear();
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
	SAFE_DELETE(mDummy2D);
	SAFE_DELETE(mDummy2DArray)
	SAFE_DELETE(mDummy2DMS)
	SAFE_DELETE(mDummy3D);
	SAFE_DELETE(mDummyTextureCube)
	SAFE_DELETE(mDummyTextureCubeArray)
	for(int g=0;g<3;g++)
	{
		vulkanDevice->destroyDescriptorPool(mDescriptorPools[g], nullptr);
	}

	ClearReleaseManager(true);

	vmaDestroyAllocator(mGPUAllocator);
	vmaDestroyAllocator(mCPUAllocator);

	vulkanDevice = nullptr;
}

vk::Instance *RenderPlatform::AsVulkanInstance()
{
	return vulkanInstance;
}

vk::PhysicalDevice *RenderPlatform::GetVulkanGPU()
{
	return vulkanGpu;
}

uint32_t RenderPlatform::GetInstanceAPIVersion()
{
	uint32_t apiVersion = VK_API_VERSION_1_0;
	uint32_t instanceVersion = 0;
	vk::Result result = vk::enumerateInstanceVersion(&instanceVersion);
	if (result == vk::Result::eSuccess && instanceVersion != 0)
		apiVersion = instanceVersion;

	return apiVersion;
}

uint32_t RenderPlatform::GetPhysicalDeviceAPIVersion()
{
	uint32_t apiVersion = VK_API_VERSION_1_0;
	if (vulkanGpu)
	{
		vk::PhysicalDeviceProperties properties = vulkanGpu->getProperties();
		apiVersion = properties.apiVersion;
	}
	return apiVersion;
}

bool RenderPlatform::CheckInstanceExtension(const std::string& instanceExtensionName)
{
	if (instanceExtensionName.empty() || deviceManager == nullptr)
		return false;

	for (const auto& instance_extension_name : deviceManager->instance_extension_names)
	{
		if (instance_extension_name.compare(instanceExtensionName) == 0)
			return true;
	}
	return false;
}

bool RenderPlatform::CheckDeviceExtension(const std::string& deviceExtensionName)
{
	if (deviceExtensionName.empty() || deviceManager == nullptr)
		return false;

	for (const auto& device_extension_name : deviceManager->device_extension_names)
	{
		if (device_extension_name.compare(deviceExtensionName) == 0)
			return true;
	}
	return false;
}

static vk::CommandPool cmdPool;
static vk::CommandBuffer cmdBuffer;

void RenderPlatform::ExecuteCommands(crossplatform::DeviceContext& deviceContext)
{
	vk::CommandBuffer* cmdBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	cmdBuffer->end();

	vk::FenceCreateInfo fenceCI;
	fenceCI.setPNext(nullptr).setFlags(vk::FenceCreateFlags(0));
	vk::Fence fence = vulkanDevice->createFence(fenceCI);

	vk::SubmitInfo si;
	si.setWaitSemaphoreCount(0).setPWaitSemaphores(nullptr)
		.setWaitDstStageMask(nullptr)
		.setCommandBufferCount(1).setPCommandBuffers(cmdBuffer)
		.setSignalSemaphoreCount(0).setPSignalSemaphores(nullptr)
		.setPNext(nullptr);
	vk::Queue queue = vulkanDevice->getQueue(0, 0);
	SIMUL_VK_CHECK(queue.submit(1, &si, fence));
	SIMUL_VK_CHECK(vulkanDevice->waitForFences(1, &fence, true, UINT64_MAX));
}

void RenderPlatform::RestartCommands(crossplatform::DeviceContext& deviceContext)
{
	vk::CommandBuffer* cmdBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	cmdBuffer->reset(vk::CommandBufferResetFlagBits::eReleaseResources);
	cmdBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));

}

void RenderPlatform::ClearReleaseManager(bool force)
{
	auto it = releaseResources.begin();
	while (it != releaseResources.end())
	{
		if (it->releaseFrame < uint64_t(frameNumber) || force)
		{
			const uint64_t &i = it->resourceHandle;
			switch (it->type)
			{
			case vk::ObjectType::eBuffer:
			{
				if (it->allocator && it->allocation)
					vmaDestroyBuffer(it->allocator, *(vk::Buffer *)&i, it->allocation);
				else
					vulkanDevice->destroyBuffer(*(vk::Buffer *)&i, nullptr);

				break;
			}
			case vk::ObjectType::eBufferView:
				vulkanDevice->destroyBufferView(*(vk::BufferView *)&i, nullptr); break;
			case vk::ObjectType::eDeviceMemory:
				vulkanDevice->freeMemory(*(vk::DeviceMemory *)&i, nullptr); break;
			case vk::ObjectType::eImageView:
				vulkanDevice->destroyImageView(*(vk::ImageView *)&i, nullptr); break;
			case vk::ObjectType::eFramebuffer:
				vulkanDevice->destroyFramebuffer(*(vk::Framebuffer *)&i, nullptr); break;
			case vk::ObjectType::eRenderPass:
				vulkanDevice->destroyRenderPass(*(vk::RenderPass *)&i, nullptr); break;
			case vk::ObjectType::eImage:
			{
				if (it->allocator && it->allocation)
					vmaDestroyImage(it->allocator, *(vk::Image *)&i, it->allocation);
				else
					vulkanDevice->destroyImage(*(vk::Image*)&i, nullptr);

				break;
			}
			case vk::ObjectType::eSampler:
				vulkanDevice->destroySampler(*(vk::Sampler *)&i, nullptr); break;
			case vk::ObjectType::ePipeline:
				vulkanDevice->destroyPipeline(*(vk::Pipeline *)&i, nullptr); break;
			case vk::ObjectType::ePipelineCache:
				vulkanDevice->destroyPipelineCache(*(vk::PipelineCache *)&i, nullptr); break;
			case vk::ObjectType::ePipelineLayout:
				vulkanDevice->destroyPipelineLayout(*(vk::PipelineLayout *)&i, nullptr); break;
			case vk::ObjectType::eDescriptorSetLayout:
				vulkanDevice->destroyDescriptorSetLayout(*(vk::DescriptorSetLayout *)&i, nullptr); break;
			case vk::ObjectType::eDescriptorPool:
				vulkanDevice->destroyDescriptorPool(*(vk::DescriptorPool *)&i, nullptr); break;
			default:
				SIMUL_BREAK("Unknown vk::ObjectType of vk::ObjectType::e{} (0x{}) in ReleaseManager.", vk::to_string(it->type), i);
			}

			it = releaseResources.erase(it);
		}
		else
		{
			it++;
		}
	}

	resourcesToBeReleased = !releaseResources.empty();
}

void RenderPlatform::BeginFrame()
{
	if (frame_started)
		return;
	crossplatform::RenderPlatform::BeginFrame();
	auto *vulkanDevice=AsVulkanDevice();
	//vulkanDevice->waitForFences(1, &deviceManagerInternal->fences[frame_index], VK_TRUE, UINT64_MAX);
	//vulkanDevice->resetFences(1, &deviceManagerInternal->fences[frame_index]);
}

void RenderPlatform::EndFrame()
{
	crossplatform::RenderPlatform::EndFrame();
}

void RenderPlatform::CopyTexture(crossplatform::DeviceContext& deviceContext, crossplatform::Texture *dest, crossplatform::Texture *source)
{
	auto *commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	immediateContext.platform_context = deviceContext.platform_context;

	auto src = (vulkan::Texture*)source;
	auto dst = (vulkan::Texture*)dest;
	if (!src || !dst)
	{
		SIMUL_CERR << "Passed a null texture to CopyTexture(), ignoring call.\n";
		return;
	}

	// Ensure textures are compatible
	if ((source->width != dest->width) ||
		(source->length != dest->length) ||
		(source->arraySize != dest->arraySize) ||
		(source->mips != dest->mips))
	{
		SIMUL_CERR << "Passed incompatible textures to CopyTexture(), both textures should have same width,height,depth and mip level, ignoring call.\n";
		return;
	}

	// Ensure source state
	bool changedSrc = false;
	std::vector<vk::ImageCopy> copyRegions;
	uint32_t offset = 0;

	{
		int w = dest->width;
		int l = dest->length;
		int d = dest->depth;
		for (int j = 0; j < dest->mips; j++)
		{
			vk::ImageCopy copyRegion = {};
			copyRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
			copyRegion.srcSubresource.mipLevel = j;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.layerCount = dest->arraySize*(dest->IsCubemap()?6:1);
			copyRegion.srcOffset = vk::Offset3D(0,0,0);

			copyRegion.dstSubresource = copyRegion.srcSubresource;

			copyRegion.extent.width = w;
			copyRegion.extent.height = l;
			copyRegion.extent.depth = d;

			copyRegions.push_back(copyRegion);
			w = (w + 1) / 2;
			l = (l + 1) / 2;
			d = (d + 1) / 2;
		}
	}

	src->SetLayout(deviceContext, vk::ImageLayout::eTransferSrcOptimal, {});
	dst->SetLayout(deviceContext, vk::ImageLayout::eTransferDstOptimal, {});
	// Perform the copy. This is done GPU side and does not incur much CPU overhead (if copying full resources)
	commandBuffer->copyImage(*src->AsVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, *dst->AsVulkanImage(), vk::ImageLayout::eTransferDstOptimal
													,static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
}

float RenderPlatform::GetDefaultOutputGamma() const 
{
	static float g=1.0f;
	return g;
}

void RenderPlatform::BeginEvent(crossplatform::DeviceContext& deviceContext, const char* name)
{
	if (debugUtilsSupported)
	{
		vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
		vk::DebugUtilsLabelEXT labelInfo;
		labelInfo.pNext = nullptr;
		labelInfo.pLabelName = name;
		labelInfo.color[0] = labelInfo.color[1] = labelInfo.color[2] = labelInfo.color[3] = 1.0f;

		vk::DispatchLoaderDynamic d;
		d.vkCmdBeginDebugUtilsLabelEXT = vkCmdBeginDebugUtilsLabelEXT;
		commandBuffer->beginDebugUtilsLabelEXT(&labelInfo, d);
	}
	if (debugMarkerSupported)
	{
		vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
		vk::DebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.pNext = nullptr;
		markerInfo.pMarkerName = name;
		markerInfo.color[0] = markerInfo.color[1] = markerInfo.color[2] = markerInfo.color[3] = 1.0f;

		vk::DispatchLoaderDynamic d;
		d.vkCmdDebugMarkerBeginEXT = vkCmdDebugMarkerBeginEXT;
		commandBuffer->debugMarkerBeginEXT(markerInfo, d);
	}
}

void RenderPlatform::EndEvent(crossplatform::DeviceContext& deviceContext)
{
	if (debugUtilsSupported)
	{
		vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
		vk::DispatchLoaderDynamic d;
		d.vkCmdEndDebugUtilsLabelEXT = vkCmdEndDebugUtilsLabelEXT;
		commandBuffer->endDebugUtilsLabelEXT(d);
	}
	if (debugMarkerSupported)
	{
		vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
		vk::DispatchLoaderDynamic d;
		d.vkCmdDebugMarkerEndEXT = vkCmdDebugMarkerEndEXT;
		commandBuffer->debugMarkerEndEXT(d);
	}
}

void RenderPlatform::DispatchCompute(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (!commandBuffer)
		return;
#if SIMUL_INTERNAL_CHECKS
	if(w*l*d<=0)
	{
		SIMUL_BREAK_ONCE("Empty compute dispatch");
	}
#endif
	EndRenderPass(deviceContext);

	vulkan::EffectPass* vkEffectPass = ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass);
	BeginEvent(deviceContext, vkEffectPass->name.c_str());
	ApplyContextState(deviceContext);
	commandBuffer->dispatch(w, l, d);
	InsertFences(deviceContext);
	EndEvent(deviceContext);
}

void RenderPlatform::ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex)
{
	EndRenderPass(deviceContext);

	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (commandBuffer)
	{
		vk::MemoryBarrier barrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead);
		commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTopOfPipe,
			vk::DependencyFlags(0), { barrier }, {}, {});
	}
}

void RenderPlatform::ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::PlatformStructuredBuffer* sb)
{
	EndRenderPass(deviceContext);

	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (commandBuffer)
	{
		vk::MemoryBarrier barrier = {};
		barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		vk::DependencyFlags flags = vk::DependencyFlagBits::eDeviceGroup;

		commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
			flags, { barrier }, {}, {});
	}
}
//Intra-commandbuffer synchronisations https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples

void RenderPlatform::DrawQuad(crossplatform::GraphicsDeviceContext& deviceContext)   
{
	if(!deviceContext.contextState.currentEffectPass)
		return;

	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (!commandBuffer)
		return;

	//BeginEvent(deviceContext, ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
	SetTopology(deviceContext, crossplatform::Topology::TRIANGLESTRIP);
	ApplyContextState(deviceContext);
	commandBuffer->draw(4,1,0,0);
	SetTopology(deviceContext, crossplatform::Topology::UNDEFINED);
	//EndEvent(deviceContext);
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext &deviceContext,bool error_checking)
{
	crossplatform::ContextState* cs = &deviceContext.contextState;
	vulkan::EffectPass* pass = (vulkan::EffectPass*)cs->currentEffectPass;

	if (!cs || !cs->currentEffectPass)
	{
		SIMUL_BREAK("No valid shader pass in ApplyContextState");
		return false;
	}
	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (!commandBuffer)
		return false;

	if (!cs->effectPassValid)
	{
		if (cs->last_action_was_compute && pass->shaders[crossplatform::SHADERTYPE_VERTEX] != nullptr)
			cs->last_action_was_compute = false;
		else if ((!cs->last_action_was_compute) && pass->shaders[crossplatform::SHADERTYPE_COMPUTE] != nullptr)
			cs->last_action_was_compute = true;
		cs->effectPassValid = true;
	}

	// Update frame_number in the DeviceContext from the RenderPlatform.
	if (last_begin_frame_number != GetFrameNumber())
	{
		// Call start render at least once per frame to make sure the bins release objects!
		ContextFrameBegin(*deviceContext.AsGraphicsDeviceContext());
		descriptorSetFrame++;
		descriptorSetFrame = descriptorSetFrame % (s_DescriptorSetCount);
		for(int g=0;g<3;g++)
		m_DescriptorSets_It[g][descriptorSetFrame] = m_DescriptorSets[g][descriptorSetFrame].begin();
	}
	if (frameNumber != mLastFrame && *commandBuffer != cmdBuffer) //Check this VkCommandBuffer is not the one used of the ImmediateContext - AJR
	{
		//Make sure that any resources set for deletion are removed
		if (resourcesToBeReleased)
			ClearReleaseManager();

		mLastFrame = frameNumber;
	}
	bool is_compute = pass->shaders[crossplatform::SHADERTYPE_COMPUTE] != nullptr;

	// Apply the pass setting up the descriptors, pipeline and render pass.
	pass->Apply(deviceContext, false);
	crossplatform::GraphicsDeviceContext* graphicsDeviceContext = deviceContext.AsGraphicsDeviceContext();
	const EffectPass::RenderPassPipeline &renderPassPipeline = pass->GetRenderPassPipeline(*graphicsDeviceContext);

	vk::DescriptorSet descriptorSets[4];
	int8_t maxDescriptorSet = -1;
	int8_t minDescriptorSet = 127;
	uint32_t *resourceGroupAppliedCtr = is_compute ? cs->resourceGroupAppliedCounterCompute : cs->resourceGroupAppliedCounter;
	for(uint8_t g=0;g<crossplatform::PER_PASS_RESOURCE_GROUP;g++)
	{
		if (resourceGroupAppliedCtr[g] != cs->resourceGroupApplyCounter[g])
		{
			vk::DescriptorSet *d = GetDescriptorSetForResourceGroup(deviceContext, g);
			if (d)
			{
				descriptorSets[g] = *d;
				minDescriptorSet = std::min(int8_t(g), minDescriptorSet);
				maxDescriptorSet = std::max(int8_t(g), maxDescriptorSet);
			}
			else return false;
		}
	}

	const vk::DescriptorSet &descriptorSet = pass->GetLatestDescriptorSet();
	bool usesPerPassResources = pass->UsesResourceLayout(crossplatform::PER_PASS_RESOURCE_GROUP);
	if (usesPerPassResources)
	{
		maxDescriptorSet = crossplatform::PER_PASS_RESOURCE_GROUP;
		descriptorSets[crossplatform::PER_PASS_RESOURCE_GROUP] = descriptorSet;
		minDescriptorSet = std::min(int8_t(crossplatform::PER_PASS_RESOURCE_GROUP), minDescriptorSet);
	}

	const vk::PipelineLayout &pipelineLayout = pass->GetLatestPipelineLayout();
	if (maxDescriptorSet >= minDescriptorSet)
	{
		// Set the Descriptor Sets
		for(uint8_t g=minDescriptorSet;g<=maxDescriptorSet;g++)
		{
			if(descriptorSets[g].operator VkDescriptorSet())
				commandBuffer->bindDescriptorSets(is_compute ? vk::PipelineBindPoint::eCompute: vk::PipelineBindPoint::eGraphics, pipelineLayout, g, 1, &(descriptorSets[g]), 0, nullptr);
		}
		for (int8_t g = minDescriptorSet; g < maxDescriptorSet + 1; g++)
		{
			resourceGroupAppliedCtr[g] = cs->resourceGroupApplyCounter[g];
		}
	}

	// If not a compute shader, apply viewports:
	if (!is_compute)
	{
		for (auto i : cs->applyVertexBuffers)
		{
			vulkan::Buffer* buffer = (vulkan::Buffer*)i.second;
			buffer->FinishLoading(deviceContext);
		}
		if (cs->indexBuffer)
		{
			vulkan::Buffer* buffer = (vulkan::Buffer*)cs->indexBuffer;
			buffer->FinishLoading(deviceContext);
		}

		//Set up clear colours
		crossplatform::TargetsAndViewport* tv = graphicsDeviceContext->GetCurrentTargetsAndViewport();
		vk::Extent2D extent = GetTargetAndViewportExtext2D(tv);
		size_t clearColoursCount = (size_t)tv->num + (tv->depthTarget.texture != nullptr ? 1 : 0);
		vk::ClearColorValue colourClear(std::array<float, 4>({ {0.0f, 0.0f, 0.0f, 0.0f} }));
		vk::ClearDepthStencilValue depthClear(0.0f, 0u);
		std::vector<vk::ClearValue>clearValues(clearColoursCount);
		for (size_t i = 0; i < clearColoursCount; i++)
		{
			if (i == clearColoursCount - 1 && tv->depthTarget.texture != nullptr)
				clearValues[i] = depthClear;
			else
				clearValues[i] = colourClear;
		}
		if (clearColoursCount == 0)
		{
			clearColoursCount = 2;
			clearValues.resize(clearColoursCount);
			clearValues[0] = colourClear;
			clearValues[1] = depthClear;
		}

		//Begin the RenderPass
		crossplatform::Viewport vp = { 0, 0, static_cast<int>(extent.width), static_cast<int>(extent.height) };
		if (extent.width * extent.height == 0)
			vp = GetViewport(*graphicsDeviceContext, 0);
		static vk::Framebuffer currentFramebuffer{};
		vk::Framebuffer framebuffer = *GetCurrentVulkanFramebuffer(*graphicsDeviceContext);
		if (framebuffer != currentFramebuffer && deviceContext.contextState.vulkanInsideRenderPass) //Check for a change of framebuffer.
		{
			EndRenderPass(deviceContext);
		}
		currentFramebuffer = framebuffer;

		if (!deviceContext.contextState.vulkanInsideRenderPass)
		{
			vk::Rect2D renderArea(vk::Offset2D(0, 0), vk::Extent2D((uint32_t)vp.w, (uint32_t)vp.h));
			vk::RenderPassBeginInfo renderPassBeginInfo = vk::RenderPassBeginInfo()
				.setRenderPass(renderPassPipeline.renderPass)
				.setFramebuffer(framebuffer)
				.setClearValueCount((uint32_t)clearColoursCount)
				.setPClearValues(clearValues.data())
				.setRenderArea(renderArea);
			commandBuffer->beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);
			//std::cout << "Begun renderpass\n";
			deviceContext.contextState.vulkanInsideRenderPass = true;
		}

		//Set Graphics Pipeline
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, renderPassPipeline.pipeline);

		//Set the Viewports and Scissors
		if(cs->viewportsChanged)
		{
			vk::Viewport vkViewports[1];
			for (int i = 0; i < 1; i++)
			{
				crossplatform::Viewport& _vp = cs->viewports[i];
				if (!(_vp.w * _vp.h))
					_vp = vp;
				if (!(_vp.w * _vp.h))
					SIMUL_BREAK("Viewport width and/or height is 0. This is invalid for Vulkan.");
				vkViewports[0].setHeight((float)_vp.h).setWidth((float)_vp.w).setX((float)_vp.x).setY((float)_vp.y).setMaxDepth(1.0f).setMinDepth(0.0f);
			}
			commandBuffer->setViewport(0, 1, vkViewports);
			cs->viewportsChanged = false;
		}
		if(cs->scissorChanged)
		{
			vk::Rect2D vkScissors[1];
			for (int i = 0; i < 1; i++)
			{
				int4 scissor = cs->scissor;
				vkScissors[0].setExtent(vk::Extent2D(scissor.z, scissor.w)).setOffset(vk::Offset2D(scissor.x, scissor.y));
			}
			commandBuffer->setScissor(0, 1, vkScissors);
			cs->scissorChanged = false;
		}
		//Set Vertex and Index buffers
		for (auto i : cs->applyVertexBuffers)
		{
			vulkan::Buffer* buffer = (vulkan::Buffer*)i.second;
			vk::Buffer vertexBuffers[] = { buffer->asVulkanBuffer() };
			vk::DeviceSize offsets[] = { 0 };
			if (vertexBuffers[0])
				commandBuffer->bindVertexBuffers(i.first, 1, vertexBuffers, offsets);
		}
		if (cs->indexBuffer)
		{
			vulkan::Buffer* buffer = (vulkan::Buffer*)cs->indexBuffer;
			vk::Buffer indexBuffer = { buffer->asVulkanBuffer() };
			vk::IndexType indexType = cs->indexBuffer->stride == 4 ? vk::IndexType::eUint32 : vk::IndexType::eUint16;
			if (indexBuffer)
				commandBuffer->bindIndexBuffer(indexBuffer, 0, indexType);
		}
	}
	else
	{
		// Set Compute Pipeline
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, renderPassPipeline.pipeline);
		
	}
	return true;
}

crossplatform::PixelFormat RenderPlatform::GetActivePixelFormat(crossplatform::GraphicsDeviceContext &deviceContext)
{
	crossplatform::PixelFormat pixelFormat=crossplatform::PixelFormat::UNKNOWN;
	{
		pixelFormat=defaultColourFormat;
		crossplatform::TargetsAndViewport *tv=&deviceContext.defaultTargetsAndViewport;
		if(deviceContext.targetStack.size())
		{
			tv=deviceContext.targetStack.top();
		}
		if(tv&&tv->textureTargets[0].texture)
			pixelFormat=tv->textureTargets[0].texture->pixelFormat;
	}
	if(pixelFormat == crossplatform::PixelFormat::UNKNOWN)
	{
		SIMUL_ASSERT_WARN_ONCE(pixelFormat != crossplatform::PixelFormat::UNKNOWN, "Unknown active pixel format!");
	}
	return pixelFormat;
}

int RenderPlatform::GetActiveNumOfSamples(crossplatform::GraphicsDeviceContext& deviceContext)
{
	int numOfSamples = 1;
	{
		crossplatform::TargetsAndViewport* tv = &deviceContext.defaultTargetsAndViewport;
		if (deviceContext.targetStack.size())
		{
			tv = deviceContext.targetStack.top();
		}
		if (tv && tv->textureTargets[0].texture)
			numOfSamples = tv->textureTargets[0].texture->GetSampleCount();
	}
	numOfSamples = numOfSamples == 0 ? 1 : numOfSamples;
	return numOfSamples;
}

vk::Extent2D RenderPlatform::GetTextureTargetExtext2D(const crossplatform::TargetsAndViewport::TextureTarget& targetTexture)
{
	int width = 0, length = 0;
	if (targetTexture.texture)
	{
		width = targetTexture.texture->width;
		length = targetTexture.texture->length;

		const int& mip = targetTexture.subresource.mipLevel;
		width >>= mip;
		length >>= mip;
	}
	return vk::Extent2D(width, length);
}

vk::Extent2D RenderPlatform::GetTargetAndViewportExtext2D(const crossplatform::TargetsAndViewport* targetsAndViewport)
{
	vk::Extent2D extent;
	if (targetsAndViewport->textureTargets[0].texture)
	{
		extent = GetTextureTargetExtext2D(targetsAndViewport->textureTargets[0]);
	}
	else if (targetsAndViewport->depthTarget.texture)
	{
		extent = GetTextureTargetExtext2D(targetsAndViewport->depthTarget);
	}
	//else
	//{
	//	extent = vk::Extent2D(targetsAndViewport->viewport.w, targetsAndViewport->viewport.h);
	//}
	return extent;
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

void RenderPlatform::CreateVulkanBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, AllocationInfo& allocationInfo, const char *name)
{
	vk::BufferCreateInfo bufferInfo = {};
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;

	VkBuffer _buffer = VK_NULL_HANDLE;
	const VkBufferCreateInfo& _bufferInfo = bufferInfo.operator const VkBufferCreateInfo &();

	VmaAllocationCreateInfo allocationCI;
	allocationCI.flags = 0;
	allocationCI.usage = VMA_MEMORY_USAGE_UNKNOWN;
	allocationCI.requiredFlags = static_cast<VkMemoryPropertyFlags>(properties);
	allocationCI.preferredFlags = 0;
	allocationCI.memoryTypeBits = 0;
	allocationCI.pool = VK_NULL_HANDLE;
	allocationCI.pUserData = nullptr;

	bool gpuMemory = (properties & vk::MemoryPropertyFlagBits::eDeviceLocal) == vk::MemoryPropertyFlagBits::eDeviceLocal;
	allocationInfo.allocator = gpuMemory ? mGPUAllocator : mCPUAllocator;

	SIMUL_VK_CHECK((vk::Result)vmaCreateBuffer(allocationInfo.allocator, &_bufferInfo, &allocationCI, &_buffer, &allocationInfo.allocation, &allocationInfo.allocationInfo));
	buffer = _buffer;

	if(name)
	{
		SetVulkanName(this, buffer, name);
		vmaSetAllocationName(allocationInfo.allocator, allocationInfo.allocation, name);
	}
}

void RenderPlatform::CreateVulkanImage(vk::ImageCreateInfo &imageCreateInfo, vk::MemoryPropertyFlags properties, vk::Image &image, AllocationInfo &allocationInfo, const char *name)
{
	VkImage _image = VK_NULL_HANDLE;
	const VkImageCreateInfo &_imageCreateInfo = imageCreateInfo.operator const VkImageCreateInfo &();

	VmaAllocationCreateInfo allocationCI;
	allocationCI.flags = 0;
	allocationCI.usage = VMA_MEMORY_USAGE_UNKNOWN;
	allocationCI.requiredFlags = static_cast<VkMemoryPropertyFlags>(properties);
	allocationCI.preferredFlags = 0;
	allocationCI.memoryTypeBits = 0;
	allocationCI.pool = VK_NULL_HANDLE;
	allocationCI.pUserData = nullptr;

	bool gpuMemory = (properties & vk::MemoryPropertyFlagBits::eDeviceLocal) == vk::MemoryPropertyFlagBits::eDeviceLocal;
	allocationInfo.allocator = gpuMemory ? mGPUAllocator : mCPUAllocator;

	SIMUL_VK_CHECK((vk::Result)vmaCreateImage(allocationInfo.allocator, &_imageCreateInfo, &allocationCI, &_image, &allocationInfo.allocation, &allocationInfo.allocationInfo));
	image = _image;

	if (name)
	{
		SetVulkanName(this, image, name);
		vmaSetAllocationName(allocationInfo.allocator, allocationInfo.allocation, name);
	}
}

void RenderPlatform::InsertFences(crossplatform::DeviceContext& deviceContext)
{
	auto pass = (vulkan::EffectPass*)deviceContext.contextState.currentEffectPass;

}

crossplatform::Texture* RenderPlatform::createTexture()
{
	crossplatform::Texture* tex= new vulkan::Texture;
	return tex;
}

crossplatform::Framebuffer* RenderPlatform::CreateFramebuffer(const char *n)
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

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer(crossplatform::ResourceUsageFrequency F)
{
	return new vulkan::PlatformConstantBuffer(F);
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
	if ((t & crossplatform::ShaderResourceType::TEXTURE_2DMS) == crossplatform::ShaderResourceType::TEXTURE_2DMS)
		return GetDummy2DMS();
	if((t&crossplatform::ShaderResourceType::TEXTURE_3D)==crossplatform::ShaderResourceType::TEXTURE_3D)
		return GetDummy3D();
	if ((t & crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY) == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
		return GetDummy2DArray();
	if((t&crossplatform::ShaderResourceType::TEXTURE_2D)==crossplatform::ShaderResourceType::TEXTURE_2D)
		return GetDummy2D();
	if((t&crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY)==crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY)
	{
		if (!mDummyTextureCubeArray)
		{
			mDummyTextureCubeArray = (vulkan::Texture*)CreateTexture("mDummyTextureCubeArray");
			mDummyTextureCubeArray->ensureTextureArraySizeAndFormat(this, 1, 1, 2, 1, crossplatform::PixelFormat::RGBA_8_UNORM,nullptr,false,false,false,true);
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
		mDummyTextureCube->ensureTextureArraySizeAndFormat(this, 1, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM,nullptr,false,false,false,true);
		
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
		crossplatform::TextureCreate textureCreate;
		textureCreate.w = 1;
		textureCreate.l = 1;
		textureCreate.d = 1;
		textureCreate.f = crossplatform::PixelFormat::RGBA_8_UNORM;
		mDummy2D->EnsureTexture(this, &textureCreate);
		mDummy2D->setTexels(immediateContext, &whiteTexel[0], 0, 1);
	}
	return mDummy2D;
}

vulkan::Texture* RenderPlatform::GetDummy2DMS()
{
	if (!mDummy2DMS)
	{
		mDummy2DMS = (vulkan::Texture*)CreateTexture("dummy2dms");
		std::shared_ptr<std::vector<std::vector<uint8_t>>> data;
		mDummy2DMS->ensureTexture2DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM,data, false, false, false, 2);
		mDummy2DMS->setTexels(immediateContext, &whiteTexel[0], 0, 1);

	}
	return mDummy2DMS;
}

vulkan::Texture* RenderPlatform::GetDummy2DArray()
{
	if (!mDummy2DArray)
	{
		mDummy2DArray = (vulkan::Texture*)CreateTexture("dummy2darray");
		mDummy2DArray->ensureTextureArraySizeAndFormat(this, 1, 1, 2, 1, crossplatform::PixelFormat::RGBA_8_UNORM, nullptr);
		mDummy2DArray->setTexels(immediateContext, &whiteTexel[0], 0, 1);
	}
	return mDummy2DArray;
}

vulkan::Texture* RenderPlatform::GetDummy3D()
{
	if (!mDummy3D)
	{
		mDummy3D = (vulkan::Texture*)CreateTexture("dummy3d");
		mDummy3D->ensureTexture3DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM);
		mDummy3D->setTexels(immediateContext, &whiteTexel[0], 0, 1);
	}
	return mDummy3D;
}

crossplatform::RenderState* RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	vulkan::RenderState* s  = new vulkan::RenderState;
	s->desc				 = desc;
	s->type				 = desc.type;
	return s;
}

crossplatform::Query* RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	vulkan::Query* q=new vulkan::Query(type);
	return q;
}

vk::Filter RenderPlatform::toVulkanMinFiltering(crossplatform::SamplerStateDesc::Filtering f)
{
	if (f == platform::crossplatform::SamplerStateDesc::LINEAR)
	{
		return vk::Filter::eLinear;
	}
	return vk::Filter::eNearest;
}

vk::Filter RenderPlatform::toVulkanMaxFiltering(crossplatform::SamplerStateDesc::Filtering f)
{
	if (f == platform::crossplatform::SamplerStateDesc::LINEAR)
	{
		return vk::Filter::eLinear;
	}
	return vk::Filter::eNearest;
}

vk::SamplerMipmapMode RenderPlatform::toVulkanMipmapMode(crossplatform::SamplerStateDesc::Filtering f)
{
	if (f == platform::crossplatform::SamplerStateDesc::LINEAR)
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
	case crossplatform::Topology::POINTLIST:
		return vk::PrimitiveTopology::ePointList;
	case crossplatform::Topology::LINELIST:
		return vk::PrimitiveTopology::eLineList;
	case crossplatform::Topology::LINESTRIP:
		return vk::PrimitiveTopology::eLineStrip;
	case crossplatform::Topology::TRIANGLELIST:
		return vk::PrimitiveTopology::eTriangleList;
	case crossplatform::Topology::TRIANGLESTRIP:
		return vk::PrimitiveTopology::eTriangleStrip;
	case crossplatform::Topology::LINELIST_ADJ:
		return vk::PrimitiveTopology::eLineListWithAdjacency;
	case crossplatform::Topology::LINESTRIP_ADJ:
		return vk::PrimitiveTopology::eLineStripWithAdjacency;
	case crossplatform::Topology::TRIANGLELIST_ADJ:
		return vk::PrimitiveTopology::eTriangleListWithAdjacency;
	case crossplatform::Topology::TRIANGLESTRIP_ADJ:
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

vk::BlendOp RenderPlatform::toVulkanBlendOperation(crossplatform::BlendOperation o)
{
	switch (o)
	{
	case platform::crossplatform::BLEND_OP_ADD:
		return vk::BlendOp::eAdd;
	case platform::crossplatform::BLEND_OP_SUBTRACT:
		return vk::BlendOp::eSubtract;
	case platform::crossplatform::BLEND_OP_MAX:
		return vk::BlendOp::eMax;
	case platform::crossplatform::BLEND_OP_MIN:
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

vk::Format RenderPlatform::ToVulkanFormat(crossplatform::PixelFormat p,crossplatform::CompressionFormat c)
{
	using namespace crossplatform;
	switch(p)
    {
   // case A2_BGR10_UNORM:
   //     return vk::Format::eA2B10G10R10UnormPack32;
    case RGB10_A2_UNORM:
        return vk::Format::eA2B10G10R10UnormPack32;
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
		switch (c)
		{
		case crossplatform::CompressionFormat::ETC2:
			return vk::Format::eEtc2R8G8B8A8UnormBlock;
		case crossplatform::CompressionFormat::BC1:
			return vk::Format::eBc1RgbaUnormBlock;
		case crossplatform::CompressionFormat::BC3:
			return vk::Format::eBc3UnormBlock;
		default:
			return vk::Format::eR8G8B8A8Unorm;
		};
	case BGRA_8_UNORM:
		switch (c)
		{
		case crossplatform::CompressionFormat::ETC2:
			return vk::Format::eEtc2R8G8B8A8UnormBlock;
		case crossplatform::CompressionFormat::BC1:
			return vk::Format::eBc1RgbaUnormBlock;
		case crossplatform::CompressionFormat::BC3:
			return vk::Format::eBc3UnormBlock;
		default:
			return vk::Format::eB8G8R8A8Unorm;
		};
	case RGBA_8_UNORM_SRGB:
		switch (c)
		{
		case crossplatform::CompressionFormat::ETC2:
			return vk::Format::eEtc2R8G8B8A8SrgbBlock;
		case crossplatform::CompressionFormat::BC1:
			return vk::Format::eBc1RgbaSrgbBlock;
		case crossplatform::CompressionFormat::BC3:
			return vk::Format::eBc3SrgbBlock;
		default:
			return vk::Format::eR8G8B8A8Srgb;
		};
	case RGBA_8_SNORM:
		return vk::Format::eR8G8B8A8Snorm;
	case RGB_8_UNORM:
		return vk::Format::eR8G8B8Unorm;
	case RGB_8_SNORM:
		return vk::Format::eR8G8B8Snorm;
	case R_8_UNORM:
		return vk::Format::eR8Unorm;
	case R_8_SNORM:
		return vk::Format::eR8Snorm;
	case R_32_UINT:
		return vk::Format::eR32Uint;
	case RG_32_UINT:
		return vk::Format::eR32G32Uint;
	case RGB_32_UINT:
		return vk::Format::eR32G32B32Uint;
	case D_32_FLOAT:
		return vk::Format::eD32Sfloat;
	case D_32_FLOAT_S_8_UINT:
		return vk::Format::eD32SfloatS8Uint;
	case D_24_UNORM_S_8_UINT:
		return vk::Format::eD24UnormS8Uint;
	case D_16_UNORM:
		return vk::Format::eD16UnormS8Uint;
	case UNDEFINED:
		return vk::Format::eUndefined;
	default:
		return vk::Format::eUndefined;
	};
}
 vk::ImageLayout RenderPlatform::ToVulkanImageLayout(crossplatform::ResourceState r)
 {
	using namespace crossplatform;
	switch(r)
	{
	case ResourceState::VERTEX_AND_CONSTANT_BUFFER:
	case ResourceState::INDEX_BUFFER:
	case ResourceState::RENDER_TARGET:
		return vk::ImageLayout::eColorAttachmentOptimal;
	case ResourceState::UNORDERED_ACCESS:
		return vk::ImageLayout::eGeneral;
	case ResourceState::DEPTH_WRITE:
		return vk::ImageLayout::eDepthStencilAttachmentOptimal;
	case ResourceState::DEPTH_READ:
	case ResourceState::NON_PIXEL_SHADER_RESOURCE:
	case ResourceState::PIXEL_SHADER_RESOURCE:
	case ResourceState::SHADER_RESOURCE:
		return vk::ImageLayout::eShaderReadOnlyOptimal;
	case ResourceState::STREAM_OUT:
	case ResourceState::INDIRECT_ARGUMENT:
	case ResourceState::COPY_DEST:
	case ResourceState::COPY_SOURCE:
	case ResourceState::RESOLVE_DEST:
	case ResourceState::RESOLVE_SOURCE:
	case ResourceState::GENERAL_READ:	
		return vk::ImageLayout::eShaderReadOnlyOptimal;
	case ResourceState::UNKNOWN:
	case ResourceState::COMMON:
	default:
        SIMUL_BREAK_ONCE("Vulkan Image Layout not found.");
		return vk::ImageLayout::eUndefined;
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
		case vk::Format::eD32SfloatS8Uint:
			return D_32_FLOAT_S_8_UINT;
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

vk::PolygonMode RenderPlatform::toVulkanPolygonMode(crossplatform::PolygonMode p)
{
	switch (p)
	{
	case crossplatform::POLYGON_MODE_FILL:
		return vk::PolygonMode::eFill;
	case crossplatform::POLYGON_MODE_LINE:
		return vk::PolygonMode::eLine;
	case crossplatform::POLYGON_MODE_POINT:
		return vk::PolygonMode::ePoint;
	default:
		SIMUL_BREAK("Undefined polygon mode");
		return vk::PolygonMode::eFill;
	}
}

vk::CullModeFlags RenderPlatform::toVulkanCullFace(crossplatform::CullFaceMode c)
{
	switch (c)
	{
	case platform::crossplatform::CULL_FACE_FRONT:
		return vk::CullModeFlagBits::eFront;
	case platform::crossplatform::CULL_FACE_BACK:
		return vk::CullModeFlagBits::eBack;
	case platform::crossplatform::CULL_FACE_FRONTANDBACK:
		return vk::CullModeFlagBits::eFrontAndBack;
	default:
		return vk::CullModeFlagBits::eNone;
	}
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

void RenderPlatform::Resolve(crossplatform::GraphicsDeviceContext& deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source)
{
}

void RenderPlatform::SaveTexture(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	vk::Result result = vk::Result::eSuccess;

	vk::CommandBuffer* cmdBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (!cmdBuffer)
		return;

	crossplatform::PixelFormat format = texture->GetFormat();
	crossplatform::PixelFormatType type = GetElementType(format);
	uint64_t elementCount = static_cast<uint64_t>(GetElementCount(format));
	uint64_t elementSize = static_cast<uint64_t>(GetElementSize(format));
	uint64_t texelCount = static_cast<uint64_t>(texture->width * texture->length * texture->depth);
	uint64_t texelSize = elementCount * elementSize;
	uint64_t size = texelCount * texelSize;

	vk::BufferCreateInfo bufferCI = vk::BufferCreateInfo(vk::BufferCreateFlags(0), size, vk::BufferUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive, 0, nullptr);
	vk::Buffer imageBuffer = vulkanDevice->createBuffer(bufferCI);

	vk::MemoryRequirements memoryRequirements;
	vulkanDevice->getBufferMemoryRequirements(imageBuffer, &memoryRequirements);

	vk::MemoryAllocateInfo memoryAI = vk::MemoryAllocateInfo(memoryRequirements.size,
		FindMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
	vk::DeviceMemory imageBufferMemory = vulkanDevice->allocateMemory(memoryAI);
	vulkanDevice->bindBufferMemory(imageBuffer, imageBufferMemory, 0);

	vulkan::Texture* t = (vulkan::Texture*)texture;
	t->SetLayout(deviceContext, vk::ImageLayout::eTransferSrcOptimal, { crossplatform::TextureAspectFlags::COLOUR, 0, 1, 0, (uint8_t)texture->GetArraySize() });
	vk::BufferImageCopy bic = vk::BufferImageCopy(0, 0, 0,
		{ vk::ImageAspectFlagBits::eColor, 0, 0, (uint32_t)texture->GetArraySize()},
		{ 0, 0, 0 },
		{ (uint32_t)texture->width, (uint32_t)texture->length, (uint32_t)texture->depth });

	cmdBuffer->copyImageToBuffer(*t->AsVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, imageBuffer, 1, &bic);

	ExecuteCommands(deviceContext);

	RestartCommands(deviceContext);

	void* ptr = vulkanDevice->mapMemory(imageBufferMemory, 0, memoryAI.allocationSize, vk::MemoryMapFlags(0));
	crossplatform::RenderPlatform::SaveTextureDataToDisk(lFileNameUtf8, texture->width, texture->length, format, ptr);
	vulkanDevice->unmapMemory(imageBufferMemory);
	ptr = nullptr;

	vulkanDevice->freeMemory(imageBufferMemory, nullptr);
	vulkanDevice->destroyBuffer(imageBuffer, nullptr);
}

void RenderPlatform::RestoreColourTextureState(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex)
{
	if (!tex)
		return;
	vulkan::Texture* t = (vulkan::Texture*)tex;
	t->SetLayout(deviceContext, vk::ImageLayout::eColorAttachmentOptimal, {});
}
void RenderPlatform::RestoreDepthTextureState(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex)
{
	if (!tex)
		return;
	vulkan::Texture* t = (vulkan::Texture*)tex;
	t->SetLayout(deviceContext, vk::ImageLayout::eDepthStencilAttachmentOptimal, {});
}

void* RenderPlatform::GetDevice()
{
	return (void*)vulkanDevice;
}

void RenderPlatform::SetStreamOutTarget(crossplatform::GraphicsDeviceContext&,crossplatform::Buffer *buffer,int/* start_index*/)
{
}

void RenderPlatform::ActivateRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext,int num,crossplatform::Texture** targs,crossplatform::Texture* depth)
{
	if (num > 8)
	{
		SIMUL_CERR << "Too many targets \n";
		return;
	}
	unsigned long long hash=(unsigned long long)num;
	for (int i = 0; i < num; i++)
	{
		hash+=(unsigned long long)targs[i]<<i;
	}
	if (depth)
		hash+=(unsigned long long)depth<<num;
	auto &target=mTargets[hash];
	target.num = num;
	for (int i = 0; i < num; i++)
	{
		target.m_rt[i] = nullptr;
		target.rtFormats[i] = targs[i]->GetFormat();
		target.textureTargets[i].texture = targs[i];
		target.textureTargets[i].subresource.baseArrayLayer = 0;
		target.textureTargets[i].subresource.arrayLayerCount = targs[i]->NumFaces();
		target.textureTargets[i].subresource.mipLevel= 0;
	}
	if (depth)
	{
		target.m_dt = nullptr;
		target.depthFormat = depth->pixelFormat;
		target.depthTarget.texture = depth;
		target.depthTarget.subresource.baseArrayLayer = 0;
		target.depthTarget.subresource.arrayLayerCount = depth->NumFaces();
		target.depthTarget.subresource.mipLevel = 0;
	}
	target.viewport = int4(0, 0, targs[0]->width, targs[0]->length);

	deviceContext.targetStack.push(&target);
	SetViewports(deviceContext, 1, &target.viewport);
}

void RenderPlatform::DeactivateRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext)
{
	crossplatform::RenderPlatform::DeactivateRenderTargets(deviceContext);
	EndRenderPass(deviceContext);
}



void RenderPlatform::SetViewports(crossplatform::GraphicsDeviceContext& deviceContext,int num ,const crossplatform::Viewport* vps)
{
	crossplatform::RenderPlatform::SetViewports(deviceContext, num, vps);
}

crossplatform::DisplaySurface* RenderPlatform::CreateDisplaySurface()
{
	static int view_id=1;
	return new vulkan::DisplaySurface(view_id++);
}

void RenderPlatform::StoreRenderState(crossplatform::DeviceContext &)
{
}

void RenderPlatform::RestoreRenderState(crossplatform::DeviceContext &)
{
}

void RenderPlatform::PopRenderTargets(crossplatform::GraphicsDeviceContext &)
{
}

void RenderPlatform::Draw(crossplatform::GraphicsDeviceContext &deviceContext,int num_verts,int start_vert)
{
	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (!commandBuffer)
		return;
	
	BeginEvent(deviceContext, ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
	ApplyContextState(deviceContext);
	commandBuffer->draw(num_verts,1,start_vert,0);
	EndEvent(deviceContext);
}

void RenderPlatform::DrawIndexed(crossplatform::GraphicsDeviceContext &deviceContext,int num_indices,int start_index,int base_vertex)
{
	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (!commandBuffer)
		return;

	//BeginEvent(deviceContext, ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
	// Only actually draw if ApplyContextState() succeeds.
	if(ApplyContextState(deviceContext))
		commandBuffer->drawIndexed(num_indices,1,start_index,base_vertex,0);
	//EndEvent(deviceContext);
}

void RenderPlatform::GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* t, bool wrap, int array_idx)
{
	t->GenerateMips(deviceContext);
}

crossplatform::Shader* RenderPlatform::CreateShader()
{
	Shader* S = new Shader();
	return S;
}

bool RenderPlatform::memory_type_from_properties(vk::PhysicalDevice *gpu,uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t *typeIndex)
{
	vk::PhysicalDeviceMemoryProperties memory_properties;
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

bool RenderPlatform::MemoryTypeFromProperties(uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t *typeIndex)
{
	return memory_type_from_properties(GetVulkanGPU(),typeBits,  requirements_mask, typeIndex);
}

crossplatform::PixelFormat RenderPlatform::defaultColourFormat=crossplatform::PixelFormat::UNKNOWN;

void RenderPlatform::SetDefaultColourFormat(crossplatform::PixelFormat p)
{
	defaultColourFormat=p;
}

void RenderPlatform::InvalidCachedFramebuffersAndRenderPasses()
{
	for (auto& fb : mFramebuffers)
		PushToReleaseManager(fb.second);
	mFramebuffers.clear();
}

RenderPassHash MakeTargetHash(crossplatform::TargetsAndViewport *tv)
{
	RenderPassHash hashval=0;
	if (tv->textureTargets[0].texture)
	{
		crossplatform::TargetsAndViewport::TextureTarget& tt = tv->textureTargets[0];
		vulkan::Texture* texture = (vulkan::Texture*)tt.texture;
		// TODO: is AsVulkanImageView really necessary here?
		hashval += (unsigned long long)(texture->AsVulkanImageView(MakeTextureView(texture->GetShaderResourceTypeForRTVAndDSV(),tt.subresource.aspectMask, tt.subresource.mipLevel, uint32_t(1), tt.subresource.baseArrayLayer, tt.subresource.arrayLayerCount)))->operator VkImageView();
		hashval += (unsigned long long)texture->width;	//Deal with resizing the framebuffer!
		hashval += (unsigned long long)texture->length;
		hashval += (unsigned long long)texture->GetArraySize();
		hashval += (unsigned long long)texture->GetSampleCount();
	}
	if (tv->depthTarget.texture)
	{
		vulkan::Texture* d = (vulkan::Texture*)tv->depthTarget.texture;
		crossplatform::TargetsAndViewport::TextureTarget& dt = tv->depthTarget;
		vulkan::Texture* texture = (vulkan::Texture*)dt.texture;
		// TODO: is AsVulkanImageView really necessary here?
		hashval += (unsigned long long)(texture->AsVulkanImageView(MakeTextureView(texture->GetShaderResourceTypeForRTVAndDSV(), dt.subresource.aspectMask, dt.subresource.mipLevel, uint32_t(1), dt.subresource.baseArrayLayer, dt.subresource.arrayLayerCount)))->operator VkImageView();
	}
	hashval+=tv->num;
	return hashval;
}

unsigned long long RenderPlatform::InitFramebuffer(crossplatform::DeviceContext& deviceContext,crossplatform::TargetsAndViewport *tv)
{
	crossplatform::PixelFormat colourPF[16] = { crossplatform::PixelFormat::UNKNOWN };
	int numOfSamples = 1;
	for (int i = 0; i < tv->num; i++)
	{
		colourPF[i] = tv->textureTargets[i].texture->pixelFormat;
		if (i == 0)
			numOfSamples = tv->textureTargets[i].texture->GetSampleCount();
	}
	numOfSamples = numOfSamples == 0 ? 1 : numOfSamples;

	vulkan::EffectPass* effectPass = (vulkan::EffectPass*)deviceContext.contextState.currentEffectPass;
	RenderPassHash hashval = MakeTargetHash(tv);
	hashval += 5 * (effectPass ? effectPass->GetHash(colourPF[0], numOfSamples, deviceContext.contextState.topology, deviceContext.contextState.currentLayout) : 0);

	std::map<unsigned long long, vk::Framebuffer>::iterator h = mFramebuffers.find(hashval);
	if (h == mFramebuffers.end() || !h->second || mFramebuffers.empty())
	{
		int count = tv->num + (tv->depthTarget.texture != nullptr && deviceContext.contextState.IsDepthActive());

		vk::Extent2D extent = GetTargetAndViewportExtext2D(tv);
		crossplatform::PixelFormat depthPF = crossplatform::PixelFormat::UNKNOWN;
		if (tv->depthTarget.texture)
		{
			if (deviceContext.contextState.IsDepthActive())
				depthPF = tv->depthTarget.texture->pixelFormat;
		}

		vk::RenderPass& vkRenderPass = effectPass->GetRenderPassPipeline(*deviceContext.AsGraphicsDeviceContext()).renderPass;
		vk::ImageView attachments[9];

		vk::FramebufferCreateInfo framebufferCreateInfo = vk::FramebufferCreateInfo();
		framebufferCreateInfo.renderPass = vkRenderPass;
		framebufferCreateInfo.width = extent.width;
		framebufferCreateInfo.height = extent.height;
		framebufferCreateInfo.layers = 1;

		for (int j = 0; j < tv->num; j++)
		{
			crossplatform::TargetsAndViewport::TextureTarget& tt = tv->textureTargets[j];
			vulkan::Texture* texture = (vulkan::Texture*)tt.texture;
			attachments[j] = *(texture->AsVulkanImageView(MakeTextureView(texture->GetShaderResourceTypeForRTVAndDSV(), 
				tt.subresource.aspectMask, tt.subresource.mipLevel, uint32_t(1), tt.subresource.baseArrayLayer, tt.subresource.arrayLayerCount)));
		}
		if (deviceContext.contextState.IsDepthActive())
		{
			crossplatform::TargetsAndViewport::TextureTarget& dt = tv->depthTarget;
			vulkan::Texture* texture = (vulkan::Texture*)dt.texture;
			if (texture)
			{
				attachments[tv->num] = *(texture->AsVulkanImageView(MakeTextureView(texture->GetShaderResourceTypeForRTVAndDSV(), dt.subresource.aspectMask,(uint8_t)dt.subresource.mipLevel, uint8_t(1),(uint8_t)dt.subresource.baseArrayLayer, (uint8_t)dt.subresource.arrayLayerCount)
			));
			}
		}
		framebufferCreateInfo.attachmentCount = count;
		framebufferCreateInfo.pAttachments = attachments;

		vk::Device* vulkanDevice = AsVulkanDevice();
		SIMUL_VK_CHECK(vulkanDevice->createFramebuffer(&framebufferCreateInfo, nullptr, &mFramebuffers[hashval]));
		SetVulkanName(this, mFramebuffers[hashval], "mFramebuffers");
	}
	return hashval;
}

void RenderPlatform::AllocateDescriptorSets(vk::DescriptorSet &descriptorSet, const vk::DescriptorSetLayout &descriptorSetLayout, uint8_t g)
{
	// Of course, only need to do this if there are ANY inputs.
	if (!descriptorSetLayout)
		return;

	vk::Device *vulkanDevice = AsVulkanDevice();

	vk::DescriptorSetAllocateInfo alloc_info = vk::DescriptorSetAllocateInfo()
												   .setDescriptorSetCount(1)
												   .setPSetLayouts(&descriptorSetLayout);

	if (mDescriptorPools[g])
	{
		alloc_info = alloc_info.setDescriptorPool(mDescriptorPools[g]);
		vk::Result result = vulkanDevice->allocateDescriptorSets(&alloc_info, &descriptorSet);
		if (result == vk::Result::eSuccess)
		{
			SetVulkanName(this, descriptorSet, fmt::format("Shared Descriptor set {0}",g));
		}
		else if(result==vk::Result::eErrorOutOfPoolMemory)
		{
			// The pool we allocated originally has no more to give.
			resourceGroupCountPerFrame[g]*=2;
			CreateDescriptorPool(g, resourceGroupCountPerFrame[g]);
			alloc_info = alloc_info.setDescriptorPool(mDescriptorPools[g]);
			result = vulkanDevice->allocateDescriptorSets(&alloc_info, &descriptorSet);
		}
		if (result != vk::Result::eSuccess)
		{
			SIMUL_CERR << "Failed to allocate descriptor set.\n";
		}
		if (descriptorSet.operator VkDescriptorSet() == nullptr)
		{
			SIMUL_CERR<<"Null descriptor set.\n";
		}
	}
}

vk::DescriptorSet *RenderPlatform::GetDescriptorSetForResourceGroup(crossplatform::DeviceContext &deviceContext, uint8_t g)
{
	auto &cs = deviceContext.contextState;
	if (cs.resourceGroupUploadedCounter[g] == cs.resourceGroupApplyCounter[g])
		return lastDescriptorSet[descriptorSetFrame][g];

	crossplatform::ResourceGroupLayout &resourceGroupLayout = resourceGroupLayouts[g];
	cs.resourceGroupUploadedCounter[g] = cs.resourceGroupApplyCounter[g];
	if (resourceGroupLayout.constantBufferSlots == 0&&resourceGroupLayout.readOnlyResourceSlots==0)
		return lastDescriptorSet[descriptorSetFrame][g];

	// get an unused Descriptor Set:
	bool inserted=false;
	if (m_DescriptorSets_It[g][descriptorSetFrame] == m_DescriptorSets[g][descriptorSetFrame].end())
	{
		// must insert a new descriptor:
		m_DescriptorSets[g][descriptorSetFrame].push_back(vk::DescriptorSet());
		m_DescriptorSets_It[g][descriptorSetFrame] = m_DescriptorSets[g][descriptorSetFrame].end();
		m_DescriptorSets_It[g][descriptorSetFrame]--;
		AllocateDescriptorSets(*m_DescriptorSets_It[g][descriptorSetFrame], descriptorSetLayouts[g], g);
		// std::cout << "New Descriptor Set added: 0x" << std::hex << (void*)((*m_DescriptorSets_It[m_InternalFrameIndex]).operator VkDescriptorSet()) << std::dec << std::endl;
		inserted=true;
	}

	vk::DescriptorSet &descriptorSet = *(m_DescriptorSets_It[g][descriptorSetFrame]);
	if (descriptorSet.operator VkDescriptorSet()==nullptr)
	{
		SIMUL_BREAK_ONCE("Null DescriptorSet error.");
		return nullptr;
	}
	vk::Device *vulkanDevice = AsVulkanDevice();

	int numConstantBuffers = resourceGroupLayout.GetNumConstantBuffers();
	int numReadOnlyResources = resourceGroupLayout.GetNumReadOnlyResources();
	int numBuffers = numConstantBuffers;
	int numImages = numReadOnlyResources;
	int numDescriptors = numBuffers + numReadOnlyResources;
	if (numDescriptors > m_writeDescriptorSets.size())
		m_writeDescriptorSets.resize(numDescriptors);

	if (numBuffers > descriptorBufferInfos.size())
		descriptorBufferInfos.resize(numBuffers);

	vk::DescriptorBufferInfo *descriptorBufferInfo = descriptorBufferInfos.data();

	if (numImages > descriptorImageInfos.size())
		descriptorImageInfos.resize(numImages);
	vk::DescriptorImageInfo *descriptorImageInfo = descriptorImageInfos.data();
	int b = 0;
	int slot = 0;
	for (int i = 0; i < numConstantBuffers; i++, b++)
	{
		//Find the slot number.
		while (slot < 64)
		{
			if (resourceGroupLayout.UsesConstantBufferSlot(slot))
			{
				break;
			}
			slot++;
		}

		//Validate slot number
		if(slot==64)
		{
			SIMUL_INTERNAL_CERR<<"Didn't find every slot for resource group "<<g<<"\n";
			break;
		}
		crossplatform::ConstantBufferBase *cb = cs.applyBuffers[slot];
		vk::WriteDescriptorSet &write = m_writeDescriptorSets[b];
		vk::Buffer *vkBuffer = nullptr;
		vk::DeviceSize vkDeviceSize = 0, offset=0;
		if (cb)
		{
			crossplatform::PlatformConstantBuffer *pcb = (crossplatform::PlatformConstantBuffer *)cb->GetPlatformConstantBuffer();
		
			write.setDstSet(descriptorSet);
			write.setDstBinding(vulkan::RenderPlatform::GenerateConstantBufferSlot(slot));
			pcb->ActualApply(deviceContext);
			vulkan::PlatformConstantBuffer *vcb = (vulkan::PlatformConstantBuffer *)pcb;
			vkBuffer = vcb->GetLastBuffer();
			vkDeviceSize = vcb->GetSize();
			offset = vcb->GetLastOffset();
		}
		else
		{
			SIMUL_INTERNAL_CERR<<"All constant buffers must have valid values in each resource group in-use.\n";
			b--;
			continue;
		}
		write.setDescriptorCount(1);
		write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
		if (vkBuffer)
		{
			descriptorBufferInfo->setOffset(offset)
				.setRange(vkDeviceSize)
				.setBuffer(*vkBuffer);
			write.setPBufferInfo(descriptorBufferInfo);
			descriptorBufferInfo++;
		}
		cs.bufferSlots |= (1 << slot);
		slot++;
	}
	slot=0;
	for (int i = 0; i < numImages; i++, b++)
	{
		// Find the slot number.
		while (slot < 64)
		{
			if (resourceGroupLayout.UsesReadOnlyResourceSlot(slot))
			{
				break;
			}
			slot++;
		}
		vk::WriteDescriptorSet &write = m_writeDescriptorSets[b];
		write.setDstSet(descriptorSet);
		crossplatform::TextureAssignment &ta = cs.textureAssignmentMap[slot];
		vk::ImageView *vkImageView=nullptr;
		vulkan::Texture *texture = (vulkan::Texture *)(ta.texture);
		if(texture)
		{
			texture->FinishLoading(deviceContext);
		}
		else
		{
			// We really don't want to have to do this, but Vulkan GLSL can't eliminate unused textures in compilation:
			if (ta.resourceType == crossplatform::ShaderResourceType::UNKNOWN )
				ta.resourceType=crossplatform::ShaderResourceType::TEXTURE_2D;
			texture = GetDummyTexture(ta.resourceType);
			ta.subresource = crossplatform::DefaultSubresourceRange;
		}
		texture->SetLayout(deviceContext, vk::ImageLayout::eShaderReadOnlyOptimal, ta.subresource);
		write.setDstBinding(vulkan::RenderPlatform::GenerateTextureSlot(slot));
		write.setDescriptorCount(1);
		if(texture)
			vkImageView = texture->AsVulkanImageView(MakeTextureView(ta.resourceType, ta.subresource));
		if (vkImageView)
			descriptorImageInfo->setImageView(*vkImageView);
		descriptorImageInfo->setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		//if (!m_VideoSource)
		{
			write.setDescriptorType(vk::DescriptorType::eSampledImage);
		}
	/*	else
		{
			// video texture:
			write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
			descriptorImageInfo->setSampler(vulkanRenderPlatform->GetSamplerYcbcr());
		}*/
		write.setPImageInfo(descriptorImageInfo);
		bool no_res = write.pImageInfo == nullptr && write.pBufferInfo == nullptr && write.pTexelBufferView == nullptr;
		no_res |= write.dstSet.operator VkDescriptorSet() == 0;
		if (no_res)
		{
			SIMUL_CERR << "VkWriteDescriptorSet (Binding = " << write.dstBinding << ") in group '"
					   << (uint32_t)g << "' has no valid resource associated with it." << std::endl;
			SIMUL_BREAK_ONCE("VkWriteDescriptorSet error.");
			return nullptr;
		}
		cs.textureSlots |= 1 << slot;
		slot++;
		descriptorImageInfo++;
	}
	numDescriptors=b;
	if (numDescriptors)
	{
		for (int i = 0; i < numDescriptors; i++)
		{
			vk::WriteDescriptorSet &write = m_writeDescriptorSets[i];
			bool no_res = write.pImageInfo == nullptr && write.pBufferInfo == nullptr && write.pTexelBufferView == nullptr;
			no_res|=write.dstSet.operator VkDescriptorSet()==0;
			if (no_res)
			{
				SIMUL_CERR << "VkWriteDescriptorSet (Binding = " << write.dstBinding << ") in group '"
						   << (uint32_t)g << "' has no valid resource associated with it." << std::endl;
				SIMUL_BREAK_ONCE("VkWriteDescriptorSet error.");
				return nullptr;
			}
		}
		vulkanDevice->updateDescriptorSets(numDescriptors, m_writeDescriptorSets.data(), 0, nullptr);
	}
	if (m_DescriptorSets_It[g][descriptorSetFrame] != m_DescriptorSets[g][descriptorSetFrame].end())
		m_DescriptorSets_It[g][descriptorSetFrame]++;
	lastDescriptorSet[descriptorSetFrame][g] = &descriptorSet;
	return &descriptorSet;
}

vk::Framebuffer *RenderPlatform::GetCurrentVulkanFramebuffer(crossplatform::GraphicsDeviceContext& deviceContext)
{
	bool dTaV = false;
	crossplatform::TargetsAndViewport *tv;
	if(deviceContext.targetStack.size())
		tv=deviceContext.targetStack.top();
	else
	{
		tv=&(deviceContext.defaultTargetsAndViewport);
		dTaV = true;
	}
	// If shader doesn't write depth, do not try to use a framebuffer that includes depth......
	bool do_depth = (tv->depthTarget.texture != nullptr);
	if(tv->textureTargets[0].texture!=nullptr)
	{
		unsigned long long hash = InitFramebuffer(deviceContext, tv);
		return &(mFramebuffers[hash]);
	}
	else
	{
		vk::Framebuffer *vfb=(vk::Framebuffer*)deviceContext.defaultTargetsAndViewport.m_rt[0];
		return vfb;
	}
}

void RenderPlatform::CreateVulkanRenderpass(crossplatform::DeviceContext& deviceContext, vk::RenderPass& renderPass
	, int num_colour, const crossplatform::PixelFormat* pixelFormats
	, crossplatform::PixelFormat depthFormat, bool depthTest, bool depthWrite, bool clear, int numOfSamples, bool multiview
	, const vk::ImageLayout* initial_layouts, const vk::ImageLayout* final_layouts)
{
	bool depth=(depthFormat!=crossplatform::PixelFormat::UNKNOWN);
	bool msaa = numOfSamples > 1;
	int num_attachments = num_colour + (depth ? 1 : 0);
	vk::AttachmentDescription* attachments = new vk::AttachmentDescription[num_attachments];
	vk::AttachmentReference* colour_reference = nullptr;
	vk::AttachmentReference depth_reference;
	if(num_colour!=0)
		colour_reference = new vk::AttachmentReference[num_colour];
	SIMUL_ASSERT(num_colour <= 16);
	for (int i = 0; i < num_colour; i++)
	{
#if VK_VERSION_1_2
		vk::ImageLayout layout = crossplatform::RenderPlatform::IsDepthFormat(pixelFormats[i]) ?
			vk::ImageLayout::eDepthAttachmentOptimalKHR : vk::ImageLayout::eColorAttachmentOptimal;
#else
		vk::ImageLayout layout = crossplatform::RenderPlatform::IsDepthFormat(pixelFormats[i]) ?
			vk::ImageLayout::eColorAttachmentOptimal : vk::ImageLayout::eColorAttachmentOptimal;
#endif
		vk::ImageLayout end_layout=vk::ImageLayout::eColorAttachmentOptimal;

		if (initial_layouts)
			layout = initial_layouts[i];
		if (final_layouts)
			end_layout = final_layouts[i];

		if (layout == vk::ImageLayout::eUndefined && !clear)
			clear = true;
	
		attachments[i]=  vk::AttachmentDescription()	.setFormat(ToVulkanFormat(pixelFormats[i]))
														.setSamples(msaa ? (vk::SampleCountFlagBits)numOfSamples : vk::SampleCountFlagBits::e1)
														.setLoadOp(clear?vk::AttachmentLoadOp::eClear:vk::AttachmentLoadOp::eLoad)
														.setStoreOp(vk::AttachmentStoreOp::eStore)
														.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
														.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
														.setInitialLayout(layout)
														.setFinalLayout(end_layout);
		colour_reference[i].setAttachment(i).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
	}
		
	if(depth)
	{
		//TODO: add stencil options for layout.
		vk::ImageLayout layout= vk::ImageLayout::eUndefined;
		if (depthWrite && depthTest)
			layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		else if (depthTest && !depthWrite)
			layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
		vk::ImageLayout end_layout=layout;//vk::ImageLayout::eDepthStencilAttachmentOptimal;

		if (layout == vk::ImageLayout::eUndefined && !clear)
			clear = true;

		attachments[num_attachments-1]=  vk::AttachmentDescription()	.setFormat(ToVulkanFormat(depthFormat))
																		.setSamples(msaa ? (vk::SampleCountFlagBits)numOfSamples : vk::SampleCountFlagBits::e1)
																		.setLoadOp(clear?vk::AttachmentLoadOp::eClear:vk::AttachmentLoadOp::eLoad)
																		.setStoreOp(vk::AttachmentStoreOp::eStore)
																		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
																		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
																		.setInitialLayout(layout)
																		.setFinalLayout(end_layout);
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

	auto rp_info = vk::RenderPassCreateInfo()
		.setAttachmentCount(num_attachments)
		.setPAttachments(attachments)
		.setSubpassCount(1)
		.setPSubpasses(&subpass)
		.setDependencyCount(0)
		.setPDependencies(nullptr);

#if PLATFORM_SUPPORT_VULKAN_MULTIVIEW
	auto multiviewCI = vk::RenderPassMultiviewCreateInfo();
	uint32_t viewMask = 0;
	if (HasRenderingFeatures(platform::crossplatform::Multiview) && multiview)
	{
		vk::PhysicalDeviceMultiviewProperties multiviewProperties;
		FillPhysicalDeviceProperties2ExtensionStructure(multiviewProperties);
		viewMask = crossplatform::RenderPlatform::GetViewMaskFromRenderTargets(*deviceContext.AsGraphicsDeviceContext(), multiviewProperties.maxMultiviewViewCount);

		if (viewMask != 0)
		{
			//Set the ViewMask in the ContextState for it to be referenced later, if needed.
			//deviceContext.contextState.viewMask = viewMask;
			multiviewCI
				.setSubpassCount(1)
				.setPViewMasks(&viewMask)
				.setDependencyCount(0)
				.setPViewOffsets(nullptr)
				.setCorrelationMaskCount(1)
				.setPCorrelationMasks(&viewMask);
			rp_info.setPNext(&multiviewCI);
		}
	}
#endif

	auto result = vulkanDevice->createRenderPass(&rp_info, nullptr, &renderPass);
	SetVulkanName(this,renderPass,platform::core::QuickFormat("RenderPass"));
	delete [] attachments;
	delete [] colour_reference;
	SIMUL_ASSERT(result == vk::Result::eSuccess);
}

void RenderPlatform::EndRenderPass(crossplatform::DeviceContext& deviceContext)
{
	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (!commandBuffer)
		return;
	if (deviceContext.contextState.vulkanInsideRenderPass)
	{
		commandBuffer->endRenderPass();
		deviceContext.contextState.vulkanInsideRenderPass = false;
	}
	//std::cout<<"Ended renderpass\n";
}

 std::string RenderPlatform::VulkanResultString(vk::Result res)
 {
	switch (res)
	{
		case vk::Result::eSuccess:
			return "SUCCESS";
		case vk::Result::eNotReady:
			return "NOT_READY";
		case vk::Result::eTimeout:
			return "TIMEOUT";
		case vk::Result::eEventSet:
			return "EVENT_SET";
		case vk::Result::eEventReset:
			return "EVENT_RESET";
		case vk::Result::eIncomplete:
			return "INCOMPLETE";
		case vk::Result::eErrorOutOfHostMemory:
			return "ERROR_OUT_OF_HOST_MEMORY";
		case vk::Result::eErrorOutOfDeviceMemory:
			return "ERROR_OUT_OF_DEVICE_MEMORY";
		case vk::Result::eErrorInitializationFailed:
			return "ERROR_INITIALIZATION_FAILED";
		case vk::Result::eErrorDeviceLost:
			return "ERROR_DEVICE_LOST";
		case vk::Result::eErrorMemoryMapFailed:
			return "ERROR_MEMORY_MAP_FAILED";
		case vk::Result::eErrorLayerNotPresent:
			return "ERROR_LAYER_NOT_PRESENT";
		case vk::Result::eErrorExtensionNotPresent:
			return "ERROR_EXTENSION_NOT_PRESENT";
		case vk::Result::eErrorFeatureNotPresent:
			return "ERROR_FEATURE_NOT_PRESENT";
		case vk::Result::eErrorIncompatibleDriver:
			return "ERROR_INCOMPATIBLE_DRIVER";
		case vk::Result::eErrorTooManyObjects:
			return "ERROR_TOO_MANY_OBJECTS";
		case vk::Result::eErrorFormatNotSupported:
			return "ERROR_FORMAT_NOT_SUPPORTED";
		case vk::Result::eErrorSurfaceLostKHR:
			return "ERROR_SURFACE_LOST_KHR";
		case vk::Result::eErrorNativeWindowInUseKHR:
			return "ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case vk::Result::eSuboptimalKHR:
			return "SUBOPTIMAL_KHR";
		case vk::Result::eErrorOutOfDateKHR:
			return "ERROR_OUT_OF_DATE_KHR";
		case vk::Result::eErrorIncompatibleDisplayKHR:
			return "ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case vk::Result::eErrorValidationFailedEXT:
			return "ERROR_VALIDATION_FAILED_EXT";
		case vk::Result::eErrorInvalidShaderNV:
			return "ERROR_INVALID_SHADER_NV";
		default:
			return "Unknown error.";
	}
}
