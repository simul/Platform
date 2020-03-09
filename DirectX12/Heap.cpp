#include "Heap.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/DirectX12/RenderPlatform.h"

namespace simul
{
	namespace dx12
	{
		Heap::Heap():
			renderPlatform(nullptr)
			,mHeap(nullptr)
		{
			
		}

		void Heap::Restore(dx12::RenderPlatform* r, UINT totalCnt, D3D12_DESCRIPTOR_HEAP_TYPE type, const char* name /*= Heap*/, bool shaderVisible /*=true*/)
		{
			HRESULT res = S_FALSE;

			Release();
			
			auto device = r->AsD3D12Device();
			SIMUL_ASSERT(device != nullptr);
			SIMUL_ASSERT(r != nullptr);
			renderPlatform=r;

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors				= totalCnt;
			desc.NodeMask					= 0;
			desc.Type						= type;
			desc.Flags						= shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
#if defined(_XBOX_ONE) || defined(_GAMING_XBOX)
			res								= device->CreateDescriptorHeap(&desc, IID_GRAPHICS_PPV_ARGS(&mHeap));
#else
			res								= device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap));
#endif
			SIMUL_ASSERT(res == S_OK);
			SIMUL_GPU_TRACK_MEMORY(mHeap, totalCnt) // Of course, not the actual memory size. But what is? D3D doesn't want us to know...

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
			mCnt += index;
			if (mCnt > mTotalCnt)
			{
				SIMUL_BREAK_ONCE("This heap reached the maximum capacity!");
				Reset();
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

		void Heap::Release()
		{
			if (mHeap)
			{
				renderPlatform->PushToReleaseManager(mHeap,mName.c_str());
			}
			mHeap		= nullptr;
			mCnt		= 0;
			mTotalCnt	= 0;
			renderPlatform=nullptr;
		}
	}
}
