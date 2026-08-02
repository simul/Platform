#include "Buffer.h"
#include "RenderPlatform.h"
#include "Platform/Vulkan/DeviceManager.h"
#include "Platform/Core/RuntimeError.h"

using namespace platform;
using namespace vulkan;

#define VK_CHECK(result)\
{\
	if(result!=vk::Result::VK_SUCCESS)\
	{\
	}\
}

Buffer::Buffer()
{
}

Buffer::~Buffer()
{
	InvalidateDeviceObjects();
}

void Buffer::InvalidateDeviceObjects()
{
	this->bufferType=crossplatform::BufferType::UNKNOWN;
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	if(!vulkanDevice)
		return;
	vulkan::RenderPlatform *rp=(vulkan::RenderPlatform *)renderPlatform;
	for(uint32_t i=0;i<numSlots;i++)
	{
		rp->PushToReleaseManager(mBuffers[i], &mAllocations[i]);
		rp->PushToReleaseManager(mStagingBuffers[i], &mStagingAllocations[i]);
		mBuffers[i]=vk::Buffer();
		mStagingBuffers[i]=vk::Buffer();
		loadingComplete[i]=false;
	}
	numSlots=1;
	currentSlot=0;
	renderPlatform=nullptr;
}

//! Allocate the ring: a host-visible staging buffer and a device-local buffer per slot.
void Buffer::CreateBuffers(vk::BufferUsageFlags deviceUsage, const char* name)
{
	for(uint32_t i=0;i<numSlots;i++)
	{
		vulkanRenderPlatform->CreateVulkanBuffer(nullptr,
			size, vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			mStagingBuffers[i], mStagingAllocations[i], (std::string(name)+" Upload").c_str());

		vulkanRenderPlatform->CreateVulkanBuffer(nullptr,
			size, vk::BufferUsageFlagBits::eTransferDst | deviceUsage,
			vk::MemoryPropertyFlagBits::eDeviceLocal, mBuffers[i], mAllocations[i], name);

		loadingComplete[i]=false;
	}
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* r
								,int num_vertices
								,int str
								,std::shared_ptr<std::vector<uint8_t>> src_data
								,bool cpu_access
								,bool streamout_target)
{
	InvalidateDeviceObjects();
	this->bufferType=crossplatform::BufferType::VERTEX;
	renderPlatform=r;

	stride = str;
	size = num_vertices * stride;
	count = num_vertices;
	// A cpu_access buffer is rewritten every frame while earlier frames may still be reading it, so it needs one slot per frame in flight.
	numSlots = cpu_access ? kNumBuffers : 1;
	currentSlot = 0;

	CreateBuffers(vk::BufferUsageFlagBits::eVertexBuffer, "VertexBuffer");

	if(src_data)
	{
		for(uint32_t i=0;i<numSlots;i++)
		{
			void* target_data = nullptr;
			SIMUL_VK_CHECK((vk::Result)vmaMapMemory(mStagingAllocations[i].allocator, mStagingAllocations[i].allocation, &target_data));
			if (target_data)
			{
				memcpy(target_data, src_data->data(), (size_t)size);
				vmaUnmapMemory(mStagingAllocations[i].allocator, mStagingAllocations[i].allocation);
			}
		}
	}
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* r,int num_indices,int index_size_bytes,std::shared_ptr<std::vector<uint8_t>> src_data, bool cpu_access )
{
	InvalidateDeviceObjects();
	renderPlatform = r;

	bufferType=crossplatform::BufferType::INDEX;
	stride = index_size_bytes;
	size = num_indices * index_size_bytes;
	count = num_indices;
	// A cpu_access buffer is rewritten every frame while earlier frames may still be reading it, so it needs one slot per frame in flight.
	numSlots = cpu_access ? kNumBuffers : 1;
	currentSlot = 0;

	CreateBuffers(vk::BufferUsageFlagBits::eIndexBuffer, "IndexBuffer");

	if (src_data)
	{
		for(uint32_t i=0;i<numSlots;i++)
		{
			void *target_data = nullptr;
			SIMUL_VK_CHECK((vk::Result)vmaMapMemory(mStagingAllocations[i].allocator, mStagingAllocations[i].allocation, &target_data));
			if (target_data)
			{
				memcpy(target_data, src_data->data(), (size_t)size);
				vmaUnmapMemory(mStagingAllocations[i].allocator, mStagingAllocations[i].allocation);
			}
		}
	}
}

void* Buffer::Map(crossplatform::DeviceContext& deviceContext)
{
	// Advance the ring on write, not on frame change: a buffer that is written once and drawn for many frames
	// must keep returning the slot that holds its data.
	if(numSlots>1)
		currentSlot = (currentSlot + 1) % numSlots;
	loadingComplete[currentSlot] = false;
	void *target_data = nullptr;
	vk::Result result = (vk::Result)vmaMapMemory(mStagingAllocations[currentSlot].allocator, mStagingAllocations[currentSlot].allocation, &target_data);
	if (result != vk::Result::eSuccess)
		return nullptr;
	return target_data;
}

void Buffer::Unmap(crossplatform::DeviceContext& deviceContext)
{
	vmaUnmapMemory(mStagingAllocations[currentSlot].allocator, mStagingAllocations[currentSlot].allocation);
}

void Buffer::FinishLoading(crossplatform::DeviceContext& deviceContext)
{
	if(loadingComplete[currentSlot]||!mBuffers[currentSlot])
		return;

	vulkanRenderPlatform->EndRenderPass(deviceContext);

	vk::BufferCopy copyRegion = {};
	copyRegion.setSize(size);
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer*)deviceContext.platform_context;
	commandBuffer->copyBuffer(mStagingBuffers[currentSlot], mBuffers[currentSlot], 1, &copyRegion);

	// The draw that follows reads this buffer at the vertex input stage. Without an explicit dependency the fetch
	// may run before the copy has landed, which shows up as individual vertices - a single glyph, in the case of the
	// ImGui debug overlay - flickering between frames.
	vk::BufferMemoryBarrier barrier;
	barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
		.setDstAccessMask(bufferType==crossplatform::BufferType::INDEX ? vk::AccessFlagBits::eIndexRead : vk::AccessFlagBits::eVertexAttributeRead)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setBuffer(mBuffers[currentSlot])
		.setOffset(0)
		.setSize(size);
	commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput,
								   vk::DependencyFlags(), 0, nullptr, 1, &barrier, 0, nullptr);

	loadingComplete[currentSlot] = true;
}

vk::Buffer Buffer::asVulkanBuffer()
{
	return mBuffers[currentSlot];
}