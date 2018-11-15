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
			void RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t sz,void* addr);
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect* effect,const char* name,int bindingIndex);
			void Apply(crossplatform::DeviceContext& deviceContext,size_t size,void* addr);
			void Unbind(crossplatform::DeviceContext& deviceContext);

			void ActualApply(crossplatform::DeviceContext &,crossplatform::EffectPass *,int) override;

			size_t GetLastOffset();
			vk::Buffer *GetLastBuffer();
			size_t GetSize();
        private:
			//! Total allocated size for each buffer
			static const UINT				mBufferSize = 1024 * 64 * 8;
			//! Number of ring buffers
			static const UINT				kNumBuffers = (SIMUL_VULKAN_FRAME_LAG+1);
			UINT							mSlots;					//number of 256-byte chunks of memory...
			UINT							mMaxDescriptors;

			UINT							mLastFrameIndex;
			UINT							mCurApplyCount;

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