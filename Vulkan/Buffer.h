#pragma once
#include <vulkan/vulkan.hpp>

#include "Platform/Vulkan/Export.h"
#include "Platform/CrossPlatform/Buffer.h"
#include "Platform/Vulkan/Allocation.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace platform
{
	namespace vulkan
	{
		//! Vulkan buffer (vertex/index) implementation
		class SIMUL_VULKAN_EXPORT Buffer : public platform::crossplatform::Buffer
		{
		public:
						Buffer();
						~Buffer() override;
			void		InvalidateDeviceObjects() override;
			void		EnsureVertexBuffer(crossplatform::RenderPlatform* r,int num_vertices,const crossplatform::Layout* layout,std::shared_ptr<std::vector<uint8_t>> data,bool cpu_access=false,bool streamout_target=false) override;
			void		EnsureIndexBuffer(crossplatform::RenderPlatform* r,int num_indices,int index_size_bytes,std::shared_ptr<std::vector<uint8_t>> data, bool cpu_access = false) override;
			void*		Map(crossplatform::DeviceContext& deviceContext) override;
			void		Unmap(crossplatform::DeviceContext& deviceContext) override;

			vk::Buffer	asVulkanBuffer();
			void		FinishLoading(crossplatform::DeviceContext& deviceContext);
		private:
			vk::Buffer mBuffer;
			AllocationInfo mAllocation;
			
			vk::Buffer mStagingBuffer;
			AllocationInfo mStagingAllocation;
			
			uint32_t size;
			bool loadingComplete = true;
		};
	}
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
