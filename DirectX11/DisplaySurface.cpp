#include "DisplaySurface.h"
#include "RenderPlatform.h"
#include "Platform/CrossPlatform/RenderDelegater.h"

using namespace platform;
using namespace dx11;

DisplaySurface::DisplaySurface(int view_id)
	:crossplatform::DisplaySurface(view_id)
	,mSwapChain(nullptr),
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
	dx11::RenderPlatform *r11=(dx11::RenderPlatform *)r;
	mDeviceRef                          = r11->AsD3D11Device();
	mHwnd                               = handle;
	SAFE_RELEASE(mDeferredContext);
	if (mDeviceRef)
	{
		UINT cf = mDeviceRef->GetCreationFlags();
		V_CHECK(mDeviceRef->CreateDeferredContext(0, &mDeferredContext));
	}
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

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc  = {};

	RECT rect;
#if defined(WINVER) && !defined(_XBOX_ONE)
	GetClientRect((HWND)mHwnd, &rect);
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
	mViewport.TopLeftX   = mViewport.TopLeftY = 0.f;
	mViewport.MinDepth   = 0.0f;
	mViewport.MaxDepth   = 1.0f;

	viewport.h          = screenHeight;
	viewport.w          = screenWidth;
	viewport.x          = 0;
	viewport.y          = 0;

	// Initialize the swap chain description.
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));


	// Set the width and height of the back buffer.
	swapChainDesc.Width = screenWidth;
	swapChainDesc.Height = screenHeight;
	// Set swapchain format for the back buffer.
	swapChainDesc.Format = dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	// Set swapchain to not be stereo.
	swapChainDesc.Stereo = false;
	// Turn multisampling off.
	swapChainDesc.SampleDesc = { 1, 0 };
	// Set the usage of the back buffer.
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	// Set number of back buffers.
	swapChainDesc.BufferCount = 3;
	// Set the swapchain scaling to stretch.
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	// Set the swap effect.
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	// Set the alpha mode to unspecified.
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// Set flags for V-Sync and back buffer resizing.
	swapChainDesc.Flags = (mIsVSYNC == false ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0) | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	//Create Swapchain.
	HRESULT result = S_OK;
#ifndef _XBOX_ONE
	IDXGIDevice * pDXGIDevice;
	renderPlatform->AsD3D11Device()->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
	IDXGIAdapter * pDXGIAdapter;
	pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);
	IDXGIFactory2 * factory;
	V_CHECK(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void **)&factory));
	V_CHECK(factory->CreateSwapChainForHwnd(renderPlatform->AsD3D11Device(), (HWND)mHwnd, &swapChainDesc, nullptr, nullptr, &mSwapChain));

	SAFE_RELEASE(factory);
	SAFE_RELEASE(pDXGIAdapter);
	SAFE_RELEASE(pDXGIDevice);
#endif

	// Get the pointer to the back buffer.
	SAFE_RELEASE(mBackBuffer);
	result = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&mBackBuffer);
	SIMUL_ASSERT(result == S_OK);
	// Create the render target view with the back buffer pointer.
	SAFE_RELEASE(mBackBufferRT);
	if(mDeviceRef)
		result = mDeviceRef->CreateRenderTargetView(mBackBuffer, NULL, &mBackBufferRT);

	viewport.w = swapChainDesc.Width;
	viewport.h = swapChainDesc.Height;
	viewport.x = 0;
	viewport.y = 0;
	if(renderer)
		renderer->ResizeView(mViewId, viewport.w, viewport.h);
	SIMUL_ASSERT(result == S_OK);
}

void DisplaySurface::Render(platform::core::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber)
{
	if(mCommandList)
		return;
	if(!mDeferredContext)
		return;
	
	 if (!mBackBufferRT)
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
		static DWORD dwFlags        = mIsVSYNC ? 0 : DXGI_PRESENT_ALLOW_TEARING;
		static UINT SyncInterval    = mIsVSYNC ? 1 : 0;
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
	if (!GetClientRect((HWND)mHwnd, &rect))
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