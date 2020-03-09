#pragma once

#include "Platform/Vulkan/Export.h"
#include "Platform/CrossPlatform/Buffer.h"
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
        //! Vulkan buffer (vertex/index) implementation
		class SIMUL_VULKAN_EXPORT Buffer:public simul::crossplatform::Buffer
		{
		public:
						Buffer();
						~Buffer() override;
			void		InvalidateDeviceObjects() override;
			void		EnsureVertexBuffer(crossplatform::RenderPlatform* r,int num_vertices,const crossplatform::Layout* layout,const void* data,bool cpu_access=false,bool streamout_target=false) override;
			void		EnsureIndexBuffer(crossplatform::RenderPlatform* r,int num_indices,int index_size_bytes,const void *data) override;
			void*		Map(crossplatform::DeviceContext& deviceContext) override;
			void		Unmap(crossplatform::DeviceContext& deviceContext) override;

			vk::Buffer	asVulkanBuffer();
			void		FinishLoading(crossplatform::DeviceContext& deviceContext);
        private:
			vk::Buffer mBuffer;
			vk::DeviceMemory mBufferMemory;
			
			//uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
			//void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory);
			struct BufferLoad
			{
				uint32_t size;
				vk::Buffer stagingBuffer;
				vk::DeviceMemory stagingBufferMemory;
			};
			BufferLoad bufferLoad;
			bool loadingComplete=true;
		};
	}
};

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
