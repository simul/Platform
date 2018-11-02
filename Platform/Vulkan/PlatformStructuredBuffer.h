#pragma once

#include "Simul/Platform/Vulkan/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
        //! Vulkan Structured Buffer implementation (SSBO)
		class PlatformStructuredBuffer:public crossplatform::PlatformStructuredBuffer
		{
		public:
			            PlatformStructuredBuffer();
            virtual     ~PlatformStructuredBuffer();

			void        RestoreDeviceObjects(crossplatform::RenderPlatform *r, int count, int unit_size, bool computable, bool cpu_read, void *init_data);
			void*       GetBuffer(crossplatform::DeviceContext &deviceContext);
			const void* OpenReadBuffer(crossplatform::DeviceContext &deviceContext);
			void        CloseReadBuffer(crossplatform::DeviceContext &deviceContext);
			void        CopyToReadBuffer(crossplatform::DeviceContext &deviceContext);
			void        SetData(crossplatform::DeviceContext &deviceContext,void *data);
			void        InvalidateDeviceObjects();

			void        Apply(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect, const crossplatform::ShaderResource &shaderResource);
			void        ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect, const crossplatform::ShaderResource &shaderResource);
            void        AddFence(crossplatform::DeviceContext& deviceContext);

            void        Unbind(crossplatform::DeviceContext &deviceContext);
			
			void ActualApply(crossplatform::DeviceContext &,crossplatform::EffectPass *,int) override;

			size_t GetLastOffset();
			vk::Buffer *GetLastBuffer();
			size_t GetSize();
        private:
			//! Total allocated size for each buffer
			static const int				mBufferSize = 1024 * 64 * 8;
			//! Number of ring buffers
			static const int				kNumBuffers = (SIMUL_VULKAN_FRAME_LAG+1);
			int								mSlots;					//number of 256-byte chunks of memory...
			int								mMaxDescriptors;

			int								mLastFrameIndex;
			int								mCurApplyCount;

			vk::Buffer 						mBuffers[kNumBuffers];
			vk::BufferView 					mBufferViews[kNumBuffers];
			vk::DeviceMemory				mMemory[kNumBuffers];
			vk::DescriptorSet				mDescriptorSet[kNumBuffers];
			vk::DescriptorPool				mDescriptorPool;
			const int kBufferAlign			= 256;
			size_t							last_offset;
			vk::Buffer						*lastBuffer;

            void* mCurReadMap				= nullptr;
			//! Total number of individual elements
			int								mNumElements;
			//! Size of each element
			int								mElementByteSize;
			int								mUnitSize;
			int								mTotalSize;
            int                         mMaxApplyMod = 1;
			unsigned char *buffer;
			bool mCpuRead;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif