#include "WindowManager.h"
#include "Window.h"
#include "SwapChain.h"

using namespace simul;
using namespace crossplatform;


void WindowManager::RemoveWindow(cp_hwnd hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	SetFullScreen(hwnd,false,0);
	delete w;
	windows.erase(hwnd);
}

SwapChain *WindowManager::GetSwapChain(cp_hwnd h)
{
	if(windows.find(h)==windows.end())
		return NULL;
	Window *w=windows[h];
	if(!w)
		return NULL;
	return w->m_swapChain;
}

void WindowManager::Render(cp_hwnd h)
{
	if(windows.find(h)==windows.end())
		return;
	Window *w=windows[h];
	if(!w)
	{
		SIMUL_CERR<<"No window exists for cp_hwnd "<<std::hex<<h<<std::endl;
		return;
	}
	if(h!=w->hwnd)
	{
		SIMUL_CERR<<"Window for cp_hwnd "<<std::hex<<h<<" has hwnd "<<w->hwnd<<std::endl;
		return;
	}
	ResizeSwapChain(w->hwnd);
	if(!w->m_texture)
	{
		SIMUL_CERR<<"No renderTarget exists for cp_hwnd "<<std::hex<<h<<std::endl;
		return;
	}
	DeviceContext &deviceContext =renderPlatform->GetImmediateContext();
	w->m_texture->activateRenderTarget(deviceContext);
	// Create the viewport.
	renderPlatform->SetViewports(deviceContext,1, &w->viewport);
	// Now set the rasterizer state.
	renderPlatform->SetRenderState(deviceContext,w->m_rasterState);
	if(w->renderer)
	{
		w->renderer->Render(w->view_id,deviceContext.asD3D11DeviceContext(), w->m_texture->AsD3D11RenderTargetView(),(int)w->viewport.w,(int)w->viewport.h);
	}
	w->m_texture->deactivateRenderTarget(deviceContext);
	static DWORD dwFlags = 0;
	// 0 - don't wait for 60Hz refresh.
	static UINT SyncInterval = 0;
    // Show the frame on the primary surface.
	// TODO: what if the device is lost?
	renderPlatform->PresentSwapChain(deviceContext,w->m_swapChain);
}

WindowManager::WindowManager()
{
}

void WindowManager::Initialize(RenderPlatform *r)
{
	renderPlatform=r;
}

WindowManager::~WindowManager()
{
	Shutdown();
}

void WindowManager::Shutdown()
{
	for(WindowMap::iterator i=windows.begin();i!=windows.end();i++)
	{
		SetFullScreen(i->second->hwnd,false,0);
		delete i->second;
	}
	windows.clear();
}

void WindowManager::SetRenderer(cp_hwnd hwnd,crossplatform::PlatformRendererInterface *ci, int view_id)
{
	AddWindow(hwnd);
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	if(!w)
		return;
	w->SetRenderer(ci,  view_id);
}

int WindowManager::GetViewId(cp_hwnd hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return -1;
	Window *w=windows[hwnd];
	return w->view_id;
}

Window *WindowManager::GetWindow(cp_hwnd hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return NULL;
	Window *w=windows[hwnd];
	return w;
}

void WindowManager::SetFullScreen(cp_hwnd hwnd,bool fullscreen,int which_output)
{
	Window *w=(Window*)GetWindow(hwnd);
	if(!w)
		return;
	if(!w->m_swapChain)
		return;
	bool current_fullscreen=w->m_swapChain->IsFullscreen();
	if((current_fullscreen)==fullscreen)
		return;
	w->m_swapChain->SetFullscreen(fullscreen);
	ResizeSwapChain(hwnd);
}

void WindowManager::ResizeSwapChain(cp_hwnd hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	if(!w)
		return;
	DeviceContext &deviceContext =renderPlatform->GetImmediateContext();
	w->ResizeSwapChain(deviceContext);
}

void WindowManager::AddWindow(cp_hwnd hwnd)
{
	if(windows.find(hwnd)!=windows.end())
		return;
	Window *window=new Window;
	windows[hwnd]=window;
	window->hwnd=hwnd;
	//crossplatform::Output o=GetOutput(0);
	window->RestoreDeviceObjects(renderPlatform,false,0,1);//o.numerator,o.denominator);
}