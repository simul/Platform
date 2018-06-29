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

		//! Direct3D11Manager corresponds to a single ID3D11Device, which it creates when initialized (i.e. a single graphics card accessed with this interface).
		class SIMUL_DIRECTX11_EXPORT Direct3D11Manager: public crossplatform::GraphicsDeviceInterface
		{
		public:
			Direct3D11Manager();
			~Direct3D11Manager();
			void Initialize(bool use_debug=false,bool instrument=false,bool default_driver=false);
			void Shutdown();
			void* GetDevice();
			void* GetDeviceContext();
			int GetNumOutputs();
			crossplatform::Output GetOutput(int i);
			void GetVideoCardInfo(char*, int&);
			void ReportMessageFilterState();
		protected:
			bool m_vsync_enabled;
			int m_videoCardMemory;
			char m_videoCardDescription[128];
			ID3D11Device* d3dDevice;
			ID3D11DeviceContext* d3dDeviceContext;
			// This represents the graphics card. We support one card only.
			IDXGIAdapter* adapter;
			typedef std::map<int,IDXGIOutput*> OutputMap;
			OutputMap outputs;

			ID3D11Debug *d3dDebug;
			ID3D11InfoQueue *d3dInfoQueue;
			IDXGIFactory* factory;
		};
		
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
			cp_hwnd hwnd;
			/// The id assigned by the renderer to correspond to this hwnd
			int view_id;			
			bool vsync;
			IDXGISwapChain				*m_swapChain;
			ID3D11RenderTargetView		*m_renderTargetView;
			ID3D11Texture2D				*m_renderTexture;
			ID3D11RasterizerState		*m_rasterState;
			D3D11_VIEWPORT				viewport;
			crossplatform::PlatformRendererInterface *renderer;
		};
		//! A class for multiple swap chains (i.e. rendering windows) to share the same d3d device.
		//! With each graphics window it manages (identified by HWND's), Direct3D11WindowManager creates and manages a IDXGISwapChain instance.
		class SIMUL_DIRECTX11_EXPORT Direct3D11WindowManager: public crossplatform::WindowManagerInterface
		{
		public:
			Direct3D11WindowManager();
			~Direct3D11WindowManager();
			void Initialize(void* d3dDevice,void *imm);
			void Shutdown();
			// Implementing Window Manager, which associates Hwnd's with renderers and view ids:
			//! Add a window. Creates a new Swap Chain.
			void AddWindow(cp_hwnd h);
			//! Removes the window and destroys its associated Swap Chain.
			void RemoveWindow(cp_hwnd h);
			IDXGISwapChain *GetSwapChain(cp_hwnd hwnd);
			void Render(cp_hwnd hwnd);
			void SetRenderer(cp_hwnd hwnd,crossplatform::PlatformRendererInterface *ci,int view_id);
			void SetFullScreen(cp_hwnd hwnd,bool fullscreen,int which_output);
			void ResizeSwapChain(cp_hwnd hwnd);
			int GetViewId(cp_hwnd hwnd);
			Window *GetWindow(cp_hwnd hwnd);
		protected:
			ID3D11Device* d3dDevice;
			ID3D11DeviceContext* d3dImmediateDeviceContext;
			typedef std::map<cp_hwnd,Window*> WindowMap;
			WindowMap windows;
		};
	}
}
#pragma warning(pop)
