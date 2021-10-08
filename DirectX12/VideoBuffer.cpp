#include "SimulDirectXHeader.h"
#include "VideoBuffer.h"
#include "RenderPlatform.h"

using namespace simul;
using namespace dx12;

VideoBuffer::VideoBuffer()
	: mGpuHeap(nullptr)
	, mIntermediateHeap(nullptr)
	, mBufferSize(0)
{

}

VideoBuffer::~VideoBuffer()
{
	InvalidateDeviceObjects();
}

void VideoBuffer::InvalidateDeviceObjects()
{
	auto rPlat = (dx12::RenderPlatform*)renderPlatform;
	rPlat->PushToReleaseManager(mGpuHeap, "Buffer");
	rPlat->PushToReleaseManager(mIntermediateHeap, "Buffer");
	mIntermediateHeap = nullptr;
	mGpuHeap = nullptr;
}

void VideoBuffer::EnsureBuffer(crossplatform::RenderPlatform* r, crossplatform::VideoBufferType bufferType, const void* data, uint32_t dataSize)
{
	HRESULT res = S_FALSE;
	mBufferSize = 0;
	SAFE_DELETE(mGpuHeap);
	SAFE_DELETE(mIntermediateHeap);
	mHasData = false;
	renderPlatform = r;
	mBufferType = bufferType;

	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		SIMUL_PPV_ARGS(&mGpuHeap)
	);
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mGpuHeap, mBufferSize)
	mGpuHeap->SetName(L"VideoBufferUpload");

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
		mIntermediateHeap->SetName(L"IntermediateVideoBuffer");

	if (data)
	{
		Update(data, dataSize);
	}
}

void* VideoBuffer::Map(crossplatform::DeviceContext& deviceContext, uint32_t newBufferSize)
{
	mBufferSize = newBufferSize;
	const CD3DX12_RANGE range(0, 0);
	mGpuMappedPtr = new UINT8[mBufferSize];
	return (void*)mGpuMappedPtr;
}

void VideoBuffer::Unmap(crossplatform::DeviceContext& deviceContext)
{
	if(mGpuMappedPtr)
	{
		Update(mGpuMappedPtr, mBufferSize);
		delete[] mGpuMappedPtr;
		mGpuMappedPtr = nullptr;
	}
}

void VideoBuffer::Update(const void* data, uint32_t dataSize)
{
	D3D12_RESOURCE_STATES stateBefore = D3D12_RESOURCE_STATE_COMMON;
	#if !defined(_DURANGO)
	switch (mBufferType)
	{
	case crossplatform::VideoBufferType::DECODE_READ:
		stateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VIDEO_DECODE_READ;
		break;
	case crossplatform::VideoBufferType::ENCODE_READ:
		stateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ;
		break;
	case crossplatform::VideoBufferType::PROCESS_READ:
		stateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ;
		break;
	}
	#endif

	if (mHasData)
	{
		D3D12_RESOURCE_BARRIER barrier1;
		barrier1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier1.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier1.Transition.pResource = mGpuHeap;
		barrier1.Transition.Subresource = 0;
		barrier1.Transition.StateBefore = stateBefore;
		barrier1.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		renderPlatform->AsD3D12CommandList()->ResourceBarrier(1, &barrier1);
	}
	else
	{
		mHasData = true;
	}

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = data;
	subresourceData.RowPitch = dataSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	UpdateSubresources(renderPlatform->AsD3D12CommandList(), mGpuHeap, mIntermediateHeap, 0, 0, 1, &subresourceData);

	D3D12_RESOURCE_BARRIER barrier2;
	barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier2.Transition.pResource = mGpuHeap;
	barrier2.Transition.Subresource = 0;
	barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier2.Transition.StateAfter = stateBefore;
	renderPlatform->AsD3D12CommandList()->ResourceBarrier(1, &barrier2);

	mBufferSize = dataSize;
}
