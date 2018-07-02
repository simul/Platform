#include "DisplaySurface.h"
#include "RenderPlatform.h"

using namespace simul;
using namespace dx11;

DisplaySurface::DisplaySurface():
    mSwapChain(nullptr),
    mBackBufferRT(nullptr),
    mBackBuffer(nullptr),
    mDeviceRef(nullptr)
{
}

DisplaySurface::~DisplaySurface()
{
    InvalidateDeviceObjects();
}

void DisplaySurface::RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)
{
    if (mHwnd && mHwnd == handle)
    {
        return;
    }
    mDeviceRef                          = renderPlatform->AsD3D11Device();
    mHwnd                               = handle;
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
   
    mViewport.Width      = screenWidth;
    mViewport.Height     = screenHeight;
    mViewport.TopLeftX   = mViewport.TopLeftY = 0;
    mViewport.MinDepth   = 0.0f;
    mViewport.MaxDepth   = 1.0f;

    Viewport.h          = screenHeight;
    Viewport.w          = screenWidth;
    Viewport.x          = 0;
    Viewport.y          = 0;

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
    swapChainDesc.BufferDesc.Format                     = dx11::RenderPlatform::ToDxgiFormat(outFmt);
    swapChainDesc.BufferDesc.RefreshRate.Numerator      = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator    = 1;
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

    result = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&mBackBuffer);
    SIMUL_ASSERT(result == S_OK);
    // Create the render target view with the back buffer pointer.
    SAFE_RELEASE(mBackBufferRT);
    result = mDeviceRef->CreateRenderTargetView(mBackBuffer, NULL, &mBackBufferRT);
    SIMUL_ASSERT(result == S_OK);
}

void DisplaySurface::InvalidateDeviceObjects()
{
    SAFE_RELEASE(mBackBufferRT);
    SAFE_RELEASE(mBackBuffer);
    SAFE_RELEASE(mSwapChain);
}

void DisplaySurface::Render()
{
    Resize();

    ID3D11DeviceContext* pImmediateContext = nullptr;
    mDeviceRef->GetImmediateContext(&pImmediateContext);
    pImmediateContext->OMSetRenderTargets(1, &mBackBufferRT, nullptr);
    const float clear[4] = { 0.0f,0.0f,0.0f,1.0f };
    pImmediateContext->ClearRenderTargetView(mBackBufferRT, clear);
    pImmediateContext->RSSetViewports(1, &mViewport);

    renderer->Render(mViewId, pImmediateContext, mBackBufferRT, mViewport.Width,mViewport.Height);

    static DWORD dwFlags        = 0;
    static UINT SyncInterval    = 0;
    V_CHECK(mSwapChain->Present(SyncInterval, dwFlags));
}

void DisplaySurface::Resize()
{
    RECT rect;
#if defined(WINVER) &&!defined(_XBOX_ONE)
    if (!GetWindowRect((HWND)mHwnd, &rect))
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
    
    mBackBufferRT->Release();
    mBackBuffer->Release();

    V_CHECK(mSwapChain->ResizeBuffers(0, W, H, DXGI_FORMAT_UNKNOWN, 0));

    // Query new buffers
    HRESULT result = {};
    mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&mBackBuffer);
    SIMUL_ASSERT(result == S_OK);
    result = mDeviceRef->CreateRenderTargetView(mBackBuffer, NULL, &mBackBufferRT);
    SIMUL_ASSERT(result == S_OK);
   
    Viewport.w          = W;
    Viewport.h          = H;
    Viewport.x          = 0;
    Viewport.y          = 0;

    mViewport.Width      = W;
    mViewport.Height     = H;

    renderer->ResizeView(mViewId,W,H);
}