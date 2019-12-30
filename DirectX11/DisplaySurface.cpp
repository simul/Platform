#include "DisplaySurface.h"
#include "RenderPlatform.h"

using namespace simul;
using namespace dx11;

DisplaySurface::DisplaySurface():
    mSwapChain(nullptr),
    mBackBufferRT(nullptr),
    mBackBuffer(nullptr),
    mDeviceRef(nullptr),
    mDeferredContext(nullptr)
	,mCommandList(nullptr)
{
}

DisplaySurface::~DisplaySurface()
{
    InvalidateDeviceObjects();
}

void DisplaySurface::RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* r, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)
{
    if (mHwnd && mHwnd == handle)
    {
        return;
    }
	mIsVSYNC = vsync;
	renderPlatform=r;
	pixelFormat=outFmt;
    mDeviceRef                          = renderPlatform->AsD3D11Device();
    mHwnd                               = handle;
}

void DisplaySurface::InvalidateDeviceObjects()
{
    SAFE_RELEASE(mDeferredContext);
    SAFE_RELEASE(mBackBufferRT);
    SAFE_RELEASE(mBackBuffer);
    SAFE_RELEASE(mSwapChain);
    SAFE_RELEASE(mCommandList);
	mDeviceRef=nullptr;
}

void DisplaySurface::InitSwapChain()
{
	if(mSwapChain)
		return;
	if(!renderPlatform->AsD3D11Device())
		return;
    DXGI_SWAP_CHAIN_DESC swapChainDesc  = {};
    D3D11_RASTERIZER_DESC rasterDesc    = {};
    RECT rect;

#if defined(WINVER) && !defined(_XBOX_ONE)
    GetWindowRect((HWND)mHwnd, &rect);
#endif

    int screenWidth  = abs(rect.right - rect.left);
    int screenHeight = abs(rect.bottom - rect.top);
    if (mViewport.Width == screenWidth && mViewport.Height == screenHeight&&mSwapChain != nullptr)
    {
        return;
    }
    SAFE_RELEASE(mSwapChain);
   
    mViewport.Width      = (float)screenWidth;
    mViewport.Height     = (float)screenHeight;
    mViewport.TopLeftX   = (float)mViewport.TopLeftY = 0;
    mViewport.MinDepth   = 0.0f;
    mViewport.MaxDepth   = 1.0f;

    viewport.h          = screenHeight;
    viewport.w          = screenWidth;
    viewport.x          = 0;
    viewport.y          = 0;

    // Initialize the swap chain description.
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
    static int uu = 3;

    // Set number of back buffers.
    swapChainDesc.BufferCount                           = uu;
    swapChainDesc.SwapEffect                            = DXGI_SWAP_EFFECT_SEQUENTIAL;
    // Set the width and height of the back buffer.
    swapChainDesc.BufferDesc.Width                      = screenWidth;
    swapChainDesc.BufferDesc.Height                     = screenHeight;
    // Set regular 32-bit surface for the back buffer.
    swapChainDesc.BufferDesc.Format                     = dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	if (mIsVSYNC)
	{
		//swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
		//swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
	}
	else
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}
    // Set the usage of the back buffer.
    swapChainDesc.BufferUsage                           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    // Set the handle for the window to render to.
    swapChainDesc.OutputWindow                          = (HWND)mHwnd;
    // Turn multisampling off.
    swapChainDesc.SampleDesc.Count                      = 1;
    swapChainDesc.SampleDesc.Quality                    = 0;
    // Set to  windowed mode initially always. Later we can make it fullscreen, otherwise DX gets confused.
    swapChainDesc.Windowed                              = true;
    // Set the scan line ordering and scaling to unspecified.
    swapChainDesc.BufferDesc.ScanlineOrdering           = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling                    = DXGI_MODE_SCALING_UNSPECIFIED;
    // Don't set the advanced flags.
    swapChainDesc.Flags                                 = 0;
    swapChainDesc.Flags                                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Get the pointer to the back buffer.
    HRESULT result = S_OK;
#ifndef _XBOX_ONE
	IDXGIDevice * pDXGIDevice;
	renderPlatform->AsD3D11Device()->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
	IDXGIAdapter * pDXGIAdapter;
	pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);
	IDXGIFactory * factory;
	V_CHECK(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&factory));
	V_CHECK(factory->CreateSwapChain(renderPlatform->AsD3D11Device(), &swapChainDesc, &mSwapChain));

	SAFE_RELEASE(factory);
	SAFE_RELEASE(pDXGIAdapter);
	SAFE_RELEASE(pDXGIDevice);
#endif
	if(mDeviceRef)
	{
		UINT cf=mDeviceRef->GetCreationFlags();
		//std::cout<<"Creation flags "<<cf<<"."<<std::endl;
		SAFE_RELEASE(mDeferredContext);
		V_CHECK(mDeviceRef->CreateDeferredContext(0,&mDeferredContext));
	}
    SAFE_RELEASE(mBackBuffer);
    result = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&mBackBuffer);
    SIMUL_ASSERT(result == S_OK);
    // Create the render target view with the back buffer pointer.
    SAFE_RELEASE(mBackBufferRT);
	if(mDeviceRef)
	    result = mDeviceRef->CreateRenderTargetView(mBackBuffer, NULL, &mBackBufferRT);

	viewport.w = swapChainDesc.BufferDesc.Width;
	viewport.h = swapChainDesc.BufferDesc.Height;
	viewport.x = 0;
	viewport.y = 0;
	if(renderer)
		renderer->ResizeView(mViewId, viewport.w, viewport.h);
    SIMUL_ASSERT(result == S_OK);
}

void DisplaySurface::Render(simul::base::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber)
{
	if(mCommandList)
		return;
	if(!mDeferredContext)
		return;

	if (delegatorReadWriteMutex)
		delegatorReadWriteMutex->lock_for_write();

	deferredContext.platform_context=mDeferredContext;
	deferredContext.renderPlatform=renderPlatform;

	renderPlatform->StoreRenderState(deferredContext);
    mDeferredContext->OMSetRenderTargets(1, &mBackBufferRT, nullptr);
#if 1
    const float clear[4] = { 0.0f,0.0f,0.0f,1.0f };
    mDeferredContext->ClearRenderTargetView(mBackBufferRT, clear);
    mDeferredContext->RSSetViewports(1, &mViewport);
	if(renderer)
	    renderer->Render(mViewId, mDeferredContext, mBackBufferRT, (int)mViewport.Width, (int)mViewport.Height, frameNumber);
#endif
    mDeferredContext->OMSetRenderTargets(0, nullptr, nullptr);
	renderPlatform->RestoreRenderState(deferredContext);
	mDeferredContext->FinishCommandList(true,&mCommandList);

	if (delegatorReadWriteMutex)
		delegatorReadWriteMutex->unlock_from_write();
}

void DisplaySurface::EndFrame()
{
	if(mCommandList)
	{
		renderPlatform->GetImmediateContext().asD3D11DeviceContext()->ExecuteCommandList(mCommandList,true);
		SAFE_RELEASE(mCommandList);
		static DWORD dwFlags        = 0;
		static UINT SyncInterval    = 0;
		V_CHECK(mSwapChain->Present(SyncInterval, dwFlags));
	}
	// We check for resize here, because we must manage the SwapChain from the main thread.
	// we may have to do it after executing the command list, because Resize destroys the CL, and we don't want to lose commands.
    Resize();
}

void DisplaySurface::Resize()
{
    RECT rect;
	InitSwapChain();
#if defined(WINVER) &&!defined(_XBOX_ONE)
    if (!GetWindowRect((HWND)mHwnd, &rect))
        return;
	if(!mSwapChain)
		return;
#endif
    UINT W = abs(rect.right - rect.left);
    UINT H = abs(rect.bottom - rect.top);
    DXGI_SWAP_CHAIN_DESC swapDesc;
    HRESULT hr = mSwapChain->GetDesc(&swapDesc);
    if (hr != S_OK)
        return;
    if (swapDesc.BufferDesc.Width == W&&swapDesc.BufferDesc.Height == H)
        return;
    
	SAFE_RELEASE(mBackBufferRT);
	SAFE_RELEASE(mBackBuffer);
	SAFE_RELEASE(mCommandList);// got to do this for some reason.

    V_CHECK(mSwapChain->ResizeBuffers(0, W, H, DXGI_FORMAT_UNKNOWN, 0));

    // Query new buffers
    HRESULT result = {};
    mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&mBackBuffer);
    SIMUL_ASSERT(result == S_OK);
    result = mDeviceRef->CreateRenderTargetView(mBackBuffer, NULL, &mBackBufferRT);
    SIMUL_ASSERT(result == S_OK);
   
    viewport.w          = W;
    viewport.h          = H;
    viewport.x          = 0;
    viewport.y          = 0;

    mViewport.Width      = (float)W;
    mViewport.Height     = (float)H;

    renderer->ResizeView(mViewId,W,H);
}