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
		mHeaps[i].Restore((dx12::RenderPlatform*)renderPlatform, mMaxDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, platform::core::QuickFormat("PlatformConstantBuffer %016x CBHeap %d",this,i), false);
		delete [] cpuDescriptorHandles[i];
		cpuDescriptorHandles[i]=new D3D12_CPU_DESCRIPTOR_HANDLE[mMaxDescriptors];
	}
	// Create the upload heap (the gpu memory that will hold the constant buffer)
	// Each upload heap has 64KB.  (64KB aligned)
	for (unsigned int i = 0; i < 3; i++)
	{
		HRESULT res;
		auto uploadHeapProperties=CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadDesc=CD3DX12_RESOURCE_DESC::Buffer(mBufferSize);
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			SIMUL_PPV_ARGS(&mUploadHeap[i])
		);
		SIMUL_ASSERT(res == S_OK);
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
		std::string str= platform::core::QuickFormat("%s mUploadHeap %d",name.c_str(),i);
		renderPlatformDx12->PushToReleaseManager(mUploadHeap[i],str.c_str());
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

void  PlatformConstantBuffer::ActualApply(crossplatform::DeviceContext & deviceContext, crossplatform::EffectPass* , int )
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
		SIMUL_BREAK_ONCE("This ConstantBuffer reached its maximum apply count");
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
#if !defined(_GAMING_XBOX) 
		ID3D12DeviceRemovedExtendedData* pDred = nullptr;
		HRESULT res = rPlat->AsD3D12Device()->QueryInterface(IID_PPV_ARGS(&pDred));
		if (SUCCEEDED(res) && pDred)
		{
			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
			res = pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput);
			if (SUCCEEDED(res))
			{
				std::cerr << "DRED - Breadcrumbs:" << std::endl;
				const D3D12_AUTO_BREADCRUMB_NODE* breadcrumb = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
				while (breadcrumb)
				{
					auto D3D12_AUTO_BREADCRUMB_OP_ToString = [](D3D12_AUTO_BREADCRUMB_OP op) -> std::string
					{
						switch (op)
						{
						case D3D12_AUTO_BREADCRUMB_OP_SETMARKER:
							return "D3D12_AUTO_BREADCRUMB_OP_SETMARKER";
						case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT:
							return "D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT";
						case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT:
							return "D3D12_AUTO_BREADCRUMB_OP_ENDEVENT";
						case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED:
							return "D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED";
						case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED:
							return "D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED";
						case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT:
							return "D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT";
						case D3D12_AUTO_BREADCRUMB_OP_DISPATCH:
							return "D3D12_AUTO_BREADCRUMB_OP_DISPATCH";
						case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION:
							return "D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION";
						case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION:
							return "D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION";
						case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE:
							return "D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE";
						case D3D12_AUTO_BREADCRUMB_OP_COPYTILES:
							return "D3D12_AUTO_BREADCRUMB_OP_COPYTILES";
						case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE:
							return "D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE";
						case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW:
							return "D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW";
						case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW:
							return "D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW";
						case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW:
							return "D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW";
						case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER:
							return "D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER";
						case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE:
							return "D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE";
						case D3D12_AUTO_BREADCRUMB_OP_PRESENT:
							return "D3D12_AUTO_BREADCRUMB_OP_PRESENT";
						case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA:
							return "D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA";
						case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION:
							return "D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION";
						case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION:
							return "D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION";
						case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME:
							return "D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME";
						case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES:
							return "D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES";
						case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT:
							return "D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT";
						case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64:
							return "D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64";
						case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION:
							return "D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION";
						case D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE:
							return "D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE";
						case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1:
							return "D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1";
						case D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION:
							return "D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION";
						case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2:
							return "D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2";
						case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1:
							return "D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1";
						case D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE:
							return "D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE";
						case D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO:
							return "D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO";
						case D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE:
							return "D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE";
						case D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS:
							return "D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS";
						case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND:
							return "D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND";
						case D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND:
							return "D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND";
						case D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION:
							return "D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION";
						case D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP:
							return "D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP";
						case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1:
							return "D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1";
						case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND:
							return "D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND";
						case D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND:
							return "D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND";
						case D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH:
							return "D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH";
					#if (WDK_NTDDI_VERSION > NTDDI_WIN10_VB)
						case D3D12_AUTO_BREADCRUMB_OP_ENCODEFRAME:
							return "D3D12_AUTO_BREADCRUMB_OP_ENCODEFRAME";
						case D3D12_AUTO_BREADCRUMB_OP_RESOLVEENCODEROUTPUTMETADATA:
							return "D3D12_AUTO_BREADCRUMB_OP_RESOLVEENCODEROUTPUTMETADATA";
					#endif
						default:
							return "";
						}
					};

					if (breadcrumb->pCommandListDebugNameA)
						std::cerr << breadcrumb->pCommandListDebugNameA;
					else if(breadcrumb->pCommandListDebugNameW)
						std::cerr << platform::core::WStringToUtf8(breadcrumb->pCommandListDebugNameW);
					else
						std::cerr << "Unknown CommandListDebugName";
					std::cerr << " : " << std::hex << "0x" << (uint64_t)(void*)(breadcrumb->pCommandList) << std::dec << std::endl;
					
					if (breadcrumb->pCommandQueueDebugNameA)
						std::cerr << breadcrumb->pCommandQueueDebugNameA;
					else if (breadcrumb->pCommandQueueDebugNameW)
						std::cerr << platform::core::WStringToUtf8(breadcrumb->pCommandQueueDebugNameW);
					else
						std::cerr << "Unknown CommandQueueDebugName";
					std::cerr << " : " << std::hex << "0x" << (uint64_t)(void*)(breadcrumb->pCommandQueue) << std::dec << std::endl;

					if (breadcrumb->pCommandHistory)
					{
						std::cerr << "CommandHistory - GPU Operation Entries: " << breadcrumb->BreadcrumbCount << std::endl;
						for (UINT32 i = 0; i < breadcrumb->BreadcrumbCount; i++)
						{
							D3D12_AUTO_BREADCRUMB_OP gpuOperation = breadcrumb->pCommandHistory[i];
							std::cerr << i << ": " << D3D12_AUTO_BREADCRUMB_OP_ToString(gpuOperation) << std::endl;
						}
						std::cerr << "CommandHistory - Last Render Or Compute GPU Operation" << std::endl;
						if (breadcrumb->pLastBreadcrumbValue)
						{
							const UINT32 lastRenderOrComputeGPUOperation = *(breadcrumb->pLastBreadcrumbValue);
							D3D12_AUTO_BREADCRUMB_OP gpuOperation = breadcrumb->pCommandHistory[lastRenderOrComputeGPUOperation];
							std::cerr << D3D12_AUTO_BREADCRUMB_OP_ToString(gpuOperation) << std::endl;
						}
					}
					breadcrumb = breadcrumb->pNext;
					std::cerr << std::endl;
				}
			}
			else
			{
				SIMUL_CERR << "ID3D12DeviceRemovedExtendedDataSettings::SetAutoBreadcrumbsEnablement() is not set correctly." << std::endl;
			}

			D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
			res = pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput);
			if (SUCCEEDED(res))
			{
				std::cerr << "DRED - Page Fault on GPU Virtual Address: " << DredPageFaultOutput.PageFaultVA << std::endl;

				auto D3D12_DRED_ALLOCATION_TYPE_ToString = [](D3D12_DRED_ALLOCATION_TYPE type) -> std::string
				{
					switch (type)
					{
					case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE:
						return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE";
					case D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR:
						return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR";
					case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE:
						return "D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE";
					case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST:
						return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST";
					case D3D12_DRED_ALLOCATION_TYPE_FENCE:
						return "D3D12_DRED_ALLOCATION_TYPE_FENCE";
					case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP:
						return "D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP";
					case D3D12_DRED_ALLOCATION_TYPE_HEAP:
						return "D3D12_DRED_ALLOCATION_TYPE_HEAP";
					case D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP:
						return "D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP";
					case D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE:
						return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE";
					case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY:
						return "D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY";
					case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER:
						return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER";
					case D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR:
						return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR";
					case D3D12_DRED_ALLOCATION_TYPE_RESOURCE:
						return "D3D12_DRED_ALLOCATION_TYPE_RESOURCE";
					case D3D12_DRED_ALLOCATION_TYPE_PASS:
						return "D3D12_DRED_ALLOCATION_TYPE_PASS";
					case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION:
						return "D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION";
					case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY:
						return "D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY";
					case D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION:
						return "D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION";
					case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP:
						return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP";
					case D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL:
						return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL";
					case D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER:
						return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER";
					case D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT:
						return "D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT";
					case D3D12_DRED_ALLOCATION_TYPE_METACOMMAND:
						return "D3D12_DRED_ALLOCATION_TYPE_METACOMMAND";
					case D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP:
						return "D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP";
					case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR:
						return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR";
					case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP:
						return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP";
					case D3D12_DRED_ALLOCATION_TYPE_VIDEO_EXTENSION_COMMAND:
						return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_EXTENSION_COMMAND";
				#if (WDK_NTDDI_VERSION > NTDDI_WIN10_VB)
					case D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER:
						return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER";
					case D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER_HEAP:
						return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER_HEAP";
				#endif
					case D3D12_DRED_ALLOCATION_TYPE_INVALID:
						return "D3D12_DRED_ALLOCATION_TYPE_INVALID";
					default:
						return "";
					}
				};
				std::cerr << "DRED - Existing Allocations:" << std::endl;
				const D3D12_DRED_ALLOCATION_NODE* existingNode = DredPageFaultOutput.pHeadExistingAllocationNode;
				while (existingNode)
				{
					if (existingNode->ObjectNameA)
						std::cerr << existingNode->ObjectNameA;
					else if (existingNode->ObjectNameW)
						std::cerr << platform::core::WStringToUtf8(existingNode->ObjectNameW);
					else
						std::cerr << "Unknown Object";
					std::cerr << " : " << D3D12_DRED_ALLOCATION_TYPE_ToString(existingNode->AllocationType) << std::endl;
					existingNode = existingNode->pNext;
					std::cerr << std::endl;
				}
				std::cerr << "DRED - Freed Allocations:" << std::endl;
				const D3D12_DRED_ALLOCATION_NODE* freedNode = DredPageFaultOutput.pHeadRecentFreedAllocationNode;
				while (freedNode)
				{
					if (freedNode->ObjectNameA)
						std::cerr << freedNode->ObjectNameA;
					else if (freedNode->ObjectNameW)
						std::cerr << platform::core::WStringToUtf8(freedNode->ObjectNameW);
					else
						std::cerr << "Unknown Object";
					std::cerr << " : " << D3D12_DRED_ALLOCATION_TYPE_ToString(freedNode->AllocationType) << std::endl;
					freedNode = freedNode->pNext;
					std::cerr << std::endl;
				}
			}
			else
			{
				SIMUL_CERR << "ID3D12DeviceRemovedExtendedDataSettings::SetPageFaultEnablement() is not set correctly." << std::endl;
			}
		}
#endif
		SIMUL_BREAK_ONCE("hResult != S_OK");
	}
	mCurApplyCount++;
}

void PlatformConstantBuffer::Unbind(platform::crossplatform::DeviceContext& )
{
}
