#pragma once
#include <dxgi.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include "Simul/Platform/DirectX11/Direct3D11CallbackInterface.h"

//! A class intended to replace DXUT, while allowing for multiple swap chains (i.e. rendering windows) to share the same d3d device.
class Direct3D11Manager
{
public:
	Direct3D11Manager();
	~Direct3D11Manager();
protected:

	void Initialize();

	void AddWindow(HWND h,int id);
	void Shutdown();

	ID3D11Device* GetDevice();
	ID3D11DeviceContext* GetDeviceContext();

	void GetVideoCardInfo(char*, int&);
private:
	bool m_vsync_enabled;
	int m_videoCardMemory;
	char m_videoCardDescription[128];
	IDXGISwapChain* m_swapChain;
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_deviceContext;
	ID3D11RenderTargetView* m_renderTargetView;
	ID3D11Texture2D* m_depthStencilBuffer;
	ID3D11DepthStencilState* m_depthStencilState;
	ID3D11DepthStencilView* m_depthStencilView;
	ID3D11RasterizerState* m_rasterState;
};

