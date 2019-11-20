#pragma once
#include "SimulDirectXHeader.h"
#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"


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
			void						EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const crossplatform::Layout *layout,const void *data,bool cpu_access=false,bool streamout_target=false);
			void						EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data);
			void						*Map(crossplatform::DeviceContext &deviceContext) override;
			void						Unmap(crossplatform::DeviceContext &deviceContext) override;
			D3D12_VERTEX_BUFFER_VIEW*	GetVertexBufferView() override;
			D3D12_INDEX_BUFFER_VIEW*	GetIndexBufferView() override;

		private:
			ID3D12Resource*				mGpuHeap;
			UINT32						mBufferSize;
			UINT8*						mGpuMappedPtr;
			D3D12_VERTEX_BUFFER_VIEW	mVertexBufferView;
			D3D12_INDEX_BUFFER_VIEW		mIndexBufferView;
		};
	}
};

