#pragma once
#include "Platform/DirectX12/Export.h"
#include "SimulDirectXHeader.h"
#include "Platform/CrossPlatform/VideoBuffer.h"

#pragma warning(disable:4251)

namespace simul
{
	namespace dx12
	{
		/// DirectX12 buffer class that can be used as index buffer and vertex buffer
		class SIMUL_DIRECTX12_EXPORT VideoBuffer : public simul::crossplatform::VideoBuffer
		{
		public:
			VideoBuffer();
			~VideoBuffer();
			void InvalidateDeviceObjects();
			void EnsureBuffer(crossplatform::RenderPlatform* r, void* graphicsContext, void* videoContext, crossplatform::VideoBufferType bufferType, const void* data, uint32_t dataSize) override;
			void* Map(void* graphicsContext, void* videoContext) override;
			void Unmap(void* graphicsContext, void* videoContext) override;
			void Update(void* graphicsContext, void* videoContext, const void* data, uint32_t dataSize) override;
			ID3D12Resource * const AsD3D12Buffer()  const override
			{
				return mGpuHeap;
			}
			ID3D12Resource *  AsD3D12Buffer() override 
			{
				return mGpuHeap;
			}
		private:
			ID3D12Resource*				mGpuHeap;
			ID3D12Resource*				mIntermediateHeap;
			UINT32						mBufferSize;
			UINT8*						mGpuMappedPtr;
			crossplatform::RenderPlatform* renderPlatform = nullptr;
		};
	}
};

