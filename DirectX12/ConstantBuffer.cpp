#include "Platform/DirectX12/ConstantBuffer.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "SimulDirectXHeader.h"
#include <string>

using namespace simul;
using namespace dx12;

inline bool IsPowerOfTwo( UINT64 n )
{
	return ( ( n & (n-1) ) == 0 && (n) != 0 );
}

inline UINT64 NextMultiple( UINT64 value, UINT64 multiple )
{
   SIMUL_ASSERT( IsPowerOfTwo(multiple) );

	return (value + multiple - 1) & ~(multiple - 1);
}

template< class T > UINT64 BytePtrToUint64( _In_ T* ptr )
{
	return static_cast< UINT64 >( reinterpret_cast< BYTE* >( ptr ) - static_cast< BYTE* >( nullptr ) );
}

PlatformConstantBuffer::PlatformConstantBuffer() :
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

static float megas = 0.0f;
void PlatformConstantBuffer::CreateBuffers(crossplatform::RenderPlatform* r, void* addr) 
{
	renderPlatform = r;
	if (addr)
		SIMUL_BREAK("Nacho has to check this");

	// Create the heaps (storage for our descriptors)
	// mBufferSize / 256 (this is the max number of descriptors we can hold with this upload heaps
	mMaxDescriptors = mBufferSize / (kBufferAlign * mSlots);
	for (unsigned int i = 0; i < 3; i++)
	{
		mHeaps[i].Release();
		mHeaps[i].Restore((dx12::RenderPlatform*)renderPlatform, mMaxDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, base::QuickFormat("PlatformConstantBuffer %016x CBHeap %d",this,i), false);
		delete [] cpuDescriptorHandles[i];
		cpuDescriptorHandles[i]=new D3D12_CPU_DESCRIPTOR_HANDLE[mMaxDescriptors];
	}
	// Create the upload heap (the gpu memory that will hold the constant buffer)
	// Each upload heap has 64KB.  (64KB aligned)
	for (unsigned int i = 0; i < 3; i++)
	{
		HRESULT res;
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			SIMUL_PPV_ARGS(&mUploadHeap[i])
		);
		SIMUL_ASSERT(res == S_OK);
		SIMUL_GPU_TRACK_MEMORY(mUploadHeap[i], mBufferSize)

		// Just debug memory usage
		megas += (float)(mBufferSize) / 1048576.0f;
		
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
		//SIMUL_COUT << "Total MB Allocated in the GPU: " << std::to_string(megas) << std::endl;
	}
}

void PlatformConstantBuffer::SetNumBuffers(crossplatform::RenderPlatform *r, UINT nContexts,  UINT nMapsPerFrame, UINT nFramesBuffering )
{
	CreateBuffers( r, nullptr);
}


D3D12_CPU_DESCRIPTOR_HANDLE simul::dx12::PlatformConstantBuffer::AsD3D12ConstantBuffer()
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
		std::string str= base::QuickFormat("%s mUploadHeap %d",name.c_str(),i);
		renderPlatformDx12->PushToReleaseManager(mUploadHeap[i],str.c_str());
		mUploadHeap[i]=nullptr;
		delete [] cpuDescriptorHandles[i];
		cpuDescriptorHandles[i]=nullptr;
	}
	renderPlatform=nullptr;
}

void PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &deviceContext, size_t size, void *addr)
{
	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	// If new frame, update current frame index and reset the apply count
	if (last_frame_number != deviceContext.frame_number)
	{
		last_frame_number = deviceContext.frame_number;
		buffer_index++;
		if(buffer_index>=kNumBuffers)
			buffer_index=0;
		mCurApplyCount = 0;
	}
	if (mCurApplyCount >= mMaxDescriptors)
	{
		// This should really be solved by having like some kind of pool? Or allocating more space, something like that
		SIMUL_BREAK_ONCE("This ConstantBuffer reached its maximum apply count");
		return;
	}

	// pDest points at the begining of the uploadHeap, we can offset it! (we created 64KB and each Constant buffer
	// has a minimum size of kBufferAlign)
	UINT8* pDest = nullptr;
	UINT64 offset = (kBufferAlign * mSlots) * mCurApplyCount;	
	const CD3DX12_RANGE mapRange(0, 0);
	HRESULT hResult=mUploadHeap[buffer_index]->Map(0, &mapRange, reinterpret_cast<void**>(&pDest));
	if(hResult==S_OK)
	{
		memcpy(pDest + offset, addr, size);
		const CD3DX12_RANGE unMapRange(offset, offset+size);
		mUploadHeap[buffer_index]->Unmap(0, &unMapRange);
	}
	else
	{
		ID3D12DeviceRemovedExtendedData *pDred;
		rPlat->AsD3D12Device()->QueryInterface(IID_PPV_ARGS(&pDred));

		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
		D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
		pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput);
		pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput);
		auto n=DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
		while(n)
		{
			if(n->pCommandQueueDebugNameW)
				std::cerr<<simul::base::WStringToUtf8(n->pCommandQueueDebugNameW).c_str()<<std::endl;
			if(n->pCommandListDebugNameW)
				std::cerr<<simul::base::WStringToUtf8(n->pCommandListDebugNameW).c_str()<<std::endl;
			n=n->pNext;
		}
		SIMUL_BREAK_ONCE("hResult != S_OK");

	}

	mCurApplyCount++;
}

void  PlatformConstantBuffer::ActualApply(crossplatform::DeviceContext&, crossplatform::EffectPass* , int )
{
}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext& )
{
}
