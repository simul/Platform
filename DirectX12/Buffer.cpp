#include "SimulDirectXHeader.h"
#include "Buffer.h"
#include "RenderPlatform.h"
#include <iomanip>

//using std::string_literals::operator""s;
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
	rPlat->PushToReleaseManager(d3d12Buffer, &mAllocationInfo);
	rPlat->PushToReleaseManager(mIntermediateHeap, &mIntermediateAllocationInfo);
	mIntermediateHeap=nullptr;
	d3d12Buffer=nullptr;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* r, int num_vertices, int str, std::shared_ptr<std::vector<uint8_t>> data, bool cpu_access, bool streamout_target)
{
	HRESULT res = S_FALSE;
	renderPlatform = r;
	stride = str;
	mBufferSize = num_vertices * stride;
	count = num_vertices;
	InvalidateDeviceObjects();

	// NOTE: Buffers must start in the COMMON resource state. Not all drivers enforce this.
	auto defaultDesc=CD3DX12_RESOURCE_DESC::Buffer(mBufferSize);
	std::string n = (name + " Vertex");
	((dx12::RenderPlatform*)renderPlatform)->CreateResource(
		defaultDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		D3D12_HEAP_TYPE_DEFAULT,
		&d3d12Buffer,
		mAllocationInfo,
		n.c_str());
	SIMUL_GPU_TRACK_MEMORY_NAMED(d3d12Buffer, mBufferSize, n.c_str())

	std::string n2 = (name + " Intermediate Vertex");
	((dx12::RenderPlatform*)renderPlatform)->CreateResource(
		defaultDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		D3D12_HEAP_TYPE_UPLOAD,
		&mIntermediateHeap,
		mIntermediateAllocationInfo,
		n.c_str());
	SIMUL_GPU_TRACK_MEMORY_NAMED(mIntermediateHeap, mBufferSize, n2.c_str())

	upload_data=data;

	// Make a vertex buffer view
	mVertexBufferView.SizeInBytes = mBufferSize;
	mVertexBufferView.StrideInBytes = stride;
	mVertexBufferView.BufferLocation = d3d12Buffer->GetGPUVirtualAddress();
	bufferType = crossplatform::BufferType::VERTEX;
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* r, int num_indices, int index_size_bytes, std::shared_ptr<std::vector<uint8_t>> data,bool cpu_access)
{
	HRESULT res = S_FALSE;
	renderPlatform = r;
	stride = index_size_bytes;
	mBufferSize = index_size_bytes * num_indices;
	count = num_indices;
	upload_data=data;
	InvalidateDeviceObjects();

	auto defaultDesc=CD3DX12_RESOURCE_DESC::Buffer(mBufferSize);
	std::string n = (name + " Index");
	((dx12::RenderPlatform*)renderPlatform)->CreateResource(
		defaultDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		D3D12_HEAP_TYPE_DEFAULT,
		&d3d12Buffer,
		mAllocationInfo,
		n.c_str());
	SIMUL_GPU_TRACK_MEMORY_NAMED(d3d12Buffer, mBufferSize, n.c_str())

	std::string n2 = (name + " Intermediate Index");
	((dx12::RenderPlatform*)renderPlatform)->CreateResource(
		defaultDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		D3D12_HEAP_TYPE_UPLOAD,
		&mIntermediateHeap,
		mIntermediateAllocationInfo,
		n.c_str());
	SIMUL_GPU_TRACK_MEMORY_NAMED(mIntermediateHeap, mBufferSize, n2.c_str())

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
	if(!mBufferSize)
		return 0;
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

void Buffer::Upload(crossplatform::DeviceContext &deviceContext)
{
	if (upload_data && upload_data->size())
	{
		//auto& deviceContext = renderPlatform->GetImmediateContext();
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = upload_data->data();
		subresourceData.RowPitch = mBufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;
		
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = d3d12Buffer;
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		LOG_BARRIER_INFO(name.c_str(), d3d12Buffer, barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		deviceContext.asD3D12Context()->ResourceBarrier(1, &barrier);

		UpdateSubresources(deviceContext.asD3D12Context(), d3d12Buffer, mIntermediateHeap, 0, 0, 1, &subresourceData);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = bufferType == crossplatform::BufferType::INDEX ? D3D12_RESOURCE_STATE_INDEX_BUFFER : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		deviceContext.asD3D12Context()->ResourceBarrier(1, &barrier);
		LOG_BARRIER_INFO(name.c_str(), d3d12Buffer, barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		upload_data=nullptr;
	}
}