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
		//! Vulkan Constant Buffer implementation (UBO)
		class SIMUL_VULKAN_EXPORT PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
		public:
                PlatformConstantBuffer();
                ~PlatformConstantBuffer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t sz,void* addr) override;
			void InvalidateDeviceObjects() override;
			void LinkToEffect(crossplatform::Effect* effect,const char* name,int bindingIndex)override;
			void Apply(crossplatform::DeviceContext& deviceContext,size_t size,void* addr) override;
			void Unbind(crossplatform::DeviceContext& deviceContext) override;

			void ActualApply(crossplatform::DeviceContext &,crossplatform::EffectPass *,int) override;

			size_t GetLastOffset();
			vk::Buffer *GetLastBuffer();
			size_t GetSize();
        private:
			//! Total allocated size for each buffer
			static const unsigned			mBufferSize = 1024 * 64 * 8;
			//! Number of ring buffers
			static const unsigned			kNumBuffers = (SIMUL_VULKAN_FRAME_LAG+1);
			unsigned						mSlots;					//number of 256-byte chunks of memory...
			unsigned						mMaxDescriptors;

			unsigned						mLastFrameIndex;
			unsigned						mCurApplyCount;

			vk::Buffer 						mBuffers[kNumBuffers];
			vk::DeviceMemory				mMemory[kNumBuffers];

			const int kBufferAlign			= 256;
			void *src;
			size_t size;
			size_t last_offset;
			vk::Buffer *lastBuffer;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif