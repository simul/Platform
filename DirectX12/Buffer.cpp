#include "SimulDirectXHeader.h"
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

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* r, int num_vertices, const crossplatform::Layout* layout, const void* data, bool cpu_access, bool streamout_target)
{
	HRESULT res = S_FALSE;
	renderPlatform = r;
	stride = layout->GetStructSize();
	mBufferSize = num_vertices * layout->GetStructSize();
	count = num_vertices;
	SAFE_DELETE(mGpuHeap);
	SAFE_DELETE(mIntermediateHeap);
	// Just debug memory usage
	//float megas = (float)mBufferSize / 1048576.0f;
	//SIMUL_COUT << "Allocating: " << std::to_string(mBufferSize) << ".bytes in the GPU, (" << std::to_string(megas) << ".MB)\n";
	// Upload heap to hold the vertex data in the GPU (we will be mapping it to copy new data)

	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		data ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
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

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = mBufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(renderPlatform->AsD3D12CommandList(), mGpuHeap, mIntermediateHeap, 0, 0, 1, &subresourceData);

		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = mGpuHeap;
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		renderPlatform->AsD3D12CommandList()->ResourceBarrier(1, &barrier);
	}

	// Make a vertex buffer view
	mVertexBufferView.SizeInBytes = mBufferSize;
	mVertexBufferView.StrideInBytes = stride;
	mVertexBufferView.BufferLocation = mGpuHeap->GetGPUVirtualAddress();
	bufferType = crossplatform::BufferType::VERTEX;
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* r, int num_indices, int index_size_bytes, const void* data)
{
	HRESULT res = S_FALSE;
	renderPlatform = r;
	stride = index_size_bytes;
	mBufferSize = index_size_bytes * num_indices;
	count = num_indices;
	SAFE_DELETE(mGpuHeap);
	SAFE_DELETE(mIntermediateHeap);

	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		data ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_INDEX_BUFFER,
		nullptr,
		SIMUL_PPV_ARGS(&mGpuHeap)
	);
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mGpuHeap, mBufferSize)
		mGpuHeap->SetName(L"IndexUpload");

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

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = mBufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(renderPlatform->AsD3D12CommandList(), mGpuHeap, mIntermediateHeap, 0, 0, 1, &subresourceData);

		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = mGpuHeap;
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;

		renderPlatform->AsD3D12CommandList()->ResourceBarrier(1, &barrier);
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

	mIndexBufferView.Format = indexFormat;
	mIndexBufferView.SizeInBytes = mBufferSize;
	mIndexBufferView.BufferLocation = mGpuHeap->GetGPUVirtualAddress();
	bufferType = crossplatform::BufferType::INDEX;
}

void *Buffer::Map(crossplatform::DeviceContext &)
{
	const CD3DX12_RANGE range(0, 0);
	mGpuMappedPtr = new UINT8[mBufferSize];
/*	HRESULT hr=mGpuHeap->Map(0, nullptr, reinterpret_cast<void**>(&mGpuMappedPtr));
	if (hr != S_OK)
		return nullptr;*/
	return (void*)mGpuMappedPtr;
}

void Buffer::Unmap(crossplatform::DeviceContext &)
{
	//const CD3DX12_RANGE range(0, 0);
	//mGpuHeap->Unmap(0, nullptr);// &range);
	if(mGpuMappedPtr)
	{
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = mGpuMappedPtr;
		subresourceData.RowPitch = mBufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		D3D12_RESOURCE_BARRIER barrier1;
		barrier1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier1.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier1.Transition.pResource = mGpuHeap;
		barrier1.Transition.Subresource = 0;
		barrier1.Transition.StateBefore = bufferType==crossplatform::BufferType::INDEX?D3D12_RESOURCE_STATE_INDEX_BUFFER:D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		barrier1.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		renderPlatform->AsD3D12CommandList()->ResourceBarrier(1, &barrier1);
		UpdateSubresources(renderPlatform->AsD3D12CommandList(), mGpuHeap, mIntermediateHeap, 0, 0, 1, &subresourceData);
		D3D12_RESOURCE_BARRIER barrier2;
		barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier2.Transition.pResource = mGpuHeap;
		barrier2.Transition.Subresource = 0;
		barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier2.Transition.StateAfter = barrier1.Transition.StateBefore;

		renderPlatform->AsD3D12CommandList()->ResourceBarrier(1, &barrier2);

		delete[] mGpuMappedPtr;
		mGpuMappedPtr = nullptr;
	}
}

D3D12_VERTEX_BUFFER_VIEW* Buffer::GetVertexBufferView()
{
	return &mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW* Buffer::GetIndexBufferView()
{
	return &mIndexBufferView;
}

