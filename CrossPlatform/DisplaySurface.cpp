#include "DisplaySurface.h"

using namespace platform;
using namespace crossplatform;

DisplaySurface::DisplaySurface():
    renderer(nullptr),
    renderPlatform(nullptr),
    mViewId(-1),
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

void DisplaySurface::SetRenderer(crossplatform::PlatformRendererInterface *ci,int vw_id)
{
	if(renderer==ci)
		return;
	if(renderer)
		renderer->RemoveView(mViewId);
    mViewId		=vw_id;
	renderer	=ci;
	if(renderer)
	{
		if(mViewId<0)
			mViewId =renderer->AddView();
		renderer->ResizeView(mViewId,viewport.w,viewport.h);
	}
}

void DisplaySurface::Release()
{
	if(renderer)
		renderer->RemoveView(mViewId);
}
