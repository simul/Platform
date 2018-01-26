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

			HWND										ConsoleWindowHandle;
			crossplatform::PlatformRendererInterface*	IRendererRef;

			//! The id assigned by the renderer to correspond to this hwnd
			int											WindowUID;			
			bool										vsync;

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
			void*						GetCommandList();
			void*						GetCommandQueue();
			int							GetNumOutputs();
			crossplatform::Output		GetOutput(int i);

			void						InitialWaitForGpu();
			int							GetViewId(HWND hwnd);
			Window*						GetWindow(HWND hwnd);
			void						ReportMessageFilterState();
			unsigned int				GetCurrentBackbufferIndex() { return mCurFrameIdx; }

		protected:
			void						WaitForGpu();
			void						MoveToNextFrame(Window *window);

			//! Map of windows
			WindowMap					windows;
			//! Map of displays
			OutputMap					outputs;

			//! The D3D device
			ID3D12Device*				mDevice;
			//! The command queue
			ID3D12CommandQueue*			mCommandQueue;		
			//! A command list used to record commands
			ID3D12GraphicsCommandList*	mCommandList;

			//! The current frame index 
			UINT						mCurFrameIdx;

			//! Event used to synchronize
			HANDLE						mFenceEvent;
			//! A d3d fence object
			ID3D12Fence*				mFence;
			//! Storage for the values of the fence
			UINT64						mFenceValues[FrameCount];
		};
	}
}
#pragma warning(pop)
