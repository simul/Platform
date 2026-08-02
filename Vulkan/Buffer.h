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
			void		EnsureVertexBuffer(crossplatform::RenderPlatform* r,int num_vertices,int stride,std::shared_ptr<std::vector<uint8_t>> data,bool cpu_access=false,bool streamout_target=false) override;
			void		EnsureIndexBuffer(crossplatform::RenderPlatform* r,int num_indices,int index_size_bytes,std::shared_ptr<std::vector<uint8_t>> data, bool cpu_access = false) override;
			void*		Map(crossplatform::DeviceContext& deviceContext) override;
			void		Unmap(crossplatform::DeviceContext& deviceContext) override;

			vk::Buffer	asVulkanBuffer();
			void		FinishLoading(crossplatform::DeviceContext& deviceContext);
		private:
			//! Allocate the ring of staging and device buffers. numSlots must be set first.
			void		CreateBuffers(vk::BufferUsageFlags deviceUsage, const char* name);

			//! A dynamic buffer is written by the CPU every frame while previous frames may still be in flight,
			//! so it needs one slot per frame in flight, in the same way as PlatformConstantBuffer and
			//! PlatformStructuredBuffer. A static buffer is written once and needs only one slot.
			static const uint32_t kNumBuffers = SIMUL_VULKAN_FRAME_LAG + 1;

			vk::Buffer mBuffers[kNumBuffers];
			AllocationInfo mAllocations[kNumBuffers];

			vk::Buffer mStagingBuffers[kNumBuffers];
			AllocationInfo mStagingAllocations[kNumBuffers];

			bool loadingComplete[kNumBuffers] = {};

			//! 1 for a static buffer, kNumBuffers for a cpu_access buffer.
			uint32_t numSlots = 1;
			//! The slot written by the most recent Map(), and read by asVulkanBuffer() and FinishLoading().
			uint32_t currentSlot = 0;

			uint32_t size;
		};
	}
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
