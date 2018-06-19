#pragma once
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/DirectX12/Export.h"

#include "SimulDirectXHeader.h"

#include <string>
#include <map>

#pragma warning(push)
#pragma warning(disable:4251)

//! Number of backbuffers
static const UINT			FrameCount = 3;

namespace simul
{
	namespace dx12
	{
		struct Window;
		typedef std::map<HWND, Window*> WindowMap;
		typedef std::map<int, IDXGIOutput*> OutputMap;

		//! Window class that holds the swap chain and the surfaces used to render
		struct SIMUL_DIRECTX12_EXPORT Window
		{
				 Window();
				 ~Window();
			void RestoreDeviceObjects(ID3D12Device* d3dDevice, bool m_vsync_enabled, int numerator, int denominator);
			void Release();
			void CreateRenderTarget(ID3D12Device* d3dDevice);
			void SetRenderer(crossplatform::PlatformRendererInterface *ci, int view_id);
			void ResizeSwapChain(ID3D12Device* d3dDevice);
			void SetCommandQueue(ID3D12CommandQueue *commandQueue);
            
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
			crossplatform::PlatformRendererInterface*	IRendererRef;

			//! The id assigned by the renderer to correspond to this hwnd
			int											WindowUID;			
			bool										vsync;
            //! If we called ResizeSwapChain(), we will cache that a resize should happen:
            bool                                        MustResize;

			//! The swap chain used to present the rendered scene
#ifdef _XBOX_ONE
			IDXGISwapChain1*							SwapChain;
#else
			IDXGISwapChain3*							SwapChain;
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
		};

        struct ImmediateContext
        {
            ID3D12GraphicsCommandList*  ICommandList;
            ID3D12CommandAllocator*     IAllocator;
            bool                        IRecording;
        };

		//! Manages the rendering
		class SIMUL_DIRECTX12_EXPORT Direct3D12Manager: public crossplatform::GraphicsDeviceInterface
		{
		public:
										Direct3D12Manager();
										~Direct3D12Manager();

			//! Initializes the manager, finds an adapter, checks feature level, creates a rendering device and a command queue
			void						Initialize(bool use_debug=false,bool instrument= false, bool default_driver = false);
			//! Add a window. Creates a new Swap Chain.
			void						AddWindow(HWND h);
			//! Removes the window and destroys its associated Swap Chain.
			void						RemoveWindow(HWND h);
			void						Shutdown();
			IDXGISwapChain*				GetSwapChain(HWND hwnd);
			void						Render(HWND hwnd);
			
			void						SetRenderer(HWND hwnd,crossplatform::PlatformRendererInterface *ci,int view_id);
			void						SetFullScreen(HWND hwnd,bool fullscreen,int which_output);
			void						ResizeSwapChain(HWND hwnd);
			void*						GetDevice();
			void*						GetDeviceContext();

            void*                       GetImmediateCommandList();
            void                        FlushImmediateCommandList();

			void*						GetCommandQueue();
			int							GetNumOutputs();
			crossplatform::Output		GetOutput(int i);
			int							GetViewId(HWND hwnd);
			Window*						GetWindow(HWND hwnd);
			void						ReportMessageFilterState();

		protected:
            ImmediateContext            mIContext;
			//! Map of windows
			WindowMap					mWindows;
			//! Map of displays
			OutputMap					mOutputs;

			//! The D3D device
			ID3D12Device*				mDevice;
			//! Used to submit commands to the GPU
			ID3D12CommandQueue*			mCommandQueue;
		};
	}
}
#pragma warning(pop)
