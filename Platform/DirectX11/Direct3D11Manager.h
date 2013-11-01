#pragma once
#include <dxgi.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <map>
#include <string>
#include "Simul/Platform/DirectX11/Direct3D11CallbackInterface.h"
#include "Simul/Platform/DirectX11/Export.h"

#ifndef DIRECT3D11MANAGERINTERFACE
#define DIRECT3D11MANAGERINTERFACE
class Direct3D11ManagerInterface
{
public:
	virtual void					AddWindow(HWND h)=0;
	virtual void					RemoveWindow(HWND h)=0;
	virtual IDXGISwapChain *		GetSwapChain(HWND h)=0;
	virtual void					SetRenderer(HWND,Direct3D11CallbackInterface *ci)=0;
	virtual void					ResizeSwapChain(HWND hwnd,int width,int height)=0;
	virtual ID3D11Device*			GetDevice()=0;
	virtual ID3D11DeviceContext*	GetDeviceContext()=0;
};
#endif
namespace simul
{
	namespace dx11
	{
		struct SIMUL_DIRECTX11_EXPORT Window
		{
			Window();
			~Window();
			void RestoreDeviceObjects(ID3D11Device* d3dDevice);
			void Release();
			void CreateRenderTarget(ID3D11Device* d3dDevice);
			void CreateDepthBuffer(ID3D11Device* d3dDevice);
			void SetRenderer(Direct3D11CallbackInterface *ci);
			HWND hwnd;
			/// The id assigned by the renderer to correspond to this hwnd
			int view_id;			
			bool vsync;
			IDXGISwapChain*				m_swapChain;
			ID3D11RenderTargetView*		m_renderTargetView;
			ID3D11Texture2D*			m_depthStencilBuffer;
			ID3D11DepthStencilState*	m_depthStencilState;
			ID3D11DepthStencilView*		m_depthStencilView;
			ID3D11RasterizerState*		m_rasterState;
			D3D11_VIEWPORT				viewport;
			Direct3D11CallbackInterface *renderer;
		};
		struct Output
		{
			std::string monitorName;
			int desktopX;
			int desktopY;
			int width;
			int height;
		};
		//! A class intended to replace DXUT, while allowing for multiple swap chains (i.e. rendering windows) to share the same d3d device.
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
			
			IDXGISwapChain *GetSwapChain(HWND h);
			void SetRenderer(HWND h,Direct3D11CallbackInterface *ci);
			void ResizeSwapChain(HWND hw,int width,int height);
			ID3D11Device* GetDevice();
			ID3D11DeviceContext* GetDeviceContext();
			int GetNumOutputs();
			Output GetOutput(int i);

			void GetVideoCardInfo(char*, int&);
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
		};

	}
}
