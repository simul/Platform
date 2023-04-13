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
#include <vulkan/vulkan.hpp>

#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

bool platform::vulkan::debugUtilsSupported=false;
bool platform::vulkan::debugMarkerSupported=false;

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

const char* RenderPlatform::GetName()const
{
	return "Vulkan";
}

void RenderPlatform::RestoreDeviceObjects(void* vkDevice_vkInstance_gpu)
{
	ERRNO_BREAK
	
	void** ptr = (void**)vkDevice_vkInstance_gpu;
	vulkanDevice = (vk::Device*)ptr[0];
	vulkanInstance = (vk::Instance*)ptr[1];
	vulkanGpu = (vk::PhysicalDevice*)ptr[2];
	immediateContext.platform_context = nullptr;
	crossplatform::RenderPlatform::RestoreDeviceObjects(nullptr);
	RecompileShaders();

	// Check feature support.
	renderingFeatures = crossplatform::RenderingFeatures::None;
	if (CheckDeviceExtension(VK_KHR_MULTIVIEW_EXTENSION_NAME))
		renderingFeatures = (crossplatform::RenderingFeatures)((uint32_t)renderingFeatures | (uint32_t)crossplatform::RenderingFeatures::Multiview);

	int swapchainImageCount = SIMUL_VULKAN_FRAME_LAG + 1;
	vk::DescriptorPoolSize const poolSizes[] = {
		vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(swapchainImageCount)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eSampledImage).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eSampler).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageBufferDynamic).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageImage).setDescriptorCount(swapchainImageCount * 32)
		,vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(swapchainImageCount * 32) };

	auto const descriptorPoolCreateInfo =
		vk::DescriptorPoolCreateInfo().setMaxSets(swapchainImageCount).setPoolSizeCount(_countof(poolSizes)).setPPoolSizes(poolSizes);
	auto result = vulkanDevice->createDescriptorPool(&descriptorPoolCreateInfo, nullptr, &mDescriptorPool);

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
	for (auto& i : mFramebuffers)
	{
		vulkanDevice->destroyFramebuffer(i.second, nullptr);
	}
	mFramebuffers.clear();
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
	SAFE_DELETE(mDummy3D);
	SAFE_DELETE(mDummy2D);
	vulkanDevice->destroyDescriptorPool(mDescriptorPool, nullptr);

	ClearReleaseManager();

	vulkanDevice = nullptr;
}

void RenderPlatform::RecompileShaders()
{
	if (!vulkanDevice)
		return;
	//shaders.clear();
	crossplatform::RenderPlatform::RecompileShaders();
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

crossplatform::GraphicsDeviceContext& RenderPlatform::GetImmediateContext()
{
	if (immediateContext.platform_context == nullptr && vulkanDevice != nullptr)
	{
		vk::CommandPoolCreateInfo cmdPoolCI;
		cmdPoolCI.setPNext(nullptr)
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(0);
		cmdPool = vulkanDevice->createCommandPool(cmdPoolCI);

		vk::CommandBufferAllocateInfo cmdBufferAI;
		cmdBufferAI.setPNext(nullptr)
			.setCommandPool(cmdPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		cmdBuffer = vulkanDevice->allocateCommandBuffers(cmdBufferAI)[0];

		vk::CommandBufferBeginInfo cmdBufferBI;
		cmdBufferBI.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		cmdBuffer.begin(cmdBufferBI);

		immediateContext.platform_context = &cmdBuffer;
		immediateContext.renderPlatform = this;
		immediateContext.contextState.contextActive = true;
	}
	return crossplatform::RenderPlatform::GetImmediateContext();
}

void RenderPlatform::FlushImmediateContext()
{
	if (!immediateContext.contextState.contextActive)
	{
		return;
	}
	cmdBuffer.end();

	vk::FenceCreateInfo fenceCI;
	fenceCI.setPNext(nullptr).setFlags(vk::FenceCreateFlags(0));
	vk::Fence fence = vulkanDevice->createFence(fenceCI);

	vk::SubmitInfo si;
	si.setWaitSemaphoreCount(0).setPWaitSemaphores(nullptr)
		.setWaitDstStageMask(nullptr)
		.setCommandBufferCount(1).setPCommandBuffers(&cmdBuffer)
		.setSignalSemaphoreCount(0).setPSignalSemaphores(nullptr)
		.setPNext(nullptr);
	vk::Queue queue = vulkanDevice->getQueue(0, 0);
	SIMUL_VK_CHECK(queue.submit(1, &si, fence));
	SIMUL_VK_CHECK(vulkanDevice->waitForFences(1, &fence, true, UINT64_MAX));

	cmdBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
}

void RenderPlatform::PushToReleaseManager(vk::Buffer &b)
{
	releaseBuffers.insert(b);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::BufferView &v)
{
	releaseBufferViews.insert(v);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::DeviceMemory &m)
{
	releaseMemories.insert(m);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::ImageView& i)
{
	releaseImageViews.insert(i);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::Framebuffer& f)
{
	releaseFramebuffers.insert(f);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::RenderPass& r)
{
	releaseRenderPasses.insert(r);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::Pipeline& r)
{
	releasePipelines.insert(r);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::PipelineCache& r)
{
	releasePipelineCaches.insert(r);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::Image& i)
{
	releaseImages.insert(i);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::Sampler& i)
{
	releaseSamplers.insert(i);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::PipelineLayout& i)
{
	releasePipelineLayouts.insert(i);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::DescriptorSetLayout& i)
{
	releaseDescriptorSetLayouts.insert(i);
	resourcesToBeReleased = true;
}
void RenderPlatform::PushToReleaseManager(vk::DescriptorPool& i)
{
	releaseDescriptorPools.insert(i);
	resourcesToBeReleased = true;
}
void RenderPlatform::ClearReleaseManager()
{
	for (auto i : releaseBuffers)
	{
		vulkanDevice->destroyBuffer(i, nullptr);
	}
	releaseBuffers.clear();
	for (auto i : releaseBufferViews)
	{
		vulkanDevice->destroyBufferView(i, nullptr);
	}
	releaseBufferViews.clear();
	for (auto i : releaseMemories)
	{
		vulkanDevice->freeMemory(i, nullptr);
	}
	releaseMemories.clear();
	for (auto i : releaseImageViews)
	{
		vulkanDevice->destroyImageView(i, nullptr);
	}
	releaseImageViews.clear();
	for (auto i : releaseFramebuffers)
	{
		vulkanDevice->destroyFramebuffer(i, nullptr);
	}
	releaseFramebuffers.clear();
	for (auto i : releaseRenderPasses)
	{
		vulkanDevice->destroyRenderPass(i, nullptr);
	}
	releaseRenderPasses.clear();
	for (auto i : releaseImages)
	{
		vulkanDevice->destroyImage(i, nullptr);
	}
	releaseImages.clear();
	for (auto i : releaseSamplers)
	{
		vulkanDevice->destroySampler(i, nullptr);
	}
	releaseSamplers.clear();
	for (auto i : releasePipelines)
	{
		vulkanDevice->destroyPipeline(i, nullptr);
	}
	releasePipelines.clear();
	for (auto i : releasePipelineCaches)
	{
		vulkanDevice->destroyPipelineCache(i, nullptr);
	}
	releasePipelineCaches.clear();
	for (auto i : releasePipelineLayouts)
	{
		vulkanDevice->destroyPipelineLayout(i, nullptr);
	}
	releasePipelineLayouts.clear();
	for (auto i : releaseDescriptorSetLayouts)
	{
		vulkanDevice->destroyDescriptorSetLayout(i, nullptr);
	}
	releaseDescriptorSetLayouts.clear();
	for (auto i : releaseDescriptorPools)
	{
		vulkanDevice->destroyDescriptorPool(i, nullptr);
	}
	releaseDescriptorPools.clear();

	resourcesToBeReleased = false;
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
	vk::ImageLayout srcLayout = src->GetLayout();
	vk::ImageLayout dstLayout = dst->GetLayout();

	src->SetLayout(deviceContext, vk::ImageLayout::eTransferSrcOptimal);
	dst->SetLayout(deviceContext, vk::ImageLayout::eTransferDstOptimal);
	// Perform the copy. This is done GPU side and does not incur much CPU overhead (if copying full resources)
	commandBuffer->copyImage(src->AsVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, dst->AsVulkanImage(), vk::ImageLayout::eTransferDstOptimal
													,static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

	// often, the textures might have been in ePreinitialized or eUndefined, which we CANNOT
	//   transition TO, so we CAN't restore the previous layouts here:
	//src->SetLayout(deviceContext, srcLayout);
	//dst->SetLayout(deviceContext, dstLayout);
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
		d.vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vulkanInstance->getProcAddr("vkCmdBeginDebugUtilsLabelEXT");
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
		d.vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)vulkanInstance->getProcAddr("vkCmdDebugMarkerBeginEXT");
		commandBuffer->debugMarkerBeginEXT(markerInfo, d);
	}
}

void RenderPlatform::EndEvent(crossplatform::DeviceContext& deviceContext)
{
	if (debugUtilsSupported)
	{
		vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
		vk::DispatchLoaderDynamic d;
		d.vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vulkanInstance->getProcAddr("vkCmdEndDebugUtilsLabelEXT");
		commandBuffer->endDebugUtilsLabelEXT(d);
	}
	if (debugMarkerSupported)
	{
		vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
		vk::DispatchLoaderDynamic d;
		d.vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)vulkanInstance->getProcAddr("vkCmdDebugMarkerEndEXT");
		commandBuffer->debugMarkerEndEXT(d);
	}
}

void RenderPlatform::DispatchCompute(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	if (!commandBuffer)
		return;

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

	BeginEvent(deviceContext, ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
	SetTopology(deviceContext, crossplatform::Topology::TRIANGLESTRIP);
	ApplyContextState(deviceContext);
	commandBuffer->draw(4,1,0,0);
	SetTopology(deviceContext, crossplatform::Topology::UNDEFINED);
	EndEvent(deviceContext);
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
	if (frameNumber != GetFrameNumber())
	{
		// Call start render at least once per frame to make sure the bins release objects!
		ContextFrameBegin(*deviceContext.AsGraphicsDeviceContext());
	}
	if (frameNumber != mLastFrame && *commandBuffer != cmdBuffer) //Check this VkCommandBuffer is not the one used of the ImmediateContext - AJR
	{
		//Make sure that any resources set for deletion are removed
		if (resourcesToBeReleased)
			ClearReleaseManager();

		mLastFrame = frameNumber;
	}
	
	//Apply the pass setting up the descriptors, pipeline and render pass.
	pass->Apply(deviceContext, false);
	crossplatform::GraphicsDeviceContext* graphicsDeviceContext = deviceContext.AsGraphicsDeviceContext();
	const EffectPass::RenderPassPipeline& renderPassPipeline = pass->GetRenderPassPipeline(*graphicsDeviceContext);
	const vk::PipelineLayout& pipelineLayout = pass->GetLastestPipelineLayout();
	const vk::DescriptorSet& descriptorSet = pass->GetLastestDescriptorSet();
	bool setDescriptors = descriptorSet ? true : false;

	// If not a compute shader, apply viewports:
	if (commandBuffer != nullptr && pass->shaders[crossplatform::SHADERTYPE_COMPUTE] == nullptr)
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
			deviceContext.contextState.vulkanInsideRenderPass = true;
		}

		//Set Graphics Pipeline
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, renderPassPipeline.pipeline);

		//Set the Viewports and Scissors
		vk::Viewport vkViewports[1];
		vk::Rect2D vkScissors[1];
		for (int i = 0; i < 1; i++)
		{
			crossplatform::Viewport& _vp = cs->viewports[i];
			if (!(_vp.w * _vp.h))
				_vp = vp;
			if (!(_vp.w * _vp.h))
				SIMUL_BREAK("Viewport width and/or height is 0. This is invalid for Vulkan.");

			int4 scissor = cs->scissor;
			vkViewports[0].setHeight((float)_vp.h).setWidth((float)_vp.w).setX((float)_vp.x).setY((float)_vp.y).setMaxDepth(1.0f).setMinDepth(0.0f);
			vkScissors[0].setExtent(vk::Extent2D(scissor.z, scissor.w)).setOffset(vk::Offset2D(scissor.x, scissor.y));
		}
		commandBuffer->setViewport(0, 1, vkViewports);
		commandBuffer->setScissor(0, 1, vkScissors);

		//Set the Desciptor Sets
		if (setDescriptors)
			commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

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
		//Set Compute Pipeline
		commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, renderPassPipeline.pipeline);

		//Set the Desciptor Sets
		if (setDescriptors)
			commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
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

		const int& mip = targetTexture.mip;
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

#include "Platform/Core/StringFunctions.h"
void RenderPlatform::CreateVulkanBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory,const char *name)
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
//#ifdef _DEBUG
	if(name)
	{
		SetVulkanName(this,(buffer),name);
		SetVulkanName(this,(bufferMemory),platform::core::QuickFormat("%s memory",name));
	}
//#endif
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
	if ((t & crossplatform::ShaderResourceType::TEXTURE_2DMS) == crossplatform::ShaderResourceType::TEXTURE_2DMS)
		return GetDummy2DMS();
	if((t&crossplatform::ShaderResourceType::TEXTURE_3D)==crossplatform::ShaderResourceType::TEXTURE_3D)
		return GetDummy3D();
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
	/*vulkan::Texture* src = (vulkan::Texture*)source;
	vulkan::Texture* dst = (vulkan::Texture*)destination;
	if (!src || !dst)
	{
		SIMUL_CERR << "Failed to Resolve.\n";
		return;
	}

	//Both have valid pixel formats
	vk::Format resolveFormat;
	if (src->pixelFormat != crossplatform::PixelFormat::UNKNOWN  && dst->pixelFormat != crossplatform::PixelFormat::UNKNOWN)
	{
		resolveFormat = vulkan::RenderPlatform::ToVulkanFormat(src->pixelFormat);
	}

	//Resolve src image to dst image
	
	vk::CommandBuffer* commandBuffer =(vk::CommandBuffer*)deviceContext.platform_context;
	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	vk::ImageResolve imageResolve;
	imageResolve.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
	imageResolve.srcOffset = vk::Offset3D(0, 0, 0);
	imageResolve.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
	imageResolve.dstOffset = vk::Offset3D(0, 0 ,0);
	imageResolve.extent = vk::Extent3D(deviceContext.defaultTargetsAndViewport.viewport.w, deviceContext.defaultTargetsAndViewport.viewport.h, 1);
	
	vk::ImageMemoryBarrier barrier = {};
	barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
	barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	barrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = src->GetImage();
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	vk::PipelineStageFlags srcAccessMask = vk::PipelineStageFlags::Flags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	vk::PipelineStageFlags dstAccessMask = vk::PipelineStageFlags::Flags(vk::PipelineStageFlagBits::eBottomOfPipe);
	vk::DependencyFlags flags = vk::DependencyFlags::Flags(vk::DependencyFlagBits::eDeviceGroup);

	if (commandBuffer)
	{
		commandBuffer->pipelineBarrier(srcAccessMask, dstAccessMask, flags, 0, nullptr, 0, nullptr, 1, &barrier);
		commandBuffer->resolveImage(src->GetImage(), vk::ImageLayout::eTransferSrcOptimal, dst->GetImage(), vk::ImageLayout::eSharedPresentKHR, imageResolve);
	}*/
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	vk::Result result = vk::Result::eSuccess;

	crossplatform::DeviceContext deviceContext = GetImmediateContext();
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
	t->SetLayout(deviceContext, vk::ImageLayout::eTransferSrcOptimal);
	vk::BufferImageCopy bic = vk::BufferImageCopy(0, 0, 0,
		{ vk::ImageAspectFlagBits::eColor, 0, 0, (uint32_t)texture->GetArraySize()},
		{ 0, 0, 0 },
		{ (uint32_t)texture->width, (uint32_t)texture->length, (uint32_t)texture->depth });

	cmdBuffer->copyImageToBuffer(t->AsVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, imageBuffer, 1, &bic);
	FlushImmediateContext();

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
	t->SetLayout(deviceContext, vk::ImageLayout::eColorAttachmentOptimal);
}
void RenderPlatform::RestoreDepthTextureState(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex)
{
	if (!tex)
		return;
	vulkan::Texture* t = (vulkan::Texture*)tex;
	t->SetLayout(deviceContext, vk::ImageLayout::eDepthStencilAttachmentOptimal);
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
		target.textureTargets[i].layer = 0;
		target.textureTargets[i].layerCount = targs[i]->NumFaces();
		target.textureTargets[i].mip= 0;
	}
	if (depth)
	{
		target.m_dt = nullptr;
		target.depthFormat = depth->pixelFormat;
		target.depthTarget.texture = depth;
		target.depthTarget.layer = 0;
		target.depthTarget.layerCount = depth->NumFaces();
		target.depthTarget.mip = 0;
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

#include <cstdint>

void RenderPlatform::SetViewports(crossplatform::GraphicsDeviceContext& deviceContext,int num ,const crossplatform::Viewport* vps)
{
	crossplatform::RenderPlatform::SetViewports(deviceContext, num, vps);
}

void RenderPlatform::EnsureEffectIsBuilt(const char *)
{
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

	BeginEvent(deviceContext, ((vulkan::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
	ApplyContextState(deviceContext);
	commandBuffer->drawIndexed(num_indices,1,start_index,base_vertex,0);
	EndEvent(deviceContext);
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
		bool allLayers = texture->NumFaces() == tt.layerCount;

		hashval += (unsigned long long)(texture->AsVulkanImageView(crossplatform::ShaderResourceType::UNKNOWN, allLayers ? -1 : tt.layer, tt.mip))->operator VkImageView();
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
		bool allLayers = texture->NumFaces() == dt.layerCount;

		hashval += (unsigned long long)(texture->AsVulkanDepthView(allLayers ? -1 : dt.layer, dt.mip))->operator VkImageView();
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
			bool allLayers = texture->NumFaces() == tt.layerCount;
			attachments[j] = *(texture->AsVulkanImageView(crossplatform::ShaderResourceType::UNKNOWN, allLayers ? -1 : tt.layer, tt.mip));
		}
		if (deviceContext.contextState.IsDepthActive())
		{
			crossplatform::TargetsAndViewport::TextureTarget& dt = tv->depthTarget;
			vulkan::Texture* texture = (vulkan::Texture*)dt.texture;
			bool allLayers = texture->NumFaces() == dt.layerCount;
			attachments[tv->num] = *(texture->AsVulkanDepthView(allLayers ? -1 : dt.layer, dt.mip));
		}
		framebufferCreateInfo.attachmentCount = count;
		framebufferCreateInfo.pAttachments = attachments;

		vk::Device* vulkanDevice = AsVulkanDevice();
		SIMUL_VK_CHECK(vulkanDevice->createFramebuffer(&framebufferCreateInfo, nullptr, &mFramebuffers[hashval]));
		SetVulkanName(this, mFramebuffers[hashval], "mFramebuffers");
	}
	return hashval;
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
		vk::ImageLayout end_layout=vk::ImageLayout::eDepthStencilAttachmentOptimal;

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
