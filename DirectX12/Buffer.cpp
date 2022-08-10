#include "SimulDirectXHeader.h"
#include "Buffer.h"
#include "RenderPlatform.h"
#include <iomanip>

using namespace platform;
using namespace dx12;

Buffer::Buffer():
	d3d12Buffer(nullptr)
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
	rPlat->PushToReleaseManager(d3d12Buffer, "Buffer");
	rPlat->PushToReleaseManager(mIntermediateHeap, "Buffer");
	mIntermediateHeap=nullptr;
	d3d12Buffer=nullptr;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* r, int num_vertices, const crossplatform::Layout* layout, const void* data, bool cpu_access, bool streamout_target)
{
	HRESULT res = S_FALSE;
	renderPlatform = r;
	stride = layout->GetStructSize();
	mBufferSize = num_vertices * layout->GetStructSize();
	count = num_vertices;
	SAFE_DELETE(d3d12Buffer);
	SAFE_DELETE(mIntermediateHeap);

	// NOTE: Buffers must start in the COMMON resource state. Not all drivers enforce this.
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		SIMUL_PPV_ARGS(&d3d12Buffer)
	);
	auto& deviceContext = renderPlatform->GetImmediateContext();
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(d3d12Buffer, mBufferSize)
	SetD3DName(d3d12Buffer,(name+" VertexUpload").c_str());
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

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = d3d12Buffer;
	barrier.Transition.Subresource = 0;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	if (data)
	{

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = mBufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		deviceContext.asD3D12Context()->ResourceBarrier(1, &barrier);

		UpdateSubresources(deviceContext.asD3D12Context(), d3d12Buffer, mIntermediateHeap, 0, 0, 1, &subresourceData);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	deviceContext.asD3D12Context()->ResourceBarrier(1, &barrier);
#if PLATFORM_DEBUG_BARRIERS
	LOG_BARRIER_INFO(name.c_str(), d3d12Buffer, barrier.Transition.StateBefore, barrier.Transition.StateAfter);
#endif

	// Make a vertex buffer view
	mVertexBufferView.SizeInBytes = mBufferSize;
	mVertexBufferView.StrideInBytes = stride;
	mVertexBufferView.BufferLocation = d3d12Buffer->GetGPUVirtualAddress();
	bufferType = crossplatform::BufferType::VERTEX;
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* r, int num_indices, int index_size_bytes, const void* data,bool cpu_access)
{
	HRESULT res = S_FALSE;
	renderPlatform = r;
	stride = index_size_bytes;
	mBufferSize = index_size_bytes * num_indices;
	count = num_indices;
	SAFE_DELETE(d3d12Buffer);
	SAFE_DELETE(mIntermediateHeap);
	auto& deviceContext = renderPlatform->GetImmediateContext();
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		SIMUL_PPV_ARGS(&d3d12Buffer)
	);
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(d3d12Buffer, mBufferSize)
	d3d12Buffer->SetName(L"IndexUpload");
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

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = d3d12Buffer;
	barrier.Transition.Subresource = 0;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	if (data)
	{
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = mBufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		deviceContext.asD3D12Context()->ResourceBarrier(1, &barrier);
#if PLATFORM_DEBUG_BARRIERS
		LOG_BARRIER_INFO(name.c_str(), d3d12Buffer, barrier.Transition.StateBefore, barrier.Transition.StateAfter);
#endif
		UpdateSubresources(deviceContext.asD3D12Context(), d3d12Buffer, mIntermediateHeap, 0, 0, 1, &subresourceData);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;

	deviceContext.asD3D12Context()->ResourceBarrier(1, &barrier);
#if PLATFORM_DEBUG_BARRIERS
	LOG_BARRIER_INFO(name.c_str(), d3d12Buffer, barrier.Transition.StateBefore, barrier.Transition.StateAfter);
#endif

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
	mIndexBufferView.BufferLocation = d3d12Buffer->GetGPUVirtualAddress();
	bufferType = crossplatform::BufferType::INDEX;
}

void *Buffer::Map(crossplatform::DeviceContext &)
{
	const CD3DX12_RANGE range(0, 0);
	mGpuMappedPtr = new UINT8[mBufferSize];
	return (void*)mGpuMappedPtr;
}

void Buffer::Unmap(crossplatform::DeviceContext &deviceContext)
{
	if(mGpuMappedPtr)
	{
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = mGpuMappedPtr;
		subresourceData.RowPitch = mBufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		D3D12_RESOURCE_BARRIER barrier1;
		barrier1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier1.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier1.Transition.pResource = d3d12Buffer;
		barrier1.Transition.Subresource = 0;
		barrier1.Transition.StateBefore = bufferType==crossplatform::BufferType::INDEX?D3D12_RESOURCE_STATE_INDEX_BUFFER:D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		barrier1.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		deviceContext.asD3D12Context()->ResourceBarrier(1, &barrier1);
#if PLATFORM_DEBUG_BARRIERS
		LOG_BARRIER_INFO(name.c_str(), d3d12Buffer, barrier1.Transition.StateBefore, barrier1.Transition.StateAfter);
#endif
		UpdateSubresources(deviceContext.asD3D12Context(), d3d12Buffer, mIntermediateHeap, 0, 0, 1, &subresourceData);
		D3D12_RESOURCE_BARRIER barrier2;
		barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier2.Transition.pResource = d3d12Buffer;
		barrier2.Transition.Subresource = 0;
		barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier2.Transition.StateAfter = barrier1.Transition.StateBefore;

		deviceContext.asD3D12Context()->ResourceBarrier(1, &barrier2);
#if PLATFORM_DEBUG_BARRIERS
		LOG_BARRIER_INFO(name.c_str(), d3d12Buffer, barrier2.Transition.StateBefore, barrier2.Transition.StateAfter);
#endif
		}
		delete[] mGpuMappedPtr;
		mGpuMappedPtr = nullptr;
	}

D3D12_VERTEX_BUFFER_VIEW* Buffer::GetVertexBufferView()
{
	return &mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW* Buffer::GetIndexBufferView()
{
	return &mIndexBufferView;
}

