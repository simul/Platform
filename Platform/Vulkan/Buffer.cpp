#ifdef _MSC_VER
    #include <windows.h>
#endif
#include "Buffer.h"
#include "RenderPlatform.h"
#include "Simul/Platform/Vulkan/DeviceManager.h"
#include "Simul/Base/RuntimeError.h"

using namespace simul;
using namespace vulkan;

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
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	if(!vulkanDevice)
		return;
	vulkanDevice->destroyBuffer(mBuffer, nullptr);
	vulkanDevice->freeMemory(mBufferMemory, nullptr);
	vulkanDevice->destroyBuffer(bufferLoad.stagingBuffer, nullptr);
	vulkanDevice->freeMemory(bufferLoad.stagingBufferMemory, nullptr);
	renderPlatform=nullptr;
}

	/*
	uint32_t Buffer::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
	{
		return 0; // for now.
		vk::Device *vulkanDevice	=renderPlatform->AsVulkanDevice();
		//vk::PhysicalDevice *gpu		=deviceManager->GetGPU();
		vk::PhysicalDeviceMemoryProperties memProperties;
		//gpu->getMemoryProperties(&memProperties);

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
	void Buffer::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory)
	{
		vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
		vk::BufferCreateInfo bufferInfo ;
		bufferInfo.setSize(size);
		bufferInfo.setUsage ( usage);
		bufferInfo.setSharingMode ( vk::SharingMode::eExclusive);

		if (vulkanDevice->createBuffer( &bufferInfo, nullptr, &buffer) != vk::Result::eSuccess)
		{
			SIMUL_BREAK("failed to create buffer!");
		}

		vk::MemoryRequirements memRequirements;
		vulkanDevice->getBufferMemoryRequirements( buffer, &memRequirements);

		vk::MemoryAllocateInfo allocInfo ;
		allocInfo.setAllocationSize ( memRequirements.size);
		bool  pass = ((vulkan::RenderPlatform*)renderPlatform)->memory_type_from_properties(
			memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&allocInfo.memoryTypeIndex);
		SIMUL_ASSERT(pass);

		if (vulkanDevice->allocateMemory(&allocInfo, nullptr, &bufferMemory) != vk::Result::eSuccess)
		{
			SIMUL_BREAK("failed to allocate buffer memory!");
		}

		vulkanDevice->bindBufferMemory( buffer, bufferMemory, 0);
	}*/

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* r
								,int num_vertices
								,const crossplatform::Layout* layout
								,const void* src_data
								,bool cpu_access
								,bool streamout_target)
{
    InvalidateDeviceObjects();
	renderPlatform=r;

	stride = layout->GetStructSize();
	bufferLoad.size= num_vertices * layout->GetStructSize();
	
	vulkanRenderPlatform->CreatVulkanBuffer(bufferLoad.size
					,vk::BufferUsageFlagBits::eTransferSrc
					,vk::MemoryPropertyFlagBits::eHostVisible  | vk::MemoryPropertyFlagBits::eHostCoherent
					,bufferLoad.stagingBuffer, bufferLoad.stagingBufferMemory,"vertex buffer upload");

	void* target_data;
	if(src_data)
	{
		vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
		vulkanDevice->mapMemory(bufferLoad.stagingBufferMemory, 0, bufferLoad.size,vk::MemoryMapFlagBits(), &target_data);
		if(target_data)
		{
			memcpy(target_data, src_data, (size_t)bufferLoad.size);
			vulkanDevice->unmapMemory(bufferLoad.stagingBufferMemory);
		}
	}

	vulkanRenderPlatform->CreatVulkanBuffer(bufferLoad.size
				, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer
				, vk::MemoryPropertyFlagBits::eDeviceLocal
				, mBuffer, mBufferMemory,"vertex buffer");
	loadingComplete=false;
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_indices,int index_size_bytes,const void* data)
{
    InvalidateDeviceObjects();
}

void* Buffer::Map(crossplatform::DeviceContext& deviceContext)
{
	loadingComplete=false;
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	void* target_data;
	vulkanDevice->mapMemory(bufferLoad.stagingBufferMemory, 0, bufferLoad.size,vk::MemoryMapFlagBits(), &target_data);
	return target_data;
}

void Buffer::Unmap(crossplatform::DeviceContext& deviceContext)
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vulkanDevice->unmapMemory(bufferLoad.stagingBufferMemory);
}

void Buffer::FinishLoading(crossplatform::DeviceContext& deviceContext)
{
	if(loadingComplete)
		return;
	vk::BufferCopy copyRegion = {};
	copyRegion.setSize(bufferLoad.size);
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer*)deviceContext.platform_context;
	commandBuffer->copyBuffer( bufferLoad.stagingBuffer, mBuffer, 1, &copyRegion);
	loadingComplete=true;
}

vk::Buffer Buffer::asVulkanBuffer()
{
		
	return mBuffer;
}