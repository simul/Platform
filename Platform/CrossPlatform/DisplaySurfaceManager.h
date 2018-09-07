#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
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
		class DisplaySurface;
		//! A class for multiple swap chains (i.e. rendering windows) to share the same device.
		//! With each graphics window it manages (identified by HWND's), WindowManager creates and manages a SwapChain instance.
		class SIMUL_CROSSPLATFORM_EXPORT DisplaySurfaceManager: public crossplatform::DisplaySurfaceManagerInterface
		{
		public:
            DisplaySurfaceManager();
			~DisplaySurfaceManager();
			void Initialize(RenderPlatform *r);
			void Shutdown();
			// Implementing Window Manager, which associates Hwnd's with renderers and view ids:
			//! Add a window. Creates a new Swap Chain.
			void AddWindow(cp_hwnd h);
			//! Removes the window and destroys its associated Swap Chain.
			void RemoveWindow(cp_hwnd h);
			void Render(cp_hwnd hwnd);
			void SetRenderer(cp_hwnd hwnd,crossplatform::PlatformRendererInterface *ci,int view_id);
			void SetFullScreen(cp_hwnd hwnd,bool fullscreen,int which_output);
			void ResizeSwapChain(cp_hwnd hwnd);
			int GetViewId(cp_hwnd hwnd);
            DisplaySurface *GetWindow(cp_hwnd hwnd);

			///
			void EndFrame();
		protected:
            static const PixelFormat                    kDisplayFormat = RGBA_8_UNORM;
			RenderPlatform*                             renderPlatform;
			typedef std::map<cp_hwnd, DisplaySurface*>  DisplaySurfaceMap;
            DisplaySurfaceMap                           surfaces;
			bool frame_started;
		};
	}
}
#ifdef _MSC_VER
    #pragma warning(pop)  
#endif