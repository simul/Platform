#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif

namespace simul
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT DisplaySurface
		{
        public:
                            DisplaySurface();
			virtual         ~DisplaySurface();
			virtual void    RestoreDeviceObjects(cp_hwnd handle, RenderPlatform* renderPlatform,bool m_vsync_enabled,int numerator,int denominator, PixelFormat outFmt);
            virtual void    InvalidateDeviceObjects() {}
			void            Release();
			void            SetRenderer(PlatformRendererInterface *ci, int view_id);
			void            ResizeSwapChain(DeviceContext &deviceContext);
            virtual void    Render() {};
			virtual void	EndFrame() {}
            cp_hwnd         GetHandle() { return mHwnd; }
            int             GetViewId() { return mViewId; }

			Viewport		           viewport;
			PlatformRendererInterface*  renderer;

		protected:
			RenderPlatform*             renderPlatform;
			//! The id assigned by the renderer to correspond to this hwnd
			int                         mViewId;	
            cp_hwnd                     mHwnd;
			bool                        mIsVSYNC;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif