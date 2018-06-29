#include "SwapChain.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"

using namespace simul;
using namespace dx11;



SwapChain::SwapChain()
	:pSwapChain(nullptr)
{
}


SwapChain::~SwapChain()
{
	InvalidateDeviceObjects();
}

void SwapChain::RestoreDeviceObjects(crossplatform::RenderPlatform *r,crossplatform::DeviceContext &deviceContext,cp_hwnd h,crossplatform::PixelFormat f,int count)
{
	crossplatform::SwapChain::RestoreDeviceObjects(r,deviceContext,h,f,count);
	hwnd=h;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	D3D11_RASTERIZER_DESC rasterDesc;
	RECT rect;
#if defined(WINVER) && !defined(_XBOX_ONE)
	GetWindowRect((HWND)hwnd,&rect);
#endif
	int screenWidth	=abs(rect.right-rect.left);
	int screenHeight=abs(rect.bottom-rect.top);
	if(size.x==screenWidth&&size.y==screenHeight&&pSwapChain!=nullptr)
		return;
	SAFE_RELEASE(pSwapChain);
	size.x=screenWidth;
	size.y=screenHeight;
	//Now that we have the refresh rate from the system we can start the DirectX initialization.
	//The first thing we'll do is fill out the description of the swap chain.
	//The swap chain is the front and back buffer to which the graphics will be drawn.
	//Generally you use a single back buffer, do all your drawing to it, and then swap it to the front buffer which then displays
	// on the user's screen. That is why it is called a swap chain.

	// Initialize the swap chain description.
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	static int uu = 3;
	// Set number of back buffers.
	swapChainDesc.BufferCount =uu;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

	// Set the width and height of the back buffer.
	swapChainDesc.BufferDesc.Width = screenWidth;
	swapChainDesc.BufferDesc.Height = screenHeight;
	// Set regular 32-bit surface for the back buffer.
	swapChainDesc.BufferDesc.Format = dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	// The next part of the description of the swap chain is the refresh rate.
	// The refresh rate is how many times a second it draws the back buffer to the front buffer.
	// If vsync is set to true in our graphicsclass.h header then this will lock the refresh rate
	// to the system settings (for example 60hz). That means it will only draw the screen 60 times
	// a second (or higher if the system refresh rate is more than 60). However if we set vsync to
	// false then it will draw the screen as many times a second as it can, however this can cause some visual artifacts.

	// Set the refresh rate of the back buffer.
	//if(m_vsync_enabled)
	{
		//swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
		//swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
	}
	//else
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	// Set the usage of the back buffer.
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Set the handle for the window to render to.
	swapChainDesc.OutputWindow = (HWND)hwnd;

	// Turn multisampling off.
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;

	// Set to  windowed mode initially always. Later we can make it fullscreen, otherwise DX gets confused.
	//if(fullscreen)
	{
	//	swapChainDesc.Windowed = false;
	}
	//else
	{
		swapChainDesc.Windowed = true;
	}

	// Set the scan line ordering and scaling to unspecified.
	swapChainDesc.BufferDesc.ScanlineOrdering	= DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling			= DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	//swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	swapChainDesc.Flags = 0;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	// Sometimes this call to create the device will fail if the primary video card is not compatible with DirectX 11.
	// Some machines may have the primary card as a DirectX 10 video card and the secondary card as a DirectX 11 video card.
	// Also some hybrid graphics cards work that way with the primary being the low power Intel card and the secondary being
	// the high power Nvidia card. To get around this you will need to not use the default device and instead enumerate all
	// the video cards in the machine and have the user choose which one to use and then specify that card when creating the device.

	// Now that we have a swap chain we need to get a pointer to the back buffer and then attach it to the swap chain.
	// We'll use the CreateRenderTargetView function to attach the back buffer to our swap chain.

	// Get the pointer to the back buffer.
	HRESULT result=S_OK;
#ifndef _XBOX_ONE
/*	NOTE: ***YOU CANNOT DO THIS***
  IDXGIFactory * factory;
	result = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&factory);
SIMUL_ASSERT(result==S_OK);*/

	IDXGIDevice * pDXGIDevice;
	renderPlatform->AsD3D11Device()->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);      
	IDXGIAdapter * pDXGIAdapter;
	pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);
	IDXGIFactory * factory;
	V_CHECK(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&factory));
	V_CHECK(factory->CreateSwapChain(renderPlatform->AsD3D11Device(),&swapChainDesc,&pSwapChain));

	SAFE_RELEASE(factory);
	SAFE_RELEASE(pDXGIAdapter);
	SAFE_RELEASE(pDXGIDevice);
#endif
}

	

void SwapChain::InvalidateDeviceObjects()
{
	SAFE_RELEASE(pSwapChain);
	crossplatform::SwapChain::InvalidateDeviceObjects();
}

bool SwapChain::IsFullscreen() const
{
	return fullscreen;
}

void SwapChain::SetFullscreen(bool)
{
}

IDXGISwapChain *SwapChain::AsDXGISwapChain()
{
	return pSwapChain;
}

void SwapChain::Resize()
{
	RECT rect;
#if defined(WINVER) &&!defined(_XBOX_ONE)
	if(!GetWindowRect((HWND)hwnd,&rect))
		return;
#endif
	UINT W	=abs(rect.right-rect.left);
	UINT H	=abs(rect.bottom-rect.top);
	ID3D11RenderTargetView *t[]={NULL};
	DXGI_SWAP_CHAIN_DESC swapDesc;
	HRESULT hr=pSwapChain->GetDesc(&swapDesc);
	if(hr!=S_OK)
		return;
	if(swapDesc.BufferDesc.Width==W&&swapDesc.BufferDesc.Height==H)
		return;
	V_CHECK(pSwapChain->ResizeBuffers(1,W,H,DXGI_FORMAT_R8G8B8A8_UNORM,0));
	
	DXGI_SURFACE_DESC surfaceDesc;
	pSwapChain->GetDesc(&swapDesc);
	surfaceDesc.Format		=swapDesc.BufferDesc.Format;
	surfaceDesc.SampleDesc	=swapDesc.SampleDesc;
	surfaceDesc.Width		=swapDesc.BufferDesc.Width;
	surfaceDesc.Height		=swapDesc.BufferDesc.Height;
	// TODO: WHy was this disabled?:
	//	renderer->ResizeView(view_id,surfaceDesc.Width,surfaceDesc.Height);
}