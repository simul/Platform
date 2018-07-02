#include "DisplaySurface.h"

using namespace simul;
using namespace crossplatform;

DisplaySurface::DisplaySurface():
    renderer(nullptr),
    renderPlatform(nullptr),
    mViewId(-1),
    mHwnd(0),
    mIsVSYNC(false)
{
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

void DisplaySurface::ResizeSwapChain(DeviceContext &deviceContext)
{
}

void DisplaySurface::SetRenderer(crossplatform::PlatformRendererInterface *ci,int vw_id)
{
	if(renderer==ci)
		return;
	if(renderer)
		renderer->RemoveView(mViewId);
    mViewId=vw_id;
	renderer	=ci;
	if(mViewId<0)
        mViewId =renderer->AddView();
	renderer->ResizeView(mViewId,Viewport.w,Viewport.h);
}

void DisplaySurface::Release()
{
	if(renderer)
		renderer->RemoveView(mViewId);
}
