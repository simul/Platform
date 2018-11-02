#include "Simul/Platform/Vulkan/PlatformStructuredBuffer.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"
using namespace simul;
using namespace vulkan;


PlatformStructuredBuffer::PlatformStructuredBuffer()
	:mSlots(0)
	,mMaxDescriptors(0)
	,mLastFrameIndex(0)
	,mCurApplyCount(0)
	,last_offset(0)
	,lastBuffer(0)
	,mCpuRead(false)
	,buffer(nullptr)
{
}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
    InvalidateDeviceObjects();
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r,int ct,int unit_size,bool cpu_read,bool,void* init_data)
{
	HRESULT res			= S_FALSE;
	mNumElements		= ct;
	mElementByteSize	= unit_size;
	
    mUnitSize           = mNumElements * mElementByteSize;
    mTotalSize			= mUnitSize * mMaxApplyMod;
	delete [] buffer;
	buffer=new unsigned char [mTotalSize];
    renderPlatform                          = r;
	vulkan::RenderPlatform* mRenderPlatform	= (vulkan::RenderPlatform*)renderPlatform;
	mCpuRead                                = cpu_read;
	mSlots = ((mTotalSize + (kBufferAlign - 1)) & ~ (kBufferAlign - 1)) / kBufferAlign;
	
	vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
		.setSize(mTotalSize)
		.setUsage(vk::BufferUsageFlagBits::eStorageBuffer);
	vk::Device *device=renderPlatform->AsVulkanDevice();
	for (unsigned int i = 0; i < kNumBuffers; i++)
	{
		auto result = device->createBuffer(&buf_info, nullptr, &mBuffers[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

		vk::MemoryRequirements mem_reqs;
		device->getBufferMemoryRequirements(mBuffers[i], &mem_reqs);

		auto mem_alloc = vk::MemoryAllocateInfo().setAllocationSize(mem_reqs.size).setMemoryTypeIndex(0);

		bool  pass=((vulkan::RenderPlatform*)renderPlatform)->memory_type_from_properties(
			mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&mem_alloc.memoryTypeIndex);
		SIMUL_ASSERT(pass);

		result = device->allocateMemory(&mem_alloc, nullptr, &mMemory[i]);
		SIMUL_ASSERT(result == vk::Result::eSuccess);
		if(init_data)
		{
			auto pData = device->mapMemory(mMemory[i], 0, VK_WHOLE_SIZE, vk::MemoryMapFlags());
			SIMUL_ASSERT(pData!=nullptr);
	
			memcpy(pData, init_data, mTotalSize);

			device->unmapMemory(mMemory[i]);
		}

		device->bindBufferMemory(mBuffers[i], mMemory[i], 0);
		SIMUL_ASSERT(result == vk::Result::eSuccess);

	//	vk::BufferViewCreateInfo bufferViewCreateInfo=vk::BufferViewCreateInfo().setBuffer(mBuffers[i])
			//.setRange(VK_WHOLE_SIZE).setOffset(0);

			//device->createBufferView(&bufferViewCreateInfo,nullptr,&mBufferViews[i]);
	}

	// Create the "Descriptor Pool":
	vk::DescriptorPoolSize const poolSizes[1] =
		{
			vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(kNumBuffers)
		};

	auto const descriptor_pool =
		vk::DescriptorPoolCreateInfo().setMaxSets(kNumBuffers).setPoolSizeCount(1).setPPoolSizes(poolSizes);

	auto result = device->createDescriptorPool(&descriptor_pool, nullptr, &mDescriptorPool);
	lastBuffer=nullptr;
	last_offset=0;
	
	last_offset=0;
	mMaxDescriptors=1;
	SIMUL_ASSERT(result == vk::Result::eSuccess);

}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext& deviceContext)
{
	return buffer;
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (deviceContext.frame_number >= kNumBuffers)
    {
    }
    return nullptr;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (mCurReadMap)
    {
    }
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
}

void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext& deviceContext,void* data)
{
    if (data)
    {
       memcpy(buffer,data,mTotalSize);
    }
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext,crossplatform::Effect* effect,const crossplatform::ShaderResource &shaderResource)
{
	crossplatform::PlatformStructuredBuffer::Apply(deviceContext,effect,shaderResource);
    int idx = deviceContext.frame_number % kNumBuffers;
 //   if (mBinding == -1)
    {
   //     mBinding = shaderResource.slot;
    }
    //if (mGPUIsMapped)
    {
   //     mGPUIsMapped = false;
    }
}


void PlatformStructuredBuffer::ActualApply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectPass *,int) 
{
	vk::Device *device=renderPlatform->AsVulkanDevice();
	if (mCurApplyCount >= mMaxDescriptors)
	{
		// This should really be solved by having like some kind of pool? Or allocating more space, something like that
		SIMUL_BREAK_ONCE("This PlatformStructuredBuffer reached its maximum apply count");
		return;
	}

	auto rPlat = (vulkan::RenderPlatform*)deviceContext.renderPlatform;
	auto curFrameIndex = rPlat->GetIdx();
	// If new frame, update current frame index and reset the apply count
	if (mLastFrameIndex != curFrameIndex)
	{
		mLastFrameIndex = curFrameIndex;
		mCurApplyCount = 0;
	}

	// pDest points at the begining of the uploadHeap, we can offset it! (we created 64KB and each Constart buffer
	// has a minimum size of kBufferAlign)
	UINT8* pDest = nullptr;
	last_offset = (kBufferAlign * mSlots) * mCurApplyCount;	
	lastBuffer=&mBuffers[curFrameIndex];

	auto pData = device->mapMemory(mMemory[curFrameIndex],last_offset, (kBufferAlign * mSlots),  vk::MemoryMapFlags());
	
	if(pData&&buffer)
	{
		memcpy(pData,buffer, mTotalSize);
		device->unmapMemory(mMemory[curFrameIndex]);
	}
	
	mCurApplyCount++;
}


void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext,crossplatform::Effect* effect,const crossplatform::ShaderResource &shaderResource)
{
    crossplatform::PlatformStructuredBuffer::ApplyAsUnorderedAccessView(deviceContext, effect, shaderResource);
	Apply(deviceContext,effect,shaderResource);
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
  //  if (mGPUBuffer[0] != 0)
    {
    }
}

size_t PlatformStructuredBuffer::GetLastOffset()
{
	return last_offset;
}

vk::Buffer *PlatformStructuredBuffer::GetLastBuffer()
{
	return lastBuffer;
}

size_t PlatformStructuredBuffer::GetSize()
{
	return mTotalSize;
}