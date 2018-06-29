#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/SwapChain.h"
#include "SimulDirectXHeader.h"
#include <string>

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif

namespace simul
{
	namespace dx11
	{
		class  SwapChain:public crossplatform::SwapChain
		{
		protected:
			IDXGISwapChain                       *pSwapChain;
			void Resize();
		public:
			SwapChain();
			virtual ~SwapChain();
			virtual void InvalidateDeviceObjects();
			/// Set the size and format
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r,crossplatform::DeviceContext &deviceContext,cp_hwnd h ,crossplatform::PixelFormat,int count);
			bool IsFullscreen() const;
			void SetFullscreen(bool);
			IDXGISwapChain *AsDXGISwapChain();
		};

	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif