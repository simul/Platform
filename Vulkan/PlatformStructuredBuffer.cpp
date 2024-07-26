#include "Platform/Vulkan/PlatformStructuredBuffer.h"
#include "Platform/Vulkan/RenderPlatform.h"
using namespace platform;
using namespace vulkan;


PlatformStructuredBuffer::PlatformStructuredBuffer()
	:mSlots(0)
	,mMaxApplyCount(2)
	,mCurApplyCount(0)
	,last_offset(0)
	,buffer(nullptr)
	,mCpuRead(false)
	,mLastFrame(0)
	,mFrameIndex(0)
{
}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
	InvalidateDeviceObjects();
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r, int count, int unit_size, bool computable, bool cpu_read, void *init_data, const char *_name, crossplatform::ResourceUsageFrequency _bufferUsageHint)
{
	bufferUsageHint = bufferUsageHint;
	renderPlatform = r;
	mNumElements = count;
	mElementByteSize = unit_size;
	if (_name)
		name = _name;
	// Really, if it's cpu-readable, we must have only one copy per-frame.
	if (mCpuRead)
		mMaxApplyCount=1;
	mUnitSize = mNumElements * mElementByteSize;
	mTotalSize = mUnitSize * mMaxApplyMod;
	delete [] buffer;
	buffer = nullptr;
	mCpuRead = cpu_read;
	mSlots = ((mTotalSize + (kBufferAlign - 1)) & ~ (kBufferAlign - 1)) / kBufferAlign;
	
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	AddPerFrameBuffer(init_data);
	firstBuffer=perFrameBuffers.begin();
	for (unsigned int i = 1; i < kNumBuffers; i++)
	{
		AddPerFrameBuffer(init_data);
	}
	lastBuffer=perFrameBuffers.end();
	// If this Structured Buffer supports CPU read,
	// we initialize a set of READ_BACK buffers:
	if (mCpuRead)
	{
		int buffer_aligned_size=mSlots*kBufferAlign;
		vk::BufferUsageFlags usageFlags=vk::BufferUsageFlagBits::eStorageBuffer|vk::BufferUsageFlagBits::eTransferDst;
		for (unsigned int i = 0; i < kNumBuffers; i++)
		{
			vulkanRenderPlatform->CreateVulkanBuffer(buffer_aligned_size, usageFlags
				, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
				, mReadBuffers[i], mReadBufferAllocationInfo[i],(name+"psb read").c_str());
		}
	}

	last_offset=0;
}

void PlatformStructuredBuffer::AddPerFrameBuffer(const void *init_data)
{
	vk::BufferUsageFlags usageFlags=vk::BufferUsageFlagBits::eStorageBuffer;
	if(mCpuRead)
		usageFlags|=vk::BufferUsageFlagBits::eTransferSrc;
	int buffer_aligned_size=mSlots*kBufferAlign;
	int alloc_size=buffer_aligned_size*mMaxApplyCount;
	vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
		.setSize(alloc_size)
		.setUsage(vk::BufferUsageFlagBits::eStorageBuffer);
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	perFrameBuffers.push_back(PerFrameBuffer());
	PerFrameBuffer &perFrameBuffer=perFrameBuffers.back();
	vulkanRenderPlatform->CreateVulkanBuffer(alloc_size
		, usageFlags
		, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		, perFrameBuffer.mBuffer, perFrameBuffer.mAllocationInfo,"psb");
	if(init_data)
	{
		void *pData = nullptr;
		vmaMapMemory(perFrameBuffer.mAllocationInfo.allocator, perFrameBuffer.mAllocationInfo.allocation, &pData);
		SIMUL_ASSERT(pData!=nullptr);
		memcpy(pData, init_data, mTotalSize);
		vmaUnmapMemory(perFrameBuffer.mAllocationInfo.allocator, perFrameBuffer.mAllocationInfo.allocation);
	}
}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext& deviceContext)
{
	if (!buffer)
		buffer = new unsigned char[mTotalSize];
	return buffer;
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	if (renderPlatform->GetFrameNumber() >= kNumBuffers)
	{
		vmaMapMemory(mReadBufferAllocationInfo[mFrameIndex].allocator, mReadBufferAllocationInfo[mFrameIndex].allocation, &mCurReadMap);
		return mCurReadMap;
	}
	return nullptr;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	if (mCurReadMap)
	{
		vmaUnmapMemory(mReadBufferAllocationInfo[mFrameIndex].allocator, mReadBufferAllocationInfo[mFrameIndex].allocation);
	}
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	vulkanRenderPlatform->EndRenderPass(deviceContext);

	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer*)deviceContext.platform_context;
	vk::BufferCopy region=vk::BufferCopy().setDstOffset(0).setSize(mTotalSize).setSrcOffset(last_offset);
	if(lastBuffer->mBuffer)
		commandBuffer->copyBuffer(lastBuffer->mBuffer,mReadBuffers[mFrameIndex],region);
}

void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext& deviceContext,void* data)
{
	if (data)
	{
		if(!buffer)
			buffer = new unsigned char[mTotalSize];
		memcpy(buffer,data,mTotalSize);
	}
}

void PlatformStructuredBuffer::ActualApply(crossplatform::DeviceContext &deviceContext,bool as_uav) 
{
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	if (mCurApplyCount >= mMaxApplyCount)
	{
		mMaxApplyCount*=mCountMultiplier;
		// Run out of applies on this per-frame buffer. Make a new set with double the applies. Or quadruple etc.
		// Add three(or however many kNumBuffers) more buffers
		AddPerFrameBuffer(nullptr);
		firstBuffer=perFrameBuffers.end();
		firstBuffer--;
		for (unsigned int i = 1; i < kNumBuffers; i++)
		{
			AddPerFrameBuffer(nullptr);
		}
		lastBuffer=firstBuffer;
		//SIMUL_BREAK_ONCE("This PlatformStructuredBuffer reached its maximum apply count");
		mCountMultiplier*=2;
		// Force restart of the frame:
		mLastFrame=(int64_t)-1;
	}

	auto rPlat = (vulkan::RenderPlatform*)deviceContext.renderPlatform;

	// If new frame, update current frame index and reset the apply count
	if (bufferUsageHint != crossplatform::ResourceUsageFrequency::ONCE)
	{
		if (mLastFrame != renderPlatform->GetFrameNumber() || lastBuffer == perFrameBuffers.end())
		{
			if (lastBuffer != perFrameBuffers.end())
				lastBuffer++;
			if (lastBuffer == perFrameBuffers.end())
				lastBuffer = firstBuffer;
			if (lastBuffer == perFrameBuffers.end())
				SIMUL_BREAK("bad buffer iterator");
			mLastFrame = renderPlatform->GetFrameNumber();
			mCurApplyCount = 0;
			mFrameIndex = (mFrameIndex + 1) % kNumBuffers;
		}
		if (lastBuffer == perFrameBuffers.end())
			SIMUL_BREAK("bad buffer iterator");
	}
	else
	{
		lastBuffer = perFrameBuffers.begin();
	}
	last_offset		=(kBufferAlign * mSlots) * mCurApplyCount;	

	if (!as_uav && buffer)
	{
		uint8_t *pData = nullptr;
		vmaMapMemory(lastBuffer->mAllocationInfo.allocator, lastBuffer->mAllocationInfo.allocation, (void**)&pData);

		if (pData)
		{
			memcpy(pData + last_offset, buffer, mTotalSize);
			vmaUnmapMemory(lastBuffer->mAllocationInfo.allocator, lastBuffer->mAllocationInfo.allocation);
		}
		mCurApplyCount++;
	}
}

void PlatformStructuredBuffer::AddFence(crossplatform::DeviceContext& deviceContext)
{
	// Insert a fence:
	if (cpu_read)
	{
	 //   int idx     = deviceContext.GetFrameNumber() % mNumBuffers;
	}
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext& deviceContext)
{
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	if(!vulkanDevice)
		return;
	vulkan::RenderPlatform *rPlat=(vulkan::RenderPlatform *)renderPlatform;
	for(auto i:perFrameBuffers)
	{
		rPlat->PushToReleaseManager(i.mBuffer, &(i.mAllocationInfo));
		rPlat->PushToReleaseManager(i.mBufferView);
	}
	perFrameBuffers.clear();
	firstBuffer=lastBuffer=perFrameBuffers.end();
	for(int i=0;i<kNumBuffers;i++)
	{
		rPlat->PushToReleaseManager(mReadBuffers[i], &(mReadBufferAllocationInfo[i]));
	}
	renderPlatform=nullptr;
	delete [] buffer;
	buffer=nullptr;
}

size_t PlatformStructuredBuffer::GetLastOffset()
{
	return last_offset;
}

vk::Buffer *PlatformStructuredBuffer::GetLastBuffer()
{
	return &lastBuffer->mBuffer;
}

size_t PlatformStructuredBuffer::GetSize()
{
	return mTotalSize;
}