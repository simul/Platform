#pragma once
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/DirectX12/Export.h"
#include "SimulDirectXHeader.h"

#include <string>
#include <map>

#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace dx12
	{
		struct Window;
		typedef std::map<HWND, Window*> WindowMap;
		typedef std::map<int, IDXGIOutput*> OutputMap;

		//! A DX12 window holds all the resources needed for rendering to a surface.
		struct SIMUL_DIRECTX12_EXPORT Window
		{
														Window();
														~Window();
			void										RestoreDeviceObjects(ID3D12Device* d3dDevice, bool m_vsync_enabled, int numerator, int denominator);
			void										Release();
			void										CreateRenderTarget(ID3D12Device* d3dDevice);
			void										CreateDepthStencil(ID3D12Device* d3dDevice);

			void										SetRenderer(crossplatform::PlatformRendererInterface *ci, int view_id);
			void										ResizeSwapChain(ID3D12Device* d3dDevice);

			HWND										hwnd;
			crossplatform::PlatformRendererInterface*	renderer;

			//! The id assigned by the renderer to correspond to this hwnd
			int											view_id;			
			bool										vsync;

			//! Number of backbuffers 
			static const UINT							FrameCount = 3;
			UINT										FrameIndex;
			
			//! The actual backbuffer resources
			ID3D12Resource*								BackBuffers[3];											
			//! Heap to store views to the backbuffers
			ID3D12DescriptorHeap*						RTHeap;		
			//! The views of each backbuffers
			CD3DX12_CPU_DESCRIPTOR_HANDLE				RTHandles[FrameCount];
			//! Used to create the views
			UINT										RTDescriptorSize;
			//! Depth format
			DXGI_FORMAT									DepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
			//! Depth stencil surface
			ID3D12Resource*								DepthStencilBuffer;
			//! Depth Stencil heap
			ID3D12DescriptorHeap*						DSHeap;

			//! The swap chain used to present the rendered scene
#ifdef _XBOX_ONE
			IDXGISwapChain1*							SwapChain;
#else
			IDXGISwapChain3*							SwapChain;
#endif
			//! We need one command allocator (storage for commands) for each backbuffer
			ID3D12CommandAllocator*						CommandAllocators[FrameCount];
			//! Reference to the command queue
			ID3D12CommandQueue*							CommandQueue;
			//! A command list used to record commands
			ID3D12GraphicsCommandList*					CommandList;

			//! Event used to synchronize
			HANDLE										FenceEvent;
			//! A d3d fence object
			ID3D12Fence*								Fence;
			//! Storage for the values of the fence
			UINT64										FenceValues[FrameCount];

			//! Rendered viewport
			D3D12_VIEWPORT								Viewport;
			//! Scissor rect
			CD3DX12_RECT								Scissor;
		};

		//! Creates a DX12 device object and holds the different outputs (windows)
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
			void*						GetDevice()					{ return mDevice; }
			void*						GetDeviceContext()			{ return 0; }
			void*						GetCommandList(HWND hwnd)	{ return GetWindow(hwnd)->CommandList;; }
			void*						GetCommandQueue(HWND hwnd)	{ return GetWindow(hwnd)->CommandQueue; }
			int							GetNumOutputs();
			crossplatform::Output		GetOutput(int i);

			void						WaitForGpu(Window* w);
			void						InitialWaitForGpu(HWND hwnd);
			int							GetViewId(HWND hwnd);
			Window*						GetWindow(HWND hwnd);
			void						ReportMessageFilterState();
			unsigned int				GetCurrentBackbufferIndex(HWND hwnd) { return GetWindow(hwnd)->FrameIndex; }

		protected:
			void						MoveToNextFrame(Window *w);

			bool						m_vsync_enabled;
			int							m_videoCardMemory;
			char						m_videoCardDescription[128];

			//! Windows referenced by this manager
			WindowMap					mWindows;
			//! Map of possible outputs (displays)
			OutputMap					mOutputs;
			//! The D3D device
			ID3D12Device*				mDevice;
		};
	}
}
#pragma warning(pop)
