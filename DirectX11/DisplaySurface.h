#pragma once

#include "Export.h"
#include "Platform/Core/ReadWriteMutex.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#include "DirectXHeader.h"
#include "MacrosDx1x.h"

namespace platform
{
    namespace dx11
    {
        class SIMUL_DIRECTX11_EXPORT DisplaySurface: public crossplatform::DisplaySurface
        {
        public:
            DisplaySurface(int view_id);
            ~DisplaySurface();
			//! Platform-dependent function called when initializing the display surface.
            void RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)override;
			//! Platform-dependent function called when uninitializing the display surface.
			void InvalidateDeviceObjects() override;
			//! Render to the display surface. Requires a reference to the mutex to make sure that this rendering doesn't take place at the same time as other render calls.
            void Render(platform::core::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber) override;
			void EndFrame() override;
            virtual void* GetPlatformDeviceContext()
            {
                return mDeferredContext;
            }
        private:
            //! Will resize the swap chain only if needed
            void Resize();
            //! SwapChain used to present images
            IDXGISwapChain*             mSwapChain;
            //! The back buffer render target
            ID3D11RenderTargetView*     mBackBufferRT;
            //! Back Buffer texture
            ID3D11Texture2D*            mBackBuffer;
            //! A reference of the device create by the manager
            ID3D11Device*               mDeviceRef;
            //! Rendering viewport
            D3D11_VIEWPORT				mViewport;
			crossplatform::DeviceContext deferredContext;
			ID3D11DeviceContext*	mDeferredContext;
			ID3D11CommandList *mCommandList;
			crossplatform::PixelFormat pixelFormat;
			void InitSwapChain();
        };
    }
}
