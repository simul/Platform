#ifndef DIRECT3D11MANAGERINTERFACE
#define DIRECT3D11MANAGERINTERFACE
#include <string>
struct Output
{
	std::string monitorName;
	int desktopX;
	int desktopY;
	int width;
	int height;
};

struct Direct3DWindow
{
	int view_id;			
};

class Direct3D11ManagerInterface
{
public:
	virtual void						AddWindow(HWND h)=0;
	virtual void						RemoveWindow(HWND h)=0;
	virtual struct IDXGISwapChain *		GetSwapChain(HWND h)=0;
	virtual void						StartRendering(HWND h)=0;
	virtual void						SetRenderer(HWND,Direct3D11CallbackInterface *ci)=0;
	virtual void						SetFullScreen(HWND hwnd,bool fullscreen,int which_output)=0;
	virtual void						ResizeSwapChain(HWND hwnd,int width,int height)=0;
	virtual struct ID3D11Device*		GetDevice()=0;
	virtual struct ID3D11DeviceContext*	GetDeviceContext()=0;
	virtual int							GetNumOutputs()=0;
	virtual Output						GetOutput(int i)=0;
	virtual Direct3DWindow*				GetWindow(HWND h)=0;
};
#endif