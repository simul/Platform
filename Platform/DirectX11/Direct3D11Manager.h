#pragma once
#include "SimulDirectXHeader.h"
#include <map>
#include <string>
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/DirectX11/Export.h"
struct ID3D11Debug;
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11
	{
		struct SIMUL_DIRECTX11_EXPORT Window
		{
			Window();
			~Window();
			void RestoreDeviceObjects(ID3D11Device* d3dDevice,bool m_vsync_enabled,int numerator,int denominator);
			void Release();
			void CreateRenderTarget(ID3D11Device* d3dDevice);
			void CreateDepthBuffer(ID3D11Device* d3dDevice);
			void SetRenderer(crossplatform::PlatformRendererInterface *ci, int view_id);
			void ResizeSwapChain(ID3D11Device* d3dDevice);
			HWND hwnd;
			/// The id assigned by the renderer to correspond to this hwnd
			int view_id;			
			bool vsync;
			IDXGISwapChain				*m_swapChain;
			ID3D11RenderTargetView		*m_renderTargetView;
			ID3D11Texture2D				*m_renderTexture;
			//ID3D11DepthStencilState		*m_depthStencilState;
			//ID3D11DepthStencilView		*m_depthStencilView;
			ID3D11RasterizerState		*m_rasterState;
			D3D11_VIEWPORT				viewport;
			crossplatform::PlatformRendererInterface *renderer;
		};
		//! A class intended to replace DXUT, while allowing for multiple swap chains (i.e. rendering windows) to share the same d3d device.

		//! Direct3D11Manager corresponds to a single ID3D11Device, which it creates when initialized (i.e. a single graphics card accessed with this interface).
		//! With each graphics window it manages (identified by HWND's), Direct3D11Manager creates and manages a IDXGISwapChain instance.
		class SIMUL_DIRECTX11_EXPORT Direct3D11Manager: public crossplatform::GraphicsDeviceInterface
		{
		public:
			Direct3D11Manager();
			~Direct3D11Manager();
			void Initialize(bool use_debug=false,bool instrument=false);
			//! Add a window. Creates a new Swap Chain.
			void AddWindow(HWND h);
			//! Removes the window and destroys its associated Swap Chain.
			void RemoveWindow(HWND h);
			void Shutdown();
			IDXGISwapChain *GetSwapChain(HWND hwnd);
			void Render(HWND hwnd);
			void SetRenderer(HWND hwnd,crossplatform::PlatformRendererInterface *ci,int view_id);
			void SetFullScreen(HWND hwnd,bool fullscreen,int which_output);
			void ResizeSwapChain(HWND hwnd);
			void* GetDevice();
			void* GetDeviceContext();
			int GetNumOutputs();
			crossplatform::Output GetOutput(int i);

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
		};

	}
}
#pragma warning(pop)
