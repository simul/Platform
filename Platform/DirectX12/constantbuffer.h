#pragma once

#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "SimulDirectXHeader.h"
#include "Heap.h"

#pragma warning(disable:4251)

namespace simul
{
	namespace dx12
	{
		//! DirectX 12 Constant buffer class implementation
		class PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{	
		public:
			PlatformConstantBuffer();
			~PlatformConstantBuffer();

			//! Returns the CPU descriptor handle of the current constant buffer, we need to apply the buffer at least once
			D3D12_CPU_DESCRIPTOR_HANDLE AsD3D12ConstantBuffer	();
			//! Here we clear the resources and also we calculate how many slots we will be using.
			void RestoreDeviceObjects							(crossplatform::RenderPlatform *r,size_t sz,void *addr);
			void InvalidateDeviceObjects						();
			void LinkToEffect									(crossplatform::Effect *effect,const char *name,int bindingIndex);
			//! This method copies the provided data into the buffer.
			void Apply											(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr);
			void ActualApply									(crossplatform::DeviceContext &deviceContext, crossplatform::EffectPass *currentEffectPass, int slot) override;
			void Unbind											(simul::crossplatform::DeviceContext &deviceContext);
			//! This method creates the heaps and the resources, it allocates GPU memory.
			void CreateBuffers									(crossplatform::RenderPlatform* r, void *addr);
			void SetNumBuffers									(crossplatform::RenderPlatform* r,UINT nContexts,  UINT nMapsPerFrame, UINT nFramesBuffering );
			
		private:
			//! Total allocated size for each buffer
			static const UINT				mBufferSize = 1024 * 64 * 2;
			//! Number of ring buffers
			static const UINT				kNumBuffers = 3;
			UINT							mSlots;
			UINT							mMaxDescriptors;

			UINT							mLastFrameIndex;
			UINT							mCurApplyCount;

			ID3D12Resource*					mUploadHeap[kNumBuffers];
			dx12::Heap						mHeaps[kNumBuffers];

			const int kBufferAlign			= D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		};
	}
}
