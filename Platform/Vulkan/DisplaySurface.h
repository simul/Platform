#pragma once

#include "Export.h"
#include "Simul/Platform/CrossPlatform/DisplaySurface.h"

typedef struct GLFWwindow GLFWwindow;
namespace simul
{
    namespace vulkan
    {
        SIMUL_VULKAN_EXPORT_CLASS DisplaySurface : public crossplatform::DisplaySurface
        {
        public:
            DisplaySurface();
            ~DisplaySurface();
            void RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)override;
            void InvalidateDeviceObjects() override;
            void Render();
			void EndFrame() override;
        private:
            //! Will resize the swap chain only if needed
            void Resize();
            //! SwapChain used to present images
           // IDXGISwapChain*             mSwapChain;
            //! The back buffer render target
           // ID3D11RenderTargetView*     mBackBufferRT;
            //! Back Buffer texture
            //ID3D11Texture2D*            mBackBuffer;
            //! A reference of the device create by the manager
            //ID3D11Device*               mDeviceRef;
            //! Rendering viewport
            //D3D11_VIEWPORT				mViewport;
			crossplatform::DeviceContext deferredContext;
			//ID3D11DeviceContext*	mDeferredContext;
			//ID3D11CommandList *mCommandList;
			crossplatform::PixelFormat pixelFormat;
			//GLFWwindow* mWindow;
#ifdef _MSC_VER
			HDC             hDC;
			HGLRC           hRC;
#endif
			void InitSwapChain();
        };
    }
}
