#ifndef GRAPHICSDEVICEINTERFACE
#define GRAPHICSDEVICEINTERFACE
#include <string>

#ifdef _XBOX_ONE
#ifdef DECLARE_HANDLE
typedef HWND cp_hwnd;
#else
typedef void* cp_hwnd;
#endif
#elif !defined(DOXYGEN) && defined(_MSC_VER)
#include <Windows.h>
#define cp_hwnd HWND
#else
typedef void* cp_hwnd;
#endif

#include "DeviceContext.h"

namespace simul
{
	namespace crossplatform
	{
		struct Output
		{
			std::string monitorName;
			int desktopX;
			int desktopY;
			int width;
			int height;
			int numerator, denominator;
		};
		/// This represents an interface that faces the raw API.
		/// The implementing class should keep a list of integer view id's
		class PlatformRendererInterface
		{
		public:
			//! Add a view. This tells the renderer to create any internal stuff it needs to handle a viewport, so that it is ready when Render() is called. It returns an identifier for that view.
			virtual int					AddView()=0;
			//! Remove the view. This might not have an immediate effect internally, but is a courtesy to the interface.
			virtual void				RemoveView(int)=0;
			//! For a view that has already been created, this ensures that it has the requested size and format.
			virtual void				ResizeView(int view_id,int w,int h)=0;
			//! Render the specified view. It's up to the renderer to decide what that means. The renderTexture is required because many API's don't allow querying of the current state.
			//! It will be assumed for simplicity that the viewport should be restored to the entire size of the renderTexture.
			virtual void				Render(int view_id,void* pContext,void* renderTexture,int w,int h)=0;
			virtual void				SetRenderDelegate(int /*view_id*/,crossplatform::RenderDelegate /*d*/){}
		};
		/// An interface class for managing GPU-accelerated graphics windows.
		/// The derived class 
		class GraphicsDeviceInterface
		{
		public:
			virtual void	Initialize(bool use_debug, bool instrument, bool default_driver) = 0;
			virtual void	Shutdown() = 0;
			virtual void*	GetDevice()=0;
			virtual void*	GetDeviceContext()=0;
			virtual int		GetNumOutputs()=0;
			virtual Output	GetOutput(int i)=0;
		};
		
		class DisplaySurfaceManagerInterface
		{
		public:
			virtual void	AddWindow(cp_hwnd h,crossplatform::PixelFormat pfm=crossplatform::PixelFormat::UNKNOWN)=0;
			virtual void	RemoveWindow(cp_hwnd h)=0;
			virtual void	Render(cp_hwnd h)=0;
			virtual void	SetRenderer(cp_hwnd,PlatformRendererInterface *ci,int view_id)=0;
			virtual void	SetFullScreen(cp_hwnd h,bool fullscreen,int which_output)=0;
			virtual void	ResizeSwapChain(cp_hwnd h)=0;
			virtual int		GetViewId(cp_hwnd h)=0;
		};
	}
}
#endif