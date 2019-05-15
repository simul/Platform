#pragma once

#include "Simul/Platform/CrossPlatform/DisplaySurface.h"
#include "SimulDirectXHeader.h"
#include "Export.h"

static const UINT FrameCount = 3;

namespace simul
{
    namespace dx12
    {
        class SIMUL_DIRECTX12_EXPORT DisplaySurface : public crossplatform::DisplaySurface
        {
        public:
            DisplaySurface();
            ~DisplaySurface();
			//! Platform-dependent function called when initializing the display surface.
            void RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)override;
			//! Platform-dependent function called when uninitializing the display surface.
            void InvalidateDeviceObjects()override;
			//! Render to the display surface. Requires a reference to the mutex to make sure that this rendering doesn't take place at the same time as other render calls.
            void Render(simul::base::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber);
            
        private:
			unsigned GetCurrentBackBufferIndex() const;
            //! Query backbuffers and create render targets
            void CreateRenderTargets(ID3D12Device* device);
            //! Creates the fences used to synchronize with the GPU
            void CreateSyncObjects();
            //! Ensures we can start rendering to the current buffer
            void StartFrame();
            void EndFrame();
            //! Checks all the fences (eg. after this method its safe to release resources)
            void WaitForAllWorkDone();
            void Resize();

            ID3D12Device*                               mDeviceRef;
            ID3D12CommandQueue*                         mQueue;
            //! The swap chain used to present the rendered scene
#ifdef _XBOX_ONE
            IDXGISwapChain1*							mSwapChain;
#else
            IDXGISwapChain3*							mSwapChain;
#endif
            //! Render viewport
            D3D12_VIEWPORT								mCurViewport;
            //! Scissor if used
            CD3DX12_RECT								mCurScissor;
            //! The actual backbuffer resources
            ID3D12Resource*								mBackBuffers[3];
            //! Heap to store views to the backbuffers
            ID3D12DescriptorHeap*						mRTHeap;
            //! The views of each backbuffers
            CD3DX12_CPU_DESCRIPTOR_HANDLE				mRTHandles[FrameCount];
            //! We need one command allocator (storage for commands) for each backbuffer
            ID3D12CommandAllocator*						mCommandAllocators[FrameCount];
            //! Event used to synchronize
            HANDLE						                mWindowEvent;
            //! Fences to syn with the GPU
            ID3D12Fence*				                mGPUFences[FrameCount];
            //! Storage for the values of the fence
            UINT64						                mFenceValues[FrameCount];
            //! Used to record commands
            ID3D12GraphicsCommandList*	                mCommandList;
            bool                                        mRecordingCommands;
        };
    }
}