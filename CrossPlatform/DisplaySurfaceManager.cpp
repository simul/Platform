#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/DisplaySurface.h"

using namespace platform;
using namespace crossplatform;

void platform::crossplatform::DisplaySurfaceManager::RemoveWindow(cp_hwnd hwnd)
{
	if(surfaces.find(hwnd)==surfaces.end())
		return;
	toRender.erase(hwnd);
	DisplaySurface *w=surfaces[hwnd];
	SetFullScreen(hwnd,false,0);
	delete w;
	surfaces.erase(hwnd);
}

void DisplaySurfaceManager::Render(cp_hwnd h)
{
	toRender.insert(h);
}

void DisplaySurfaceManager::RenderAll(bool clear)
{
	for (cp_hwnd h : toRender)
	{
		if (surfaces.find(h) == surfaces.end())
			return;
		DisplaySurface *w = surfaces[h];
		if (!w)
		{
			SIMUL_CERR << "No window exists for cp_hwnd " << std::hex << h << std::endl;
			return;
		}
		if (h != w->GetHandle())
		{
			SIMUL_CERR << "Window for cp_hwnd " << std::hex << h << " has hwnd " << w->GetHandle() << std::endl;
			return;
		}
		if (renderPlatform && !renderPlatform->FrameStarted())
		{
			renderPlatform->BeginFrame();
		}
		w->StartFrame();
		w->Render(delegatorReadWriteMutex, renderPlatform->GetFrameNumber());
		if(delegatorReadWriteMutex->locked())
		{
			SIMUL_BREAK("Locked mutex");
		}
	}
	if(clear)
		toRender.clear();
}

DisplaySurfaceManager::DisplaySurfaceManager():
				renderPlatform(nullptr)
{
}

void DisplaySurfaceManager::Initialize(RenderPlatform *r)
{
	renderPlatform=r;
	if (!delegatorReadWriteMutex)
		delegatorReadWriteMutex = new platform::core::ReadWriteMutex;
}

DisplaySurfaceManager::~DisplaySurfaceManager()
{
	Shutdown();
}

void DisplaySurfaceManager::Shutdown()
{
	for(DisplaySurfaceMap::iterator i=surfaces.begin();i!=surfaces.end();i++)
	{
		SetFullScreen(i->second->GetHandle(),false,0);
		delete i->second;
	}
	surfaces.clear();
}

void DisplaySurfaceManager::SetRenderer(crossplatform::RenderDelegatorInterface *ci)
{
	renderDelegater=ci;
	for(auto s:surfaces)
	{
		s.second->SetRenderer(ci);
	}
}

int DisplaySurfaceManager::GetViewId(cp_hwnd hwnd)
{
	if(surfaces.find(hwnd)==surfaces.end())
		return -1;
	DisplaySurface *w=surfaces[hwnd];
	return w->GetViewId();
}

DisplaySurface *DisplaySurfaceManager::GetWindow(cp_hwnd hwnd)
{
	if(surfaces.find(hwnd)==surfaces.end())
		return NULL;
	DisplaySurface *w=surfaces[hwnd];
	return w;
}

void DisplaySurfaceManager::SetFullScreen(cp_hwnd hwnd,bool fullscreen,int which_output)
{
	DisplaySurface *w=(DisplaySurface*)GetWindow(hwnd);
	if(!w)
		return;
	// TO-DO!
}

void DisplaySurfaceManager::ResizeSwapChain(cp_hwnd hwnd)
{
	if(surfaces.find(hwnd)==surfaces.end())
		return;
	DisplaySurface *w=surfaces[hwnd];
	if(!w)
		return;
	DeviceContext &deviceContext =renderPlatform->GetImmediateContext();
	w->ResizeSwapChain(deviceContext);
}

void DisplaySurfaceManager::AddWindow(cp_hwnd hwnd,crossplatform::PixelFormat fmt,bool vsync)
{
	if(surfaces.find(hwnd)!=surfaces.end())
		return;
	if(fmt==crossplatform::UNKNOWN)
		fmt=kDisplayFormat;
	SIMUL_NULL_CHECK_RETURN(renderPlatform,"Can't add a window when renderPlatform has not been set.")
	DisplaySurface *window=nullptr;
	if(createSurfaceDelegate)
		window=createSurfaceDelegate(hwnd);
	else
		window=renderPlatform->CreateDisplaySurface();
	window->SetRenderer(renderDelegater);
	surfaces[hwnd]=window;
	window->RestoreDeviceObjects(hwnd,renderPlatform,vsync,fmt);
}

void DisplaySurfaceManager::EndFrame(bool clear)
{
	RenderAll(clear);
	ERRNO_BREAK
	for(auto s:surfaces)
	{
		s.second->EndFrame();
	}
	renderPlatform->EndFrame();
}
