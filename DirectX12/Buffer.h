#pragma once
#include "SimulDirectXHeader.h"
#include "Platform/DirectX12/Export.h"
#include "Platform/CrossPlatform/Buffer.h"


#pragma warning(disable:4251)

namespace simul
{
	namespace dx12
	{
		/// DirectX12 buffer class that can be used as index buffer and vertex buffer
		class SIMUL_DIRECTX12_EXPORT Buffer:public simul::crossplatform::Buffer
		{
		public:
										Buffer();
										~Buffer();
			void						InvalidateDeviceObjects();
			void						EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const crossplatform::Layout *layout,const void *data,bool cpu_access=false,bool streamout_target=false) override;
			void						EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data, bool cpu_access = false) override;
			void						*Map(crossplatform::DeviceContext &deviceContext) override;
			void						Unmap(crossplatform::DeviceContext &deviceContext) override;
			ID3D12Resource * const AsD3D12Buffer()  const override
			{
				return d3d12Buffer;
			}
			ID3D12Resource *  AsD3D12Buffer() override 
			{
				return d3d12Buffer;
			}
			D3D12_VERTEX_BUFFER_VIEW*	GetVertexBufferView() override;
			D3D12_INDEX_BUFFER_VIEW*	GetIndexBufferView() override;
		private:
			ID3D12Resource*				d3d12Buffer;
			ID3D12Resource*				mIntermediateHeap;
			UINT32						mBufferSize;
			UINT8*						mGpuMappedPtr;
			D3D12_VERTEX_BUFFER_VIEW	mVertexBufferView;
			D3D12_INDEX_BUFFER_VIEW		mIndexBufferView;
			unsigned char *				mappedData=nullptr;
		};
	}
};

