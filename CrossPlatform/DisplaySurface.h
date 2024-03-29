#pragma once
#include "Platform/Core/ReadWriteMutex.h"
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace platform
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT DisplaySurface
		{
		public:
			DisplaySurface(int view_id);
			virtual ~DisplaySurface();
			//! Platform-dependent function called when initializing the display surface.
			virtual void RestoreDeviceObjects(cp_hwnd handle, RenderPlatform *renderPlatform, bool m_vsync_enabled, PixelFormat outFmt);
			//! Platform-dependent function called when uninitializing the display surface.
			virtual void InvalidateDeviceObjects() {}
			void Release();
			void SetRenderer(RenderDelegatorInterface *ci);
			void ResizeSwapChain(DeviceContext &deviceContext);
			virtual void Render(platform::core::ReadWriteMutex *delegatorReadWriteMutex, long long frameNumber){};
			virtual void StartFrame() {}
			virtual void EndFrame() {}
			bool IsSwapChainIsGammaEncoded() const
			{
				return swapChainIsGammaEncoded;
			}
			virtual void *GetPlatformDeviceContext()
			{
				return nullptr;
			}
			cp_hwnd GetHandle() { return mHwnd; }
			int GetViewId() { return mViewId; }

			Viewport viewport;
			RenderDelegatorInterface *renderer;

		protected:
			bool				swapChainIsGammaEncoded = false;
			RenderPlatform*		renderPlatform;
			//! The id assigned by the renderer to correspond to this hwnd
			int					mViewId;
			cp_hwnd				mHwnd;
			bool				mIsVSYNC;
			int4				lastWindow = {0, 0, 0, 0};
		};
	}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif