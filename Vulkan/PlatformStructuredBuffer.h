#pragma once
#include <vulkan/vulkan.hpp>

#include "Platform/Vulkan/Export.h"
#include "Platform/Vulkan/Allocation.h"
#include "Platform/CrossPlatform/Effect.h"
#include <list>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
	#pragma warning(disable:4275)
#endif

namespace platform
{
	namespace vulkan
	{
		//! Vulkan Structured Buffer implementation (SSBO)
		class PlatformStructuredBuffer:public crossplatform::PlatformStructuredBuffer
		{
		public:
						PlatformStructuredBuffer();
			virtual		~PlatformStructuredBuffer();

			void		RestoreDeviceObjects(crossplatform::RenderPlatform *r, int count, int unit_size, bool computable, bool cpu_read, const void *init_data,const char *name, crossplatform::ResourceUsageFrequency bufferUsageHint) override;
			void*		GetBuffer(crossplatform::DeviceContext &deviceContext) override;
			const void* OpenReadBuffer(crossplatform::DeviceContext &deviceContext) override;
			void		CloseReadBuffer(crossplatform::DeviceContext &deviceContext) override;
			void		CopyToReadBuffer(crossplatform::DeviceContext &deviceContext) override;
			void		SetData(crossplatform::DeviceContext &deviceContext,void *data) override;
			void		InvalidateDeviceObjects() override;

			void		ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext, const crossplatform::ShaderResource &shaderResource) override;
			void		AddFence(crossplatform::DeviceContext& deviceContext) ;

			void		Unbind(crossplatform::DeviceContext &deviceContext) override;
			
			void		ActualApply(crossplatform::DeviceContext &,bool) override;

			virtual bool IsValid() const override { return mNumElements>0; }
			size_t GetLastOffset();
			vk::Buffer *GetLastBuffer();
			size_t GetSize();
		private:
			//! Total allocated size for each buffer
			static const int				mBufferSize = 1024 * 64 * 8;
			//! Number of ring buffers
			static const int				kNumBuffers = (SIMUL_VULKAN_FRAME_LAG+1);
			int								mSlots;					//number of 256-byte chunks of memory...
			int								mMaxApplyCount;
			int								mCountMultiplier=2;

			int								mCurApplyCount;

			struct PerFrameBuffer
			{
				vk::Buffer					mBuffer;
				vk::BufferView				mBufferView;
				AllocationInfo				mAllocationInfo;
			};
			std::list<PerFrameBuffer>		perFrameBuffers;
			std::list<PerFrameBuffer>::iterator lastBuffer;
			std::list<PerFrameBuffer>::iterator firstBuffer;
			
			vk::Buffer						mReadBuffers[kNumBuffers];
			AllocationInfo					mReadBufferAllocationInfo[kNumBuffers];
			const int kBufferAlign			= 256;
			size_t							last_offset;

			void* mCurReadMap				= nullptr;
			//! Total number of individual elements
			int								mNumElements;
			//! Size of each element
			int								mElementByteSize;
			int								mUnitSize;
			int								mTotalSize;
			int								mMaxApplyMod = 1;
			unsigned char *					buffer;
			bool							mCpuRead;
			void							AddPerFrameBuffer(const void *init_data);
			uint64_t						mLastFrame;
			int	mFrameIndex;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif