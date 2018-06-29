#pragma once
#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/CrossPlatform/SwapChain.h"
#include "Simul/Platform/DirectX12/Export.h"

#include "SimulDirectXHeader.h"
#include <string>

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif


namespace simul
{
	namespace dx12
	{
		static const UINT			FrameCount = 3;
		class SwapChain:public crossplatform::SwapChain
		{
		protected:
            
            //! Checks if it is safe to start rendering to the current buffer.
            //! This method will block if needed
            void StartFrame();
            //! Schedules a present command and setups required sync for next frames.
            void EndFrame();
            //! Waits until all GPU work is completed
            void WaitForAllWorkDone();

            //! Returns the current backbuffer index (0,1,2 if 3x buffering)
            UINT GetCurrentIndex();

			HWND										ConsoleWindowHandle;

			//! The id assigned by the renderer to correspond to this hwnd
			int											WindowUID;			
			bool										vsync;
            //! If we called ResizeSwapChain(), we will cache that a resize should happen:
            bool                                        MustResize;

			//! The swap chain used to present the rendered scene
#ifdef _XBOX_ONE
			IDXGISwapChain1*							pSwapChain;
#else
			IDXGISwapChain3*							pSwapChain;
#endif
				
			//! Rendered viewport
			D3D12_VIEWPORT								CurViewport;
			//! Scissor if used
			CD3DX12_RECT								CurScissor;
			//! The actual backbuffer resources
			ID3D12Resource*								BackBuffers[3];

			//! Heap to store views to the backbuffers
			ID3D12DescriptorHeap*						RTHeap;		
			//! The views of each backbuffers
			CD3DX12_CPU_DESCRIPTOR_HANDLE				RTHandles[FrameCount];
			//! We need one command allocator (storage for commands) for each backbuffer
			ID3D12CommandAllocator*						CommandAllocators[FrameCount];
			//! Reference to the command queue
			ID3D12CommandQueue*							CommandQueueRef;
			//! Reference to the device
			ID3D12Device*								DeviceRef;

            //! Event used to synchronize
            HANDLE						                WindowEvent;
            //! Fences to syn with the GPU
            ID3D12Fence*				                GPUFences[FrameCount];
            //! Storage for the values of the fence
            UINT64						                FenceValues[FrameCount];

            //! Used to record commands
            ID3D12GraphicsCommandList*	                CommandList;
            bool                                        RecordingCommands;

        private:
            //! Called when we add the window
            void CreateSyncObjects();
			void Resize();
			void SetCommandQueue(ID3D12CommandQueue *commandQueue);
		public:
			SwapChain();
			virtual ~SwapChain();
			virtual void InvalidateDeviceObjects();
			/// Set the size and format
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r,crossplatform::DeviceContext &deviceContext,cp_hwnd h,crossplatform::PixelFormat,int count);
			bool IsFullscreen() const;
			void SetFullscreen(bool);
			IDXGISwapChain *AsDXGISwapChain();
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)  
#endif