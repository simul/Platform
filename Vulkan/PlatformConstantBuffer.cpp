#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/PlatformConstantBuffer.h"

using namespace platform;
using namespace vulkan;


PlatformConstantBuffer::PlatformConstantBuffer(crossplatform::ResourceUsageFrequency F) : crossplatform::PlatformConstantBuffer(F),
			mSlots(0)
			,mMaxAppliesPerFrame(0)
			,mLastFrameIndex(0)
			,mCurApplyCount(0)
			,src(0)
			,size(0)
	,last_offset(0)
	,lastBuffer(nullptr)
{
}

PlatformConstantBuffer::~PlatformConstantBuffer()
{
    InvalidateDeviceObjects();
}

void PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t sz,void *addr)
{
	renderPlatform = r;
	if (resourceUsageFrequency == crossplatform::ResourceUsageFrequency::ONCE || resourceUsageFrequency == crossplatform::ResourceUsageFrequency::ONCE_PER_FRAME)
	{
		//TODO: is this efficient?
		mBufferSize = (unsigned)sz + 2*kBufferAlign;
	}
	SIMUL_ASSERT(sz<=mBufferSize);
	mSlots = unsigned(((sz + size_t(kBufferAlign - 1)) & ~size_t(kBufferAlign - 1)) / size_t(kBufferAlign));
	SIMUL_ASSERT(mSlots>0);
	mMaxAppliesPerFrame = mBufferSize / (kBufferAlign * mSlots);
	SIMUL_ASSERT(mMaxAppliesPerFrame>0);
	vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
		.setSize(mBufferSize)
		.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
		.setSharingMode(vk::SharingMode::eExclusive);
	vulkan::RenderPlatform *vrp = static_cast<vulkan::RenderPlatform*>(renderPlatform);
	vk::Device *vulkanDevice = vrp->AsVulkanDevice();
	for (unsigned int i = 0; i < kNumBuffers; i++)
	{
		vrp->PushToReleaseManager(mBuffers[i]);
		vrp->PushToReleaseManager(mMemory[i]);
		auto result = vulkanDevice->createBuffer(&buf_info, nullptr, &mBuffers[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		SetVulkanName(renderPlatform,mBuffers[i],"Constant buffer");

		vk::MemoryRequirements mem_reqs;
		vulkanDevice->getBufferMemoryRequirements(mBuffers[i], &mem_reqs);

		auto mem_alloc = vk::MemoryAllocateInfo().setAllocationSize(mem_reqs.size).setMemoryTypeIndex(0);

		bool  pass = ((vulkan::RenderPlatform*)renderPlatform)->MemoryTypeFromProperties(
			mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&mem_alloc.memoryTypeIndex);
		SIMUL_ASSERT(pass);

		result = vulkanDevice->allocateMemory(&mem_alloc, nullptr, &mMemory[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		SetVulkanName(renderPlatform,mMemory[i],"Constant buffer memory");
		if(addr)
		{
			auto pData = vulkanDevice->mapMemory(mMemory[i], 0, VK_WHOLE_SIZE, vk::MemoryMapFlags());
			SIMUL_ASSERT(pData!=nullptr);

			memcpy(pData, addr, sz);

			vulkanDevice->unmapMemory(mMemory[i]);
		}

		vulkanDevice->bindBufferMemory(mBuffers[i], mMemory[i], 0);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
	}

	lastBuffer=nullptr;
	last_offset=0;
	size=sz;
	last_offset=0;
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	vulkan::RenderPlatform* r = static_cast<vulkan::RenderPlatform*>(renderPlatform);
	if(!vulkanDevice)
		return;
	for (uint32_t i = 0; i < kNumBuffers; i++)
	{
		r->PushToReleaseManager(mBuffers[i]);
		r->PushToReleaseManager(mMemory[i]);
	}
	renderPlatform=nullptr;
}

void PlatformConstantBuffer::Apply(platform::crossplatform::DeviceContext& deviceContext,size_t sz,void* addr)
{
	src=addr;
	//size=sz;
}

void PlatformConstantBuffer::ActualApply(crossplatform::DeviceContext &deviceContext) 
{
	if(!src)
		return;
	if (!changed)
	{
		if(resourceUsageFrequency == crossplatform::ResourceUsageFrequency::ONCE && mCurApplyCount > 0)
			return;
		if (resourceUsageFrequency == crossplatform::ResourceUsageFrequency::ONCE_PER_FRAME && mCurApplyCount > 0)
			return;
		if (resourceUsageFrequency == crossplatform::ResourceUsageFrequency::FEW_PER_FRAME && mCurApplyCount > 0)
			return;
	}
	changed=false;
	bool resetframe=false;
	vk::Device *vulkanDevice=((vulkan::RenderPlatform*)renderPlatform)->AsVulkanDevice();
	if (mCurApplyCount >= mMaxAppliesPerFrame)
	{
		// This should really be solved by having some kind of pool? Or allocating more space, something like that
		//SIMUL_BREAK_ONCE("This ConstantBuffer reached its maximum apply count");
		mBufferSize*=2;
		RestoreDeviceObjects(renderPlatform,size,nullptr);
		resetframe=true;
		//return;
	}

	auto rPlat = (vulkan::RenderPlatform*)renderPlatform;
	// If new frame, update current frame index and reset the apply count
	if (mLastFrameIndex != renderPlatform->GetFrameNumber())
	{
		resetframe = true;
	}
	if(resetframe)
	{
		mLastFrameIndex = renderPlatform->GetFrameNumber();
		mCurApplyCount = 0;
		currentFrameIndex++;
		currentFrameIndex = currentFrameIndex % (kNumBuffers);
	}

	// pDest points at the begining of the uploadHeap, we can offset it! (we created 64KB and each Constart buffer
	// has a minimum size of kBufferAlign)
	uint8_t* pDest = nullptr;
	last_offset = (kBufferAlign * mSlots) * mCurApplyCount;	
	lastBuffer=&mBuffers[currentFrameIndex];

	auto pData = vulkanDevice->mapMemory(mMemory[currentFrameIndex],last_offset, (kBufferAlign * mSlots),  vk::MemoryMapFlags());
	
	if(pData)
	{
		memcpy(pData,src, size);
		vulkanDevice->unmapMemory(mMemory[currentFrameIndex]);
	}
	
	mCurApplyCount++;
	src=0;
}

size_t PlatformConstantBuffer::GetLastOffset()
{
	return last_offset;
}

vk::Buffer *PlatformConstantBuffer::GetLastBuffer()
{
	return lastBuffer;
}

size_t PlatformConstantBuffer::GetSize()
{
	return size;
}

void PlatformConstantBuffer::Unbind(platform::crossplatform::DeviceContext& deviceContext)
{
}
