#pragma once
#include "SimulDirectXHeader.h"
#include <map>
#include <string>
#include "Simul/Platform/DirectX11/Direct3D11CallbackInterface.h"
#include "Simul/Platform/DirectX11/Direct3D11ManagerInterface.h"
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
			void SetRenderer(Direct3D11CallbackInterface *ci);
			HWND hwnd;
			/// The id assigned by the renderer to correspond to this hwnd
			int view_id;			
			bool vsync;
			IDXGISwapChain				*m_swapChain;
			ID3D11RenderTargetView		*m_renderTargetView;
			ID3D11Texture2D				*m_depthStencilBuffer;
			ID3D11DepthStencilState		*m_depthStencilState;
			ID3D11DepthStencilView		*m_depthStencilView;
			ID3D11RasterizerState		*m_rasterState;
			D3D11_VIEWPORT				viewport;
			Direct3D11CallbackInterface *renderer;
		};
		//! A class intended to replace DXUT, while allowing for multiple swap chains (i.e. rendering windows) to share the same d3d device.

		//! Direct3D11Manager corresponds to a single ID3D11Device (i.e. a single graphics card accessed with this interface).
		//! With each graphics window it manages (identified by HWND's), Direct3D11Manager
		class SIMUL_DIRECTX11_EXPORT Direct3D11Manager: public Direct3D11ManagerInterface
		{
		public:
			Direct3D11Manager();
			~Direct3D11Manager();
			void Initialize();
			//! Add a window. Creates a new Swap Chain.
			void AddWindow(HWND h);
			//! Removes the window and destroys its associated Swap Chain.
			void RemoveWindow(HWND h);
			void Shutdown();
			IDXGISwapChain *GetSwapChain(HWND hwnd);
			void StartRendering(HWND hwnd);
			void SetRenderer(HWND hwnd,Direct3D11CallbackInterface *ci);
			void SetFullScreen(HWND hwnd,bool fullscreen,int which_output);
			void ResizeSwapChain(HWND hwnd,int width,int height);
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
		};

	}
}
#pragma warning(pop)
