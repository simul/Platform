#include "Platform/Vulkan/PlatformStructuredBuffer.h"
#include "Platform/Vulkan/RenderPlatform.h"
using namespace simul;
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

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r,int ct,int unit_size,bool cpur,bool,void* init_data,const char *n, crossplatform::BufferUsageHint bufferUsageHint)
{
    renderPlatform                          = r;
	mNumElements		= ct;
	mElementByteSize	= unit_size;
	if(n)
		name=n;
	// Really, if it's cpu-readable, we must have only one copy per-frame.
    if (mCpuRead)
		mMaxApplyCount=1;
    mUnitSize           = mNumElements * mElementByteSize;
    mTotalSize			= mUnitSize * mMaxApplyMod;
	delete [] buffer;
	buffer = nullptr;
	mCpuRead            =cpur;
	mSlots = ((mTotalSize + (kBufferAlign - 1)) & ~ (kBufferAlign - 1)) / kBufferAlign;
	
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
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
				, mReadBuffers[i], mReadBufferMemory[i],(name+"psb read").c_str());
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
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	perFrameBuffers.push_back(PerFrameBuffer());
	PerFrameBuffer &perFrameBuffer=perFrameBuffers.back();
	vulkanRenderPlatform->CreateVulkanBuffer(alloc_size
		, usageFlags
		, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		, perFrameBuffer.mBuffer, perFrameBuffer.mMemory,"psb");
	if(init_data)
	{
		auto pData = vulkanDevice->mapMemory(perFrameBuffer.mMemory, 0, VK_WHOLE_SIZE, vk::MemoryMapFlags());
		SIMUL_ASSERT(pData!=nullptr);
		memcpy(pData, init_data, mTotalSize);
		vulkanDevice->unmapMemory(perFrameBuffer.mMemory);
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
    if (deviceContext.frame_number >= kNumBuffers)
    {
		vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
		mCurReadMap = vulkanDevice->mapMemory(mReadBufferMemory[mFrameIndex], 0, VK_WHOLE_SIZE, vk::MemoryMapFlags());
		return mCurReadMap;
    }
    return nullptr;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (mCurReadMap)
    {
		vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
		vulkanDevice->unmapMemory(mReadBufferMemory[mFrameIndex]);
    }
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer*)deviceContext.platform_context;
	vk::BufferCopy region=vk::BufferCopy().setDstOffset(0).setSize(mTotalSize).setSrcOffset(last_offset);
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

void PlatformStructuredBuffer::ActualApply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectPass *,int slot,bool as_uav) 
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
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
	if (mLastFrame != deviceContext.frame_number || lastBuffer == perFrameBuffers.end())
	{
		if(lastBuffer!=perFrameBuffers.end())
			lastBuffer++;
		if(lastBuffer==perFrameBuffers.end())
			lastBuffer=firstBuffer;
		if(lastBuffer==perFrameBuffers.end())
			SIMUL_BREAK("bad buffer iterator");
		mLastFrame = deviceContext.frame_number;
		mCurApplyCount = 0;
		mFrameIndex = (mFrameIndex + 1) % kNumBuffers;
	}
	if(lastBuffer==perFrameBuffers.end())
		SIMUL_BREAK("bad buffer iterator");
	// pDest points at the begining of the uploadHeap, we can offset it! (we created 64KB and each Constart buffer
	// has a minimum size of kBufferAlign)
	uint8_t* pDest	=nullptr;
	last_offset		=(kBufferAlign * mSlots) * mCurApplyCount;	

	if (!as_uav&&buffer)
	{
		auto pData = vulkanDevice->mapMemory(lastBuffer->mMemory, last_offset, (kBufferAlign * mSlots), vk::MemoryMapFlags());

		if (pData)
		{
			memcpy(pData, buffer, mTotalSize);
			vulkanDevice->unmapMemory(lastBuffer->mMemory);
		}
		mCurApplyCount++;
	}
}


void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext,const crossplatform::ShaderResource &shaderResource)
{
    crossplatform::PlatformStructuredBuffer::ApplyAsUnorderedAccessView(deviceContext,  shaderResource);
	Apply(deviceContext,shaderResource);
}

void PlatformStructuredBuffer::AddFence(crossplatform::DeviceContext& deviceContext)
{
    // Insert a fence:
    if (cpu_read)
    {
     //   int idx     = deviceContext.frame_number % mNumBuffers;
    }
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext& deviceContext)
{
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	if(!vulkanDevice)
		return;
	vulkan::RenderPlatform *rPlat=(vulkan::RenderPlatform *)renderPlatform;
	for(auto i:perFrameBuffers)
    {
		rPlat->PushToReleaseManager(i.mBuffer);
		rPlat->PushToReleaseManager(i.mBufferView);
		rPlat->PushToReleaseManager(i.mMemory);
	}
	perFrameBuffers.clear();
	firstBuffer=lastBuffer=perFrameBuffers.end();
	for(int i=0;i<kNumBuffers;i++)
    {
		rPlat->PushToReleaseManager(mReadBuffers[i]);
		rPlat->PushToReleaseManager(mReadBufferMemory[i]);
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