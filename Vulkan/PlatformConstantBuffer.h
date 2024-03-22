#pragma once

#include "Platform/Vulkan/Export.h"
#include "Platform/Vulkan/Allocation.h"
#include "Platform/CrossPlatform/Effect.h"
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
	#pragma warning(disable:4275)
#endif

namespace platform
{
	namespace vulkan
	{
		//! Vulkan Constant Buffer implementation (UBO)
		class SIMUL_VULKAN_EXPORT PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
		public:
			PlatformConstantBuffer(crossplatform::ResourceUsageFrequency F);
			~PlatformConstantBuffer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t sz,void* addr) override;
			void InvalidateDeviceObjects() override;
			void Apply(crossplatform::DeviceContext& deviceContext,size_t size,void* addr) override;
			void Unbind(crossplatform::DeviceContext& deviceContext) override;

			void ActualApply(crossplatform::DeviceContext &) override;

			size_t GetLastOffset();
			vk::Buffer *GetLastBuffer();
			size_t GetSize();
		private:
			//! Total allocated size for each buffer
			unsigned						mBufferSize = 1024 * 64 * 8;
			//! Number of ring buffers
			static const unsigned			kNumBuffers = (SIMUL_VULKAN_FRAME_LAG+1);
			unsigned						mSlots;					//number of 256-byte chunks of memory...
			unsigned						mMaxAppliesPerFrame=0;

			int64_t							mLastFrameIndex;
			unsigned						mCurApplyCount;

			vk::Buffer 						mBuffers[kNumBuffers];
			AllocationInfo					mAllocationInfo[kNumBuffers];

			const int kBufferAlign			= 256;
			void *src;
			size_t size;
			size_t last_offset;
			vk::Buffer *lastBuffer;
			unsigned char currentFrameIndex = 0;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif