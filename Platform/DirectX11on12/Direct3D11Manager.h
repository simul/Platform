#pragma once
#include "SimulDirectXHeader.h"
#include <map>
#include <string>
#include "Simul/Platform/DirectX11on12/Direct3D11CallbackInterface.h"
#include "Simul/Platform/DirectX11on12/Direct3D11ManagerInterface.h"
#include "Simul/Platform/DirectX11on12/Export.h"

//cpo make sure this is the right place
#include <d3d12.h>
//cpo windows helpers ???? 
#include "D3dx12.h"
#include <d3d11on12.h>
#include <dxgi.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>

#include <DirectXMath.h>				//cpo only for vertex structure

struct Vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;
};


//cpo this define lifted from dx12 demo referenced system header
#define IID_PPV_ARGS(ppType) __uuidof(**(ppType)), IID_PPV_ARGS_Helper(ppType)


struct ID3D11Debug;
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11on12
	{
		struct SIMUL_DIRECTX11_EXPORT Window
		{
			Window();
			~Window();
			//cpovoid RestoreDeviceObjects(ID3D11Device* d3dDevice, bool m_vsync_enabled, int numerator, int denominator);
			void RestoreDeviceObjects(ID3D12Device* d3dDevice, bool m_vsync_enabled, int numerator, int denominator);
			void Release();
			void CreateRenderTarget(ID3D12Device* d3dDevice);
			void CreateDepthBuffer(ID3D12Device* d3dDevice);

			void CreateDepthBuffer(ID3D11Device* d3dDevice);


			void SetRenderer(Direct3D11CallbackInterface *ci, int view_id);
			void ResizeSwapChain(ID3D12Device* d3dDevice);
			HWND hwnd;
			/// The id assigned by the renderer to correspond to this hwnd
			int view_id;			
			bool vsync;
			IDXGISwapChain3				*m_swapChain;
			ID3D11RenderTargetView		*m_renderTargetView[3];											//cpo FrameCount
			//cpo does not exist ID3D12RenderTargetView		*m_renderTargetView;
			ID3D11Texture2D				*m_depthStencilTexture;
			ID3D12Resource				*m_depthStencilTexture12;

			ID3D11DepthStencilState		*m_depthStencilState;
			ID3D11DepthStencilView		*m_depthStencilView;
			ID3D11RasterizerState		*m_rasterState;
			D3D11_VIEWPORT				viewport;
			Direct3D11CallbackInterface *renderer;
			//cpo extensions for dx12
			ID3D12Resource* backBufferPtr[3];											//cpo FrameCount

			static const UINT FrameCount = 3;
			void setCommandQueue(ID3D12CommandQueue *commandQueue);
			void setRootSignature(ID3D12RootSignature *rootSignature);
			void setD3D11Device(ID3D11Device* d3dDevice);

			// duplicate ID3D12CommandAllocator *m_commandAllocators[FrameCount];



			UINT m_frameIndex;

			void preFrame(UINT frameIndex);
			void postFrame(UINT frameIndex);
			//void setD3D12Device(ID3D12Device* d3dDevice);


			ID3D11Device* d3d11Device;
			ID3D11On12Device* d3d11on12Device;
			ID3D12Device* d3d12Device;
			ID3D12CommandQueue *m_dx12CommandQueue;

			ID3D11Resource *m_wrappedBackBuffer[FrameCount];		//cpo //todo fix number to frame count // d3d 11 side thing
			ID3D11Resource *m_WrappedDepthStencilTexture;

			ID3D12DescriptorHeap *m_rtvHeap;			// replaces rendertargetview
			UINT m_rtvDescriptorSize;

														// Synchronization objects.
			ID3D12CommandAllocator *m_commandAllocators[FrameCount];					//cpo //todo fix number of command allocators here



			//pipeline


			//duplicated but needed
			ID3D12RootSignature *m_rootSignature;							//cpo 



			CD3DX12_VIEWPORT m_viewport;
			CD3DX12_RECT m_scissorRect;


		};
		//! A class intended to replace DXUT, while allowing for multiple swap chains (i.e. rendering windows) to share the same d3d device.

		//! Direct3D11Manager corresponds to a single ID3D11Device, which it creates when initialized (i.e. a single graphics card accessed with this interface).
		//! With each graphics window it manages (identified by HWND's), Direct3D11Manager creates and manages a IDXGISwapChain instance.
		class SIMUL_DIRECTX11_EXPORT Direct3D11Manager: public Direct3D11ManagerInterface
		{
		public:
			Direct3D11Manager();
			~Direct3D11Manager();

			HRESULT WINAPI createDx11on12Device(
				_In_opt_ IDXGIAdapter* pAdapter,
				D3D_DRIVER_TYPE DriverType,
				HMODULE Software,
				UINT Flags,
				_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
				UINT FeatureLevels,
				UINT SDKVersion,
				_Out_opt_ ID3D11Device** ppDevice,
				_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
				_Out_opt_ ID3D11DeviceContext** ppImmediateContext);


			void Initialize(bool use_debug=false,bool instrument=false);
			//! Add a window. Creates a new Swap Chain.
			void AddWindow(HWND h);
			//! Removes the window and destroys its associated Swap Chain.
			void RemoveWindow(HWND h);
			void Shutdown();
			IDXGISwapChain *GetSwapChain(HWND hwnd);
			void Render(HWND hwnd);
			
			void SetRenderer(HWND hwnd,Direct3D11CallbackInterface *ci,int view_id);
			void SetFullScreen(HWND hwnd,bool fullscreen,int which_output);
			void ResizeSwapChain(HWND hwnd);
			ID3D11Device* GetDevice();
			ID3D11DeviceContext* GetDeviceContext();
			int GetNumOutputs();
			Output GetOutput(int i);

			void GetVideoCardInfo(char*, int&);
			int GetViewId(HWND hwnd);
			Window *GetWindow(HWND hwnd);
			void ReportMessageFilterState();
		protected:
			bool m_vsync_enabled;
			int m_videoCardMemory;
			char m_videoCardDescription[128];
			ID3D11Device* d3dDevice;
			ID3D11DeviceContext* d3dDeviceContext;
			// This represents the graphics card. We support one card only.
			IDXGIAdapter* adapter;
			typedef std::map<HWND,Window*> WindowMap;
			WindowMap windows;
			typedef std::map<int,IDXGIOutput*> OutputMap;
			OutputMap outputs;

			ID3D11Debug *d3dDebug ;
			ID3D11InfoQueue *d3dInfoQueue ;
			IDXGIFactory* factory;
			//cpo dx12 specfic member variables 
			static const UINT FrameCount = 3;

			ID3D12Device *m_d3d12Device;

			ID3D12CommandQueue *m_commandQueue;
			ID3D12RootSignature *m_rootSignature;							//cpo 


			//cpo extra to test d3d12

			// App resources.
			ID3D12Resource *m_vertexBuffer;
			D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

			//cpo following need more set up
			ID3D12GraphicsCommandList *m_commandList;
			ID3D12PipelineState *m_pipelineState;


			UINT m_frameIndex;
			HANDLE m_fenceEvent;
			ID3D12Fence *m_fence;
			UINT64 m_fenceValues[FrameCount];

			float m_aspectRatio = 1.0;


			void LoadAssets(Window *window);
			void WaitForGpu();
			void MoveToNextFrame(Window *window);
			void PopulateCommandList(Window *window);


			//cpo just debug 
			UINT m_totalFrames = 0;

		};

	}
}
#pragma warning(pop)
