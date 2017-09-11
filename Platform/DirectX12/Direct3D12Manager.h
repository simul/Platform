#pragma once
#include "SimulDirectXHeader.h"
#include <map>
#include <string>
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/DirectX12/Export.h"

#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>

#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11on12
	{
		struct Window;
		typedef std::map<HWND, Window*> WindowMap;
		typedef std::map<int, IDXGIOutput*> OutputMap;

		/// Window class that holds the swap chain and the surfaces used to render (colour and depth)
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

			/// The id assigned by the renderer to correspond to this hwnd
			int											view_id;			
			bool										vsync;
			/// The swap chain used to present the rendered scene
			IDXGISwapChain3*							m_swapChain;

			void										setCommandQueue(ID3D12CommandQueue *commandQueue);

			/// Number of backbuffers 
			static const UINT							FrameCount = 3;
			UINT										m_frameIndex;
			
			/// The actual backbuffer resources
			ID3D12Resource*								m_backBuffers[3];											
			/// Heap to store views to the backbuffers
			ID3D12DescriptorHeap*						m_rtvHeap;		
			/// The views of each backbuffers
			CD3DX12_CPU_DESCRIPTOR_HANDLE				mRtvCpuHandle[FrameCount];
			/// Used to create the views
			UINT										m_rtvDescriptorSize;

			/// Depth format
			DXGI_FORMAT									mDepthStencilFmt = DXGI_FORMAT_D32_FLOAT;
			/// Depth stencil surface
			ID3D12Resource*								m_depthStencilTexture12;
			/// Depth Stencil heap
			ID3D12DescriptorHeap*						m_dsHeap;
			/// We need one command allocator (storage for commands) for each backbuffer
			ID3D12CommandAllocator*						m_commandAllocators[FrameCount];
			/// Reference to the command queue
			ID3D12CommandQueue*							m_dx12CommandQueue;

			/// Reference to the device
			ID3D12Device*								d3d12Device;

			/// Rendered viewport
			CD3DX12_VIEWPORT							m_viewport;
			/// Scissor if used
			CD3DX12_RECT								m_scissorRect;
		};

		/// Manages the rendering
		class SIMUL_DIRECTX12_EXPORT Direct3D11Manager: public crossplatform::GraphicsDeviceInterface
		{
		public:
									Direct3D11Manager();
									~Direct3D11Manager();

			/// Initializes the manager, finds an adapter, checks feature level, creates a rendering device and a command queue
			void					Initialize(bool use_debug=false,bool instrument=false);
			/// Add a window. Creates a new Swap Chain.
			void					AddWindow(HWND h);
			/// Removes the window and destroys its associated Swap Chain.
			void					RemoveWindow(HWND h);
			void					Shutdown();
			IDXGISwapChain*			GetSwapChain(HWND hwnd);
			void					Render(HWND hwnd);
			
			void					SetRenderer(HWND hwnd,crossplatform::PlatformRendererInterface *ci,int view_id);
			void					SetFullScreen(HWND hwnd,bool fullscreen,int which_output);
			void					ResizeSwapChain(HWND hwnd);
			void*					GetDevice12();
			void*					GetDevice() { return 0; }
			void*					GetDeviceContext() { return 0; }
			void*					GetCommandList() { return m_commandList; }
			void*					GetCommandQueue() { return m_commandQueue; }
			int						GetNumOutputs();
			crossplatform::Output	GetOutput(int i);

			void					InitialWaitForGpu();
			int						GetViewId(HWND hwnd);
			Window*					GetWindow(HWND hwnd);
			void					ReportMessageFilterState();
			unsigned int			GetCurrentBackbufferIndex() { return m_frameIndex; }

		protected:
			void						WaitForGpu();
			void						MoveToNextFrame(Window *window);

			bool						m_vsync_enabled;
			int							m_videoCardMemory;
			char						m_videoCardDescription[128];

			/// Map of windows
			WindowMap					windows;
			/// Map of displays
			OutputMap					outputs;

			/// Number of backbuffers
			static const UINT			FrameCount = 3;

			/// The D3D device 
			ID3D12Device*				m_d3d12Device;
			/// The command queue
			ID3D12CommandQueue*			m_commandQueue;		
			/// A command list used to record commands
			ID3D12GraphicsCommandList*	m_commandList;

			/// The current frame index 
			UINT						m_frameIndex;
			/// Event used to synchronize
			HANDLE						m_fenceEvent;
			/// A d3d fence object
			ID3D12Fence*				m_fence;
			/// Storage for the values of the fence
			UINT64						m_fenceValues[FrameCount];
		};
	}
}
#pragma warning(pop)
