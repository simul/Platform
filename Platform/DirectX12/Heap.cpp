#include "Heap.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"

namespace simul
{
	namespace dx12
	{
		Heap::Heap():
			mHeap(nullptr)
		{
			
		}

		void Heap::Restore(dx12::RenderPlatform* renderPlatform, UINT totalCnt, D3D12_DESCRIPTOR_HEAP_TYPE type, const char* name /*= Heap*/, bool shaderVisible /*=true*/)
		{
			HRESULT res = S_FALSE;
			auto device = renderPlatform->AsD3D12Device();
			SIMUL_ASSERT(device != nullptr);
			SIMUL_ASSERT(renderPlatform != nullptr);

			Release(renderPlatform);

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors				= totalCnt;
			desc.NodeMask					= 0;
			desc.Type						= type;
			desc.Flags						= shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			res								= device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap));
			SIMUL_ASSERT(res == S_OK);

			mHandleIncrement				= device->GetDescriptorHandleIncrementSize(type);
			mCpuHandle						= mHeap->GetCPUDescriptorHandleForHeapStart();
			mGpuHandle						= mHeap->GetGPUDescriptorHandleForHeapStart();
			mName							= name;
			mHeap->SetName(std::wstring(mName.begin(), mName.end()).c_str());

			mTotalCnt = totalCnt;
		}

		void Heap::Offset(int index)
		{
			mCpuHandle.Offset(index, mHandleIncrement);
			mGpuHandle.Offset(index, mHandleIncrement);
			mCnt++;
			if (mCnt > mTotalCnt)
			{
				SIMUL_BREAK_ONCE("Nacho has to fix this");
			}
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE Heap::CpuHandle()
		{
			return mCpuHandle;
		}

		CD3DX12_GPU_DESCRIPTOR_HANDLE Heap::GpuHandle()
		{
			return mGpuHandle;
		}

		UINT Heap::GetCount()
		{
			return mCnt;
		}

		ID3D12DescriptorHeap * Heap::GetHeap()
		{
			return mHeap;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE Heap::GetCpuHandleFromStartAfOffset(UINT off)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE h	= mHeap->GetCPUDescriptorHandleForHeapStart();
			h.ptr							+= off * mHandleIncrement;
			return h;
		}

		void Heap::Release(dx12::RenderPlatform* r)
		{
			if (mHeap)
			{
				r->PushToReleaseManager(mHeap,mName);
			}
			mHeap		= nullptr;
			mCnt		= 0;
			mTotalCnt	= 0;
		}
	}
}
