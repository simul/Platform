#pragma once

#include "Export.h"
#include "Platform/Core/RuntimeError.h"
#include "SimulDirectXHeader.h"

namespace simul
{
	namespace dx12
	{
		class RenderPlatform;
		struct Fence;
		//! A class to implement common video encodng/decoding functionality for DirectX 12.
		class SIMUL_DIRECTX12_EXPORT CommandListController
		{
		public:
			CommandListController();
			~CommandListController();
			void Initialize(RenderPlatform* renderPlatform, D3D12_COMMAND_LIST_TYPE commandListType, const char* name, ID3D12CommandQueue* commandQueue = nullptr, uint32_t numAllocators = 4);
			void Release();
			void ResetCommandList();
			void ExecuteCommandList();
			// Waits until the GPU is finished with the command allocator at the index.
			// The allocator will be the last one used if the index is less than 0.
			void WaitOnGPU(int allocIndex = -1);

			ID3D12CommandQueue* GetCommandQueue() const
			{
				return mCommandQueue;
			}

			ID3D12CommandList* GetCommandList() const
			{
				return mCommandList;
			}

		private:
			void CloseCommandList();
			ID3D12CommandQueue* mCommandQueue;
			ID3D12CommandAllocator** mAllocators;
			ID3D12CommandList* mCommandList;
			Fence** mFences;
			uint32_t mNumAllocators;
			uint32_t mIndex;
			bool mCommandQueueOwner;
			bool mCommandListRecording;
			HANDLE	mCompletionEvent;
		};
	}
}