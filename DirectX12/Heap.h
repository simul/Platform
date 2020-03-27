/*
	Heap.h 
*/

#pragma once

#include "Export.h"
#include "DirectXHeader.h"

#include <string>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251) //! "needs to have dll-interface to be used by clients of class"
#endif

namespace simul
{
	namespace dx12
	{
		class RenderPlatform;
	}
	namespace dx12
	{
		//! DirectX12 Descriptor Heap helper class
		class SIMUL_DIRECTX12_EXPORT Heap
		{
		public:
			Heap();
			~Heap();
			//! Recreates the API DescriporHeap with the provided settings
			void							Restore(dx12::RenderPlatform* r, UINT totalCnt, D3D12_DESCRIPTOR_HEAP_TYPE type, const char* name = "Heap", bool shaderVisible = true);
			//! Offsets both CPU and GPU handles
			void							Offset(int index = 1);
			CD3DX12_CPU_DESCRIPTOR_HANDLE	CpuHandle();
			CD3DX12_GPU_DESCRIPTOR_HANDLE	GpuHandle();
			UINT							GetCount();
			ID3D12DescriptorHeap*			GetHeap();
			//! Returns a CPU handle from the start of the heap at the provided offset
			D3D12_CPU_DESCRIPTOR_HANDLE		GetCpuHandleFromStartAfOffset(UINT off);
			UINT							GetHandleIncrement()const { return mHandleIncrement; }
			//! This method won't release or destroy anything, it will reset the internal count of handles
			//!	and make the held handles point at the start of the heap
			void Reset()
			{
				mCpuHandle = mHeap->GetCPUDescriptorHandleForHeapStart();
				mGpuHandle = mHeap->GetGPUDescriptorHandleForHeapStart();
				mCnt = 0;
			}
			void Release();

		private:
			dx12::RenderPlatform* renderPlatform;
			ID3D12DescriptorHeap*			mHeap;
			CD3DX12_CPU_DESCRIPTOR_HANDLE	mCpuHandle;
			CD3DX12_GPU_DESCRIPTOR_HANDLE	mGpuHandle;
			UINT							mHandleIncrement;

			UINT							mTotalCnt;
			UINT							mCnt;
			std::string						mName;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif