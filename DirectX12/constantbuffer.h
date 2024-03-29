#pragma once

#include "Platform/DirectX12/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "SimulDirectXHeader.h"
#include "Heap.h"

#pragma warning(disable:4251)

namespace platform
{
	namespace dx12
	{
		//! DirectX 12 Constant buffer class implementation
		class PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{	
		public:
			PlatformConstantBuffer(crossplatform::ResourceUsageFrequency F);
			~PlatformConstantBuffer();

			//! Returns the CPU descriptor handle of the current constant buffer, we need to apply the buffer at least once
			D3D12_CPU_DESCRIPTOR_HANDLE AsD3D12ConstantBuffer	();
			//! Here we clear the resources and also we calculate how many slots we will be using.
			void RestoreDeviceObjects							(crossplatform::RenderPlatform *r,size_t sz,void *addr);
			void InvalidateDeviceObjects						();
			void LinkToEffect									(crossplatform::Effect *effect,const char *name,int bindingIndex);
			//! This method copies the provided data into the buffer.
			void Apply											(platform::crossplatform::DeviceContext &deviceContext,size_t size,void *addr);
			void ActualApply									(crossplatform::DeviceContext &deviceContext) override;
			void Unbind											(platform::crossplatform::DeviceContext &deviceContext);
			//! This method creates the heaps and the resources, it allocates GPU memory.
			void CreateBuffers									(crossplatform::RenderPlatform* r, void *addr);
			void SetNumBuffers									(crossplatform::RenderPlatform* r,UINT nContexts,  UINT nMapsPerFrame, UINT nFramesBuffering );
			
		private:
			//! Total allocated size for each buffer
			UINT							mBufferSize = 1024 * 64 * 8;
			//! Number of ring buffers
			static const UINT				kNumBuffers = 3;
			UINT							mSlots;				//number of 256-byte chunks of memory...
			UINT							mMaxDescriptors;

			long long						last_frame_number=UINT_MAX;
			UINT							buffer_index=0;
			UINT							mCurApplyCount;

			ID3D12Resource*					mUploadHeap[kNumBuffers];
			AllocationInfo					mUploadAllocationInfos[kNumBuffers];
			dx12::Heap						mHeaps[kNumBuffers];

			const int kBufferAlign			= D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
			D3D12_CPU_DESCRIPTOR_HANDLE		*cpuDescriptorHandles[kNumBuffers];

			// point to whatever Apply sent us.
			void* src=nullptr;
			size_t size=0;
		};
	}
}
