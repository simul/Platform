#include "SimulDirectXHeader.h"
#include "Buffer.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"

using namespace simul;
using namespace dx12;

Buffer::Buffer():
	mUploadHeap(nullptr)
{

}

Buffer::~Buffer()
{
	InvalidateDeviceObjects();
}

void Buffer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(mUploadHeap);
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const crossplatform::Layout *layout,const void *data,bool cpu_access,bool streamout_target)
{
	HRESULT res = S_FALSE;

	stride = layout->GetStructSize();
	mBufferSize = num_vertices * layout->GetStructSize();

	// Just debug memory usage
	//float megas = (float)mBufferSize / 1048576.0f;
	//SIMUL_COUT << "Allocating: " << std::to_string(mBufferSize) << ".bytes in the GPU, (" << std::to_string(megas) << ".MB)\n";

	// Upload heap to hold the vertex data in the GPU (we will be mapping it to copy new data)
	CD3DX12_RESOURCE_DESC b=CD3DX12_RESOURCE_DESC::Buffer(mBufferSize);
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		SIMUL_PPV_ARGS(&mUploadHeap)
	);
	SIMUL_ASSERT(res == S_OK);
	mUploadHeap->SetName(L"VertexUpload");

	// LOL! \O_O/
	if (data)
	{
		crossplatform::DeviceContext tmpCrap;
		Map(tmpCrap);
		memcpy(mGpuMappedPtr, data, mBufferSize);
		Unmap(tmpCrap);
	}
	
	// Make a vertex buffer view
	mVertexBufferView.SizeInBytes		= mBufferSize;
	mVertexBufferView.StrideInBytes		= stride;
	mVertexBufferView.BufferLocation	= mUploadHeap->GetGPUVirtualAddress();
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data)
{
	HRESULT res = S_FALSE;
	mBufferSize = index_size_bytes * num_indices;

	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
        SIMUL_PPV_ARGS(&mUploadHeap)
	);
	SIMUL_ASSERT(res == S_OK);
	mUploadHeap->SetName(L"IndexUpload");

	// LOL! \O_O/
	if (data)
	{
		crossplatform::DeviceContext tmpCrap;
		Map(tmpCrap);
		memcpy(mGpuMappedPtr, data, mBufferSize);
		Unmap(tmpCrap);
	}

	DXGI_FORMAT indexFormat;
	if (index_size_bytes == 4)
	{
		indexFormat = DXGI_FORMAT_R32_UINT;
	}
	else if (index_size_bytes == 2)
	{
		indexFormat = DXGI_FORMAT_R16_UINT;
	}
	else
	{
		indexFormat = DXGI_FORMAT_UNKNOWN;
		SIMUL_BREAK("Improve this!");
	}

	mIndexBufferView.Format			= indexFormat;
	mIndexBufferView.SizeInBytes	= mBufferSize;
	mIndexBufferView.BufferLocation = mUploadHeap->GetGPUVirtualAddress();
}

void *Buffer::Map(crossplatform::DeviceContext &)
{
	const CD3DX12_RANGE range(0, 0);
	mUploadHeap->Map(0, &range, reinterpret_cast<void**>(&mGpuMappedPtr));
	return (void*)mGpuMappedPtr;
}

void Buffer::Unmap(crossplatform::DeviceContext &)
{
	const CD3DX12_RANGE range(0, 0);
	mUploadHeap->Unmap(0, &range);
}

D3D12_VERTEX_BUFFER_VIEW* Buffer::GetVertexBufferView()
{
	return &mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW* Buffer::GetIndexBufferView()
{
	return &mIndexBufferView;
}

