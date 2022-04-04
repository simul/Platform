#include "VideoBuffer.h"
#include "SimulDirectXHeader.h"
#include "RenderPlatform.h"
#include <d3d12video.h>

using namespace simul;
using namespace dx12;

VideoBuffer::VideoBuffer()
	: mGpuHeap(nullptr)
	, mIntermediateHeap(nullptr)
	, mBufferSize(0)
	, mGpuMappedPtr(nullptr)
{

}

VideoBuffer::~VideoBuffer()
{
	InvalidateDeviceObjects();
}

void VideoBuffer::InvalidateDeviceObjects()
{
	auto rPlat = (dx12::RenderPlatform*)renderPlatform;
	rPlat->PushToReleaseManager(mGpuHeap, "VideoBuffer");
	rPlat->PushToReleaseManager(mIntermediateHeap, "VideoBuffer");
	mIntermediateHeap = nullptr;
	mGpuHeap = nullptr;
	SAFE_DELETE_ARRAY(mGpuMappedPtr);
}

void VideoBuffer::EnsureBuffer(crossplatform::RenderPlatform* r, crossplatform::VideoBufferType bufferType, uint32_t dataSize)
{
	HRESULT res = S_FALSE;
	mBufferSize = dataSize;
	SAFE_RELEASE(mGpuHeap);
	SAFE_RELEASE(mIntermediateHeap);
	SAFE_DELETE_ARRAY(mGpuMappedPtr);
	mHasData = false;
	renderPlatform = r;
	mBufferType = bufferType;

	mState = D3D12_RESOURCE_STATE_COMMON;

	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
		mState,
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

	mGpuMappedPtr = new UINT8[mBufferSize];
}

void VideoBuffer::ChangeState(void* videoContext, crossplatform::VideoBufferState bufferstate)
{
	static const D3D12_RESOURCE_STATES uploadState = D3D12_RESOURCE_STATE_COPY_DEST;

	D3D12_RESOURCE_STATES videoState;
	switch (mBufferType)
	{
	case crossplatform::VideoBufferType::DECODE_READ:
		videoState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VIDEO_DECODE_READ;
		break;
	case crossplatform::VideoBufferType::ENCODE_READ:
		videoState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ;
		break;
	case crossplatform::VideoBufferType::PROCESS_READ:
		videoState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ;
		break;
	default:
		return;
	}

	D3D12_RESOURCE_STATES stateBefore = mState;
	D3D12_RESOURCE_STATES stateAfter;
	
	switch (bufferstate)
	{
	case crossplatform::VideoBufferState::COMMON:
		stateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
		break;
	case crossplatform::VideoBufferState::UPLOAD:
		stateAfter = uploadState;
		break;
	case crossplatform::VideoBufferState::OPERATION:
		stateAfter = videoState;
		break;
	default:
		return;
	}

	if (stateBefore == stateAfter)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = mGpuHeap;
	barrier.Transition.Subresource = 0;
	barrier.Transition.StateBefore = stateBefore;
	barrier.Transition.StateAfter = stateAfter;
	
	if (stateAfter == uploadState)
	{
		((ID3D12GraphicsCommandList*)videoContext)->ResourceBarrier(1, &barrier);
	}
	else
	{
		switch (mBufferType)
		{
		case crossplatform::VideoBufferType::DECODE_READ:
			((ID3D12VideoDecodeCommandList*)videoContext)->ResourceBarrier(1, &barrier);
			break;
		case crossplatform::VideoBufferType::ENCODE_READ:
			((ID3D12VideoEncodeCommandList*)videoContext)->ResourceBarrier(1, &barrier);
			break;
		case crossplatform::VideoBufferType::PROCESS_READ:
			((ID3D12VideoProcessCommandList*)videoContext)->ResourceBarrier(1, &barrier);
			break;
		}
	}
	
	mState = stateAfter;
}

void VideoBuffer::Update(void* graphicsContext, const void* data, uint32_t dataSize)
{
	SIMUL_ASSERT(dataSize <= mBufferSize);

	memcpy(mGpuMappedPtr, data, dataSize);

	ID3D12GraphicsCommandList* graphicsCommandList = (ID3D12GraphicsCommandList*)graphicsContext;
	
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = mGpuMappedPtr;
	subresourceData.RowPitch = mBufferSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	UINT64 result = UpdateSubresources(graphicsCommandList, mGpuHeap, mIntermediateHeap, 0, 0, 1, &subresourceData);

	SIMUL_ASSERT(result != 0);
}
