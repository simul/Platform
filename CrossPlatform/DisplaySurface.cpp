#include "DisplaySurface.h"
#include "RenderDelegater.h"
using namespace platform;
using namespace crossplatform;

DisplaySurface::DisplaySurface(int view_id):
    renderer(nullptr),
    renderPlatform(nullptr),
    mViewId(view_id),
    mHwnd(0),
    mIsVSYNC(false)
{
	memset(&viewport,0,sizeof(viewport));
}

DisplaySurface::~DisplaySurface()
{
	Release();
}

void DisplaySurface::RestoreDeviceObjects(cp_hwnd handle, RenderPlatform* r,bool m_vsync_enabled,int numerator,int denominator, PixelFormat outFmt)
{
    mHwnd=handle;
    renderPlatform=r;
    mIsVSYNC=m_vsync_enabled;
}

void DisplaySurface::ResizeSwapChain(DeviceContext &)
{
	if(renderer)
		renderer->ResizeView(mViewId,viewport.w,viewport.h);
}

void DisplaySurface::SetRenderer(crossplatform::RenderDelegaterInterface *ci)
{
	renderer	=ci;
}

void DisplaySurface::Release()
{
	if(renderer)
		renderer->RemoveView(mViewId);
}
