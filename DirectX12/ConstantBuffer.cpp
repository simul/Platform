#include "Platform/DirectX12/ConstantBuffer.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "SimulDirectXHeader.h"
#include <string>

//using std::string_literals::operator""s;

using namespace platform;
using namespace dx12;

template< class T > UINT64 BytePtrToUint64( _In_ T* ptr )
{
	return static_cast< UINT64 >( reinterpret_cast< BYTE* >( ptr ) - static_cast< BYTE* >( nullptr ) );
}

PlatformConstantBuffer::PlatformConstantBuffer(crossplatform::ResourceUsageFrequency F) :
	crossplatform::PlatformConstantBuffer(F),
	mSlots(0),
	mMaxDescriptors(0),
	mCurApplyCount(0)
{
	for (unsigned int i = 0; i < kNumBuffers; i++)
	{
		mUploadHeap[i]          = nullptr;
		cpuDescriptorHandles[i] = nullptr;
	}
}

PlatformConstantBuffer::~PlatformConstantBuffer()
{
	InvalidateDeviceObjects();
}

void PlatformConstantBuffer::CreateBuffers(crossplatform::RenderPlatform* r, void* addr) 
{
	renderPlatform = r;

	// Create the heaps (storage for our descriptors)
	// mBufferSize / 256 (this is the max number of descriptors we can hold with this upload heaps
	mMaxDescriptors = mBufferSize / (kBufferAlign * mSlots);
	for (unsigned int i = 0; i < 3; i++)
	{
		mHeaps[i].Release();
		mHeaps[i].Restore((dx12::RenderPlatform*)renderPlatform, mMaxDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, platform::core::QuickFormat("PlatformConstantBuffer %016x CBHeap %d",this,i), false);
		delete [] cpuDescriptorHandles[i];
		cpuDescriptorHandles[i]=new D3D12_CPU_DESCRIPTOR_HANDLE[mMaxDescriptors];
	}
	// Create the upload heap (the gpu memory that will hold the constant buffer)
	// Each upload heap has 64KB.  (64KB aligned)
	for (unsigned int i = 0; i < 3; i++)
	{
		auto uploadDesc=CD3DX12_RESOURCE_DESC::Buffer(mBufferSize);
		((dx12::RenderPlatform*)renderPlatform)->CreateResource(
			uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			D3D12_HEAP_TYPE_UPLOAD,
			&mUploadHeap[i],
			mUploadAllocationInfos[i],
			name.c_str()
		);
		SIMUL_GPU_TRACK_MEMORY(mUploadHeap[i], mBufferSize)
		
		// If it is an UPLOAD buffer, will be the memory allocated in VRAM too?
		// as far as we know, an UPLOAD buffer has CPU read/write access and GPU read access.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.SizeInBytes						= kBufferAlign * mSlots;
		for(UINT j=0;j<mMaxDescriptors;j++)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE &handle		= cpuDescriptorHandles[i][j];
			UINT64 offset							= (kBufferAlign * mSlots) * j;	
			handle									= mHeaps[i].GetCpuHandleFromStartAfOffset(j);
			cbvDesc.BufferLocation					= mUploadHeap[i]->GetGPUVirtualAddress() + offset;
			renderPlatform->AsD3D12Device()->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}

void PlatformConstantBuffer::SetNumBuffers(crossplatform::RenderPlatform *r, UINT nContexts,  UINT nMapsPerFrame, UINT nFramesBuffering )
{
	CreateBuffers( r, nullptr);
}


D3D12_CPU_DESCRIPTOR_HANDLE PlatformConstantBuffer::AsD3D12ConstantBuffer()
{
	// This method is a bit hacky, it basically returns the CPU handle of the last
	// "current" descriptor we will increase the curApply after the apply that's why 
	// we substract 1 here
	if (mCurApplyCount == 0)
	{
		SIMUL_BREAK("We must apply this cb first");
		mCurApplyCount=1;
	}
	return cpuDescriptorHandles[buffer_index][mCurApplyCount-1];
}

void PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t size,void *addr)
{
	renderPlatform = r;
	
	// Calculate the number of slots this constant buffer will use:
	// mSlots = 256 Aligned size / 256
	mSlots = ((size + (kBufferAlign - 1)) & ~ (kBufferAlign - 1)) / kBufferAlign;
	InvalidateDeviceObjects();
	if(!r)
		return;
	SetNumBuffers( r,1, 64, 2 );
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect, const char *n, int bindingIndex)
{
	name=n;
	for (unsigned int i = 0; i < 3; i++)
	{
		mHeaps[i].GetHeap()->SetName(std::wstring(name.begin(), name.end()).c_str());
	}
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
	for (unsigned int i = 0; i < kNumBuffers; i++)
	{
		mHeaps[i].Release();
		renderPlatformDx12->PushToReleaseManager(mUploadHeap[i], &mUploadAllocationInfos[i]);
		mUploadHeap[i]=nullptr;
		delete [] cpuDescriptorHandles[i];
		cpuDescriptorHandles[i]=nullptr;
	}
	renderPlatform=nullptr;
	src = nullptr;
	size = 0;
}

void PlatformConstantBuffer::Apply(platform::crossplatform::DeviceContext &deviceContext, size_t sz, void *addr)
{
	src=addr;
	size=sz;
}

void  PlatformConstantBuffer::ActualApply(crossplatform::DeviceContext & deviceContext )
{
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	// If new frame, update current frame index and reset the apply count
	if (last_frame_number != renderPlatform->GetFrameNumber())
	{
		last_frame_number = renderPlatform->GetFrameNumber();
		buffer_index++;
		if (buffer_index >= kNumBuffers)
			buffer_index = 0;
		mCurApplyCount = 0;
	}
	if (mCurApplyCount >= mMaxDescriptors)
	{
		// This should really be solved by having like some kind of pool? Or allocating more space, something like that
		if(mCurApplyCount == mMaxDescriptors)
		{
			SIMUL_BREAK_ONCE("This ConstantBuffer reached its maximum apply count");
			mBufferSize*=2;
			CreateBuffers(renderPlatform,nullptr);
		}
		return;
	}

	// pDest points at the begining of the uploadHeap, we can offset it! (we created 64KB and each Constant buffer
	// has a minimum size of kBufferAlign)
	UINT8* pDest = nullptr;
	UINT64 offset = (kBufferAlign * mSlots) * mCurApplyCount;
	const CD3DX12_RANGE mapRange(0, 0);
	HRESULT hResult = mUploadHeap[buffer_index]->Map(0, &mapRange, reinterpret_cast<void**>(&pDest));
	if (hResult == S_OK)
	{
		memcpy(pDest + offset, src, size);
		const CD3DX12_RANGE unMapRange(offset, offset + size);
		mUploadHeap[buffer_index]->Unmap(0, &unMapRange);
	}
	else
	{
		rPlat->PrintDeviceRemovedExtendedData();
		SIMUL_BREAK_ONCE("hResult != S_OK");
	}
	mCurApplyCount++;
}

void PlatformConstantBuffer::Unbind(platform::crossplatform::DeviceContext& )
{
}
