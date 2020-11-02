#pragma once

#include "Platform/DirectX12/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "SimulDirectXHeader.h"
#include "Heap.h"

#pragma warning(disable:4251)

namespace simul
{
	namespace dx12
	{
		class EffectPass;
		class Effect;
		class RenderPlatform;
		//! DirectX12 structured buffer class
		class PlatformStructuredBuffer:public crossplatform::PlatformStructuredBuffer
		{
		public:
											PlatformStructuredBuffer();
			virtual							~PlatformStructuredBuffer();
			void							RestoreDeviceObjects(crossplatform::RenderPlatform* renderPlatform,int ct,int unit_size,bool computable,bool cpu_read,void *init_data,const char *name, crossplatform::BufferUsageHint b);
			void							Apply(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const crossplatform::ShaderResource &shaderResource);
            void                            ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const crossplatform::ShaderResource &shaderResource);
			//! Returns an initialized pointer with the size of this structured buffer that can be used to set data. After
			//! changing the data of this pointer, we must Apply this SB so the changes will be uploaded to the GPU.
			void*							GetBuffer(crossplatform::DeviceContext &deviceContext);
			//! Returns a pointer to the data of this structured buffer. This operation has a latency of
			//! mReadTotalCnt - 1 frames.
			const void*						OpenReadBuffer(crossplatform::DeviceContext &deviceContext);
			//! After opening the buffer, we close it.
			void							CloseReadBuffer(crossplatform::DeviceContext &deviceContext);
			//! Scheudles a copy so we can latter read the data.
			void							CopyToReadBuffer(crossplatform::DeviceContext &deviceContext);
			void							SetData(crossplatform::DeviceContext &deviceContext,void *data);
			void							InvalidateDeviceObjects();
			void							Unbind(crossplatform::DeviceContext &deviceContext);
			D3D12_CPU_DESCRIPTOR_HANDLE*    AsD3D12ShaderResourceView(crossplatform::DeviceContext &deviceContext);
			D3D12_CPU_DESCRIPTOR_HANDLE*    AsD3D12UnorderedAccessView(crossplatform::DeviceContext &deviceContext,int = 0);
			ID3D12Resource*					AsD3D12Resource(crossplatform::DeviceContext &deviceContext);
			void							ActualApply(simul::crossplatform::DeviceContext& deviceContext, EffectPass* currentEffectPass, int slot);

		private:
            //! If we called GetBuffer, we need to update the GPU data, this methods, handles updating the data at 
            //! different offsets + CPU<->GPU memory transition
            void                            UpdateBuffer(simul::crossplatform::DeviceContext& deviceContext);

			static const unsigned int		mBuffering = 3;
			ID3D12Resource*					mGPUBuffer;
			ID3D12Resource*					mUploadBuffer;
			ID3D12Resource*					mReadBuffers[mBuffering];

			//! We hold the currently mapped pointer
			UINT8*							mReadSrc;
			//! Temporal buffer used to upload data to the GPU
			void*							mTempBuffer;
			//! If true, we have to update the GPU data
			bool							mChanged;

			dx12::Heap						mBufferSrvHeap;
			dx12::Heap						mBufferUavHeap;

            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> mSrvViews;
            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> mUavViews;

			D3D12_RESOURCE_STATES			mCurrentState;

			//! Total number of individual elements
			int								mNumElements;
			//! Size of each element
			int								mElementByteSize;
			//! Total size (size of one unit * max applies)
			int								mTotalSize;
            int								mUnitSize;
            //! Does this buffer allow reads from CPU?
            bool							mCpuRead;

            //! How many times we can Apply this SB with different data
            //! During runtime we will check the current applies and recreate if needed
            int								mMaxApplyMod = 6;
            int								mCurApplies;
            uint64_t						mLastFrame;
			int								mFrameCycle=0;
		};
	}
}
