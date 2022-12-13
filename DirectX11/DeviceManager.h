#pragma once
#include "DirectXHeader.h"
#include <map>
#include <string>
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/DirectX11/Export.h"
struct ID3D11Debug;
#pragma warning(push)
#pragma warning(disable:4251)
namespace platform
{
	namespace dx11
	{

		//! DeviceManager corresponds to a single ID3D11Device, which it creates when initialized (i.e. a single graphics card accessed with this interface).
		class SIMUL_DIRECTX11_EXPORT DeviceManager: public crossplatform::GraphicsDeviceInterface
		{
		public:
			DeviceManager();
			~DeviceManager();
			void Initialize(bool use_debug=false,bool instrument=false,bool default_driver=false);
			bool IsActive() const;
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
			ID3D11InfoQueue *infoQueue;
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
			void SetRenderer(crossplatform::RenderDelegaterInterface *ci, int view_id);
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
			crossplatform::RenderDelegaterInterface *renderer;
		};
	}
}
#pragma warning(pop)
