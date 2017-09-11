/*
	Fence.h
*/

#pragma once

#include "Export.h"
#include "SimulDirectXHeader.h"


namespace simul
{
	namespace dx11on12
	{
		class RenderPlatform;
	}
	namespace dx12
	{
		/// DirectX12 Fence helper class
		class SIMUL_DIRECTX12_EXPORT Fence
		{
		public:
					Fence();
					~Fence();
			/// Recreates the internal fence objects 
			void	Restore(dx11on12::RenderPlatform* renderPlatform);
			/// Adds a signal
			void	Signal(dx11on12::RenderPlatform* renderPlatform);
			/// Waits for the previous signal to be completed
			void	WaitUntilCompleted(dx11on12::RenderPlatform* renderPlatform);
			/// The GPU waits for the fence value
			void	WaitGPU(dx11on12::RenderPlatform* renderPlatform);
			void	Release();

		private:
			ID3D12Fence*	mFence;
			INT64			mFenceValue;
			INT64			mFenceSignal;
			HANDLE			mFenceEvent;
			bool			mSignaled;
		};
	}
}