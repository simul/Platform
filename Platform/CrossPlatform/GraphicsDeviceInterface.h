#ifndef GRAPHICSDEVICEINTERFACE
#define GRAPHICSDEVICEINTERFACE
#include <string>


#ifdef DOXYGEN
typedef void *HWND;
#else
typedef HWND__ *HWND;
#endif

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
			virtual void				OnCreateDevice(void* pd3dDevice)=0;
			//! Add a view. This tells the renderer to create any internal stuff it needs to handle a viewport, so that it is ready when Render() is called. It returns an identifier for that view.
			virtual int					AddView()=0;
			virtual void				RemoveView(int)=0;
			//! For a view that has already been created, this ensures that it has the requested size and format.
			virtual void				ResizeView(int view_id,int w,int h)=0;
			//! Render the specified view. It's up to the renderer to decide what that means. The renderTexture is required because many API's don't allow querying of the current state.
			//! It will be assumed for simplicity that the viewport should be restored to the entire size or the renderTexture.
			virtual void				Render(int view_id,void* pContext,void* renderTexture)=0;
		};
		/// An interface class for managing GPU-accelerated graphics windows.
		/// The derived class 
		class GraphicsDeviceInterface
		{
		public:
			virtual void	AddWindow(HWND h)=0;
			virtual void	RemoveWindow(HWND h)=0;
			virtual void	Render(HWND h)=0;
			virtual void	SetRenderer(HWND,PlatformRendererInterface *ci,int view_id)=0;
			virtual void	SetFullScreen(HWND hwnd,bool fullscreen,int which_output)=0;
			virtual void	ResizeSwapChain(HWND hwnd)=0;
			virtual void*	GetDevice()=0;
			virtual void*	GetDeviceContext()=0;
			virtual int		GetNumOutputs()=0;
			virtual Output	GetOutput(int i)=0;
			virtual int		GetViewId(HWND h)=0;
		};
	}
}
#endif