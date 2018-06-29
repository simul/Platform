#include "SwapChain.h"


using namespace simul;
using namespace crossplatform;


SwapChain::SwapChain()
	:
	hwnd(0),size(0,0),bufferCount(0)
		,pixelFormat(PixelFormat::UNKNOWN)
	,fullscreen(false)
{
}


SwapChain::~SwapChain()
{
}

void SwapChain::InvalidateDeviceObjects()
{
}


void SwapChain::RestoreDeviceObjects(RenderPlatform *r,DeviceContext &deviceContext,cp_hwnd h,PixelFormat f,int c)
{
	renderPlatform=r;
	hwnd=h;
	pixelFormat=f;
	bufferCount=c;
}