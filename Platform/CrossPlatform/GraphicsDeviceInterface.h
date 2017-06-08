#ifndef GRAPHICSDEVICEINTERFACE
#define GRAPHICSDEVICEINTERFACE
#include <string>

struct Output
{
	std::string monitorName;
	int desktopX;
	int desktopY;
	int width;
	int height;
	int numerator,denominator;
};
typedef HWND__ *HWND;
namespace simul
{
	namespace crossplatform
	{
		/// This represents a
		///
		class PlatformRendererInterface
		{
		public:
			virtual void				OnCreateDevice(void* pd3dDevice)=0;
			//! Add a view. This tells the renderer to create any internal stuff it needs to handle a viewport, so that it is ready when Render() is called. It returns an identifier for that view.
			virtual int					AddView(bool external_fb)=0;
			virtual void				RemoveView(int)=0;
			//! For a view that has already been created, this ensures that it has the requested size and format.
			virtual void				ResizeView(int view_id,int w,int h)=0;
			//! Render the specified view. It's up to the renderer to decide what that means, and it's assumed that the render target and default depth buffer are already activated.
			//! If a depth buffer is passed, 
			virtual void				Render(int view_id,void* device,void* pContext)=0;
		};
		/// An interface class for managing GPU-accelerated graphics windows.
		/// The derived class 
		class GraphicsDeviceInterface
		{
		public:
			virtual void						AddWindow(HWND h)=0;
			virtual void						RemoveWindow(HWND h)=0;
			virtual void						Render(HWND h)=0;
			virtual void						SetRenderer(HWND,PlatformRendererInterface *ci,int view_id)=0;
			virtual void						SetFullScreen(HWND hwnd,bool fullscreen,int which_output)=0;
			virtual void						ResizeSwapChain(HWND hwnd)=0;
			virtual struct ID3D11Device*		GetDevice()=0;
			virtual struct ID3D11DeviceContext*	GetDeviceContext()=0;
			virtual int							GetNumOutputs()=0;
			virtual Output						GetOutput(int i)=0;
			virtual int							GetViewId(HWND h)=0;
		};
	}
}
#endif