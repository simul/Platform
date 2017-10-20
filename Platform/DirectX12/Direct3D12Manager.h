#pragma once
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/DirectX12/Export.h"

#include "SimulDirectXHeader.h"

#include <string>
#include <map>

#pragma warning(push)
#pragma warning(disable:4251)

const UINT kNumBackBuffers = 3;

namespace simul
{
	namespace dx12
	{
		struct Window;
		typedef std::map<HWND, Window*> WindowMap;
		typedef std::map<int, IDXGIOutput*> OutputMap;

		//! Container for dx12 resources created by the window
		struct WindowResourcesD3D12
		{
#ifdef _XBOX_ONE
			IDXGISwapChain1*							SwapChain;
#else
			IDXGISwapChain3*							SwapChain;
#endif

			ID3D12DescriptorHeap*						RtHeap;
			ID3D12Resource*								ColourSurfaces[kNumBackBuffers];
			CD3DX12_CPU_DESCRIPTOR_HANDLE				RtViews[kNumBackBuffers];

			ID3D12DescriptorHeap*						DsHeap;
			ID3D12Resource*								DepthStencilSurface;

			ID3D12CommandAllocator*						CommandAllocators[kNumBackBuffers];
			ID3D12CommandQueue*							CommandQueue;

			ID3D12GraphicsCommandList*					CommandList;
		};

		//! Synchronization for the window
		struct WindowSyncD3D12
		{
			HANDLE						Event;
			ID3D12Fence*				Fence;
			UINT64						FenceValues[kNumBackBuffers];
		};

		//! Window used for rendering
		struct SIMUL_DIRECTX12_EXPORT Window
		{
						Window();
						~Window();

			void		RestoreDeviceObjects(ID3D12Device* d3dDevice, bool m_vsync_enabled, int numerator, int denominator);
			void		Release();
			void		SetRenderer(crossplatform::PlatformRendererInterface *ci, int view_id);
			void		ResizeSwapChain(ID3D12Device* d3dDevice);

			float		GetWidth()const		{ return Viewport.Width; }
			float		GetHeight()const	{ return Viewport.Height; }

			HWND					hwnd;

			//! The id assigned by the renderer to correspond to this hwnd
			int						view_id;			

			bool					VSync;

			WindowResourcesD3D12	WindowResources;
			WindowSyncD3D12			WindowSync;

			UINT					FrameIndex;
			//! Depth format
			DXGI_FORMAT				DepthFormat = DXGI_FORMAT_D32_FLOAT;
			//! Colour format
			DXGI_FORMAT				ColourFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
			//! Rendered viewport
			D3D12_VIEWPORT			Viewport;
			//! Scissor if used
			CD3DX12_RECT			Scissor;

			crossplatform::PlatformRendererInterface*	renderer;

		private:
			void										CreateRenderTarget(ID3D12Device* d3dDevice);
			void										CreateDepthStencil(ID3D12Device* d3dDevice);
		};

		//! Manages the rendering
		class SIMUL_DIRECTX12_EXPORT Direct3D12Manager: public crossplatform::GraphicsDeviceInterface
		{
		public:
									Direct3D12Manager();
									~Direct3D12Manager();

			//! Initializes the manager, finds an adapter, checks feature level, creates a rendering device and a command queue
			void					Initialize(bool use_debug=false,bool instrument=false);
			//! Add a window. Creates a new Swap Chain.
			void					AddWindow(HWND h);
			//! Removes the window and destroys its associated Swap Chain.
			void					RemoveWindow(HWND h);
			void					Shutdown();
			IDXGISwapChain*			GetSwapChain(HWND hwnd);
			void					Render(HWND hwnd);
			
			void					SetRenderer(HWND hwnd,crossplatform::PlatformRendererInterface *ci,int view_id);
			void					SetFullScreen(HWND hwnd,bool fullscreen,int which_output);
			void					ResizeSwapChain(HWND hwnd);
			void*					GetDevice12();
			void*					GetDevice() { return GetDevice12(); }
			void*					GetDeviceContext() { return 0; }
			void*					GetCommandList(HWND hwnd);
			void*					GetCommandQueue(HWND hwnd);
			int						GetNumOutputs();
			crossplatform::Output	GetOutput(int i);

			void					InitialWaitForGpu(HWND hwnd);
			int						GetViewId(HWND hwnd);
			Window*					GetWindow(HWND hwnd);
			void					ReportMessageFilterState();
			unsigned int			GetCurrentBackbufferIndex(HWND hwnd);

		protected:
			void					WaitForGpu(HWND hwnd);
			void					MoveToNextFrame(Window *window);

			bool					m_vsync_enabled;
			int						m_videoCardMemory;
			char					m_videoCardDescription[128];

			//! Windows held by this manager
			WindowMap				windows;

			//! Map of displays
			OutputMap				outputs;

			//! The D3D device
			ID3D12Device*			mDevice;

			bool mPendingImmediate = false;
			WindowResourcesD3D12	mImmediateWindow;
		};
	}
}
#pragma warning(pop)
