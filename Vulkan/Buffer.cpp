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
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	if(!vulkanDevice)
		return;
	vulkan::RenderPlatform *rp=(vulkan::RenderPlatform *)renderPlatform;
	rp->PushToReleaseManager(mBuffer, &mAllocation);
	rp->PushToReleaseManager(mStagingBuffer, &mStagingAllocation);
	renderPlatform=nullptr;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* r
								,int num_vertices
								,const crossplatform::Layout* layout
								,std::shared_ptr<std::vector<uint8_t>> src_data
								,bool cpu_access
								,bool streamout_target)
{
	InvalidateDeviceObjects();
	renderPlatform=r;

	stride = layout->GetStructSize();
	size = num_vertices * layout->GetStructSize();
	count = num_vertices;
	
	vulkanRenderPlatform->CreateVulkanBuffer(
		size, vk::BufferUsageFlagBits::eTransferSrc, 
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
		mStagingBuffer, mStagingAllocation, "VertexBuffer Upload");

	if(src_data)
	{
		void* target_data = nullptr;
		SIMUL_VK_CHECK((vk::Result)vmaMapMemory(mStagingAllocation.allocator, mStagingAllocation.allocation, &target_data));
		if (target_data)
		{
			memcpy(target_data, src_data->data(), (size_t)size);
			vmaUnmapMemory(mStagingAllocation.allocator, mStagingAllocation.allocation);
		}
	}

	vulkanRenderPlatform->CreateVulkanBuffer(
		size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal, mBuffer, mAllocation, "VertexBuffer");

	loadingComplete=false;
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* r,int num_indices,int index_size_bytes,std::shared_ptr<std::vector<uint8_t>> src_data, bool cpu_access )
{
	InvalidateDeviceObjects();
	renderPlatform = r;

	bufferType=crossplatform::BufferType::INDEX;
	stride = index_size_bytes;
	size = num_indices * index_size_bytes;
	count = num_indices;

	vulkanRenderPlatform->CreateVulkanBuffer(
		size, vk::BufferUsageFlagBits::eTransferSrc, 
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		mStagingBuffer, mStagingAllocation, "IndexBuffer Upload");

	if (src_data)
	{
		void *target_data = nullptr;
		SIMUL_VK_CHECK((vk::Result)vmaMapMemory(mStagingAllocation.allocator, mStagingAllocation.allocation, &target_data));
		if (target_data)
		{
			memcpy(target_data, src_data->data(), (size_t)size);
			vmaUnmapMemory(mStagingAllocation.allocator, mStagingAllocation.allocation);
		}
	}

	vulkanRenderPlatform->CreateVulkanBuffer(
		size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal, mBuffer, mAllocation, "IndexBuffer");

	loadingComplete = false;
}

void* Buffer::Map(crossplatform::DeviceContext& deviceContext)
{
	loadingComplete = false;
	void *target_data = nullptr;
	vk::Result result = (vk::Result)vmaMapMemory(mStagingAllocation.allocator, mStagingAllocation.allocation, &target_data);
	if (result != vk::Result::eSuccess)
		return nullptr;
	return target_data;
}

void Buffer::Unmap(crossplatform::DeviceContext& deviceContext)
{
	vmaUnmapMemory(mStagingAllocation.allocator, mStagingAllocation.allocation);
}

void Buffer::FinishLoading(crossplatform::DeviceContext& deviceContext)
{
	if(loadingComplete)
		return;

	vulkanRenderPlatform->EndRenderPass(deviceContext);

	vk::BufferCopy copyRegion = {};
	copyRegion.setSize(size);
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer*)deviceContext.platform_context;
	commandBuffer->copyBuffer(mStagingBuffer, mBuffer, 1, &copyRegion);
	loadingComplete = true;
}

vk::Buffer Buffer::asVulkanBuffer()
{
	return mBuffer;
}