#include "DirectXHeader.h"
#include "Buffer.h"
#include "RenderPlatform.h"

using namespace simul;
using namespace dx12;

Buffer::Buffer():
	mGpuHeap(nullptr)
	,mIntermediateHeap(nullptr)
{

}

Buffer::~Buffer()
{
	InvalidateDeviceObjects();
}

void Buffer::InvalidateDeviceObjects()
{
	auto rPlat = (dx12::RenderPlatform*)renderPlatform;
	rPlat->PushToReleaseManager(mGpuHeap, "Buffer");
	rPlat->PushToReleaseManager(mIntermediateHeap, "Buffer");
	mIntermediateHeap=nullptr;
	mGpuHeap=nullptr;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform *r,int num_vertices,const crossplatform::Layout *layout,const void *data,bool cpu_access,bool streamout_target)
{
	HRESULT res = S_FALSE;
	renderPlatform = r;
	stride = layout->GetStructSize();
	mBufferSize = num_vertices * layout->GetStructSize();

	// Just debug memory usage
	//float megas = (float)mBufferSize / 1048576.0f;
	//SIMUL_COUT << "Allocating: " << std::to_string(mBufferSize) << ".bytes in the GPU, (" << std::to_string(megas) << ".MB)\n";

	// Upload heap to hold the vertex data in the GPU (we will be mapping it to copy new data)
	CD3DX12_RESOURCE_DESC b=CD3DX12_RESOURCE_DESC::Buffer(mBufferSize);
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		data?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr,
		SIMUL_PPV_ARGS(&mGpuHeap)
	);
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mGpuHeap, mBufferSize)
	mGpuHeap->SetName(L"VertexUpload");

	if (data)
	{
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			SIMUL_PPV_ARGS(&mIntermediateHeap)
		);
		SIMUL_ASSERT(res == S_OK);
		SIMUL_GPU_TRACK_MEMORY(mIntermediateHeap, mBufferSize)
		mIntermediateHeap->SetName(L"IntermediateVertexBuffer");
	}

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = data;
	subresourceData.RowPitch = mBufferSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	UpdateSubresources(renderPlatform->AsD3D12CommandList(), mGpuHeap, mIntermediateHeap, 0, 0, 1, &subresourceData);

	
	// Make a vertex buffer view
	mVertexBufferView.SizeInBytes		= mBufferSize;
	mVertexBufferView.StrideInBytes		= stride;
	mVertexBufferView.BufferLocation	= mGpuHeap->GetGPUVirtualAddress();
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform *r,int num_indices,int index_size_bytes,const void *data)
{
	HRESULT res = S_FALSE;
	renderPlatform = r;
	mBufferSize = index_size_bytes * num_indices;

	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		data?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_INDEX_BUFFER,
		nullptr,
        SIMUL_PPV_ARGS(&mGpuHeap)
	);
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mGpuHeap, mBufferSize)
	mGpuHeap->SetName(L"IndexUpload");

	ID3D12Resource* mIntermediateHeap = nullptr;

	if (data)
	{
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			SIMUL_PPV_ARGS(&mIntermediateHeap)
		);
		SIMUL_ASSERT(res == S_OK);
		SIMUL_GPU_TRACK_MEMORY(mIntermediateHeap, mBufferSize)
		mIntermediateHeap->SetName(L"IntermediateIndexBuffer");
		/*
		crossplatform::DeviceContext tmpCrap;
		Map(tmpCrap);
		memcpy(mGpuMappedPtr, data, mBufferSize);
		Unmap(tmpCrap);
		*/
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

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = data;
	subresourceData.RowPitch = mBufferSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	UpdateSubresources(renderPlatform->AsD3D12CommandList(), mGpuHeap, mIntermediateHeap,
		0, 0, 1, &subresourceData);

	mIndexBufferView.Format			= indexFormat;
	mIndexBufferView.SizeInBytes	= mBufferSize;
	mIndexBufferView.BufferLocation = mGpuHeap->GetGPUVirtualAddress();
}

void *Buffer::Map(crossplatform::DeviceContext &)
{
	const CD3DX12_RANGE range(0, 0);
	mGpuMappedPtr = nullptr;
	HRESULT hr=mGpuHeap->Map(0, nullptr, reinterpret_cast<void**>(&mGpuMappedPtr));
	if (hr != S_OK)
		return nullptr;
	return (void*)mGpuMappedPtr;
}

void Buffer::Unmap(crossplatform::DeviceContext &)
{
	const CD3DX12_RANGE range(0, 0);
	mGpuHeap->Unmap(0, nullptr);// &range);
}

D3D12_VERTEX_BUFFER_VIEW* Buffer::GetVertexBufferView()
{
	return &mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW* Buffer::GetIndexBufferView()
{
	return &mIndexBufferView;
}

