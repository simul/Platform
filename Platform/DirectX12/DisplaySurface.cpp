#include "DisplaySurface.h"
#include "RenderPlatform.h"

using namespace simul;
using namespace dx12;

DisplaySurface::DisplaySurface():
    mDeviceRef(nullptr),
    mQueue(nullptr),
    mSwapChain(nullptr),
    mRTHeap(nullptr)
	,mCommandList(nullptr)
{
    for (int i = 0; i < FrameCount; i++)
    {
        mBackBuffers[i] = nullptr;
        mCommandAllocators[i] = nullptr;
        mGPUFences[i] = nullptr;
        mFenceValues[i] = 0;
    }
}

DisplaySurface::~DisplaySurface()
{
    WaitForAllWorkDone();
    InvalidateDeviceObjects();
}

void DisplaySurface::RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, int numerator, int denominator, crossplatform::PixelFormat outFmt)
{
    if (mHwnd && mHwnd == handle)
    {
        return;
    }
    InvalidateDeviceObjects();

    mHwnd                   = handle;
    this->renderPlatform    = renderPlatform;
    mIsVSYNC                = vsync;

	HRESULT res = S_FALSE;
	RECT rect	= {};
    mDeviceRef  = renderPlatform->AsD3D12Device();

#if defined(WINVER) && !defined(_XBOX_ONE) &&!defined(_GAMING_XBOX)
	GetWindowRect(mHwnd, &rect);
#endif

	int screenWidth			= abs(rect.right - rect.left);
	int screenHeight		= abs(rect.bottom - rect.top);

	// Viewport
	mCurViewport.TopLeftX	= 0;
	mCurViewport.TopLeftY	= 0;
	mCurViewport.Width		= (float)screenWidth;
	mCurViewport.Height		= (float)screenHeight;
	mCurViewport.MinDepth	= 0.0f;
	mCurViewport.MaxDepth	= 1.0f;

	// Scissor
	mCurScissor.left		= 0;
	mCurScissor.top		    = 0;
	mCurScissor.right	    = screenWidth;
	mCurScissor.bottom	    = screenHeight;

   viewport.w              = screenWidth;
   viewport.h              = screenHeight;
   viewport.x              = viewport.y = 0;

#ifndef _XBOX_ONE 
#ifndef _GAMING_XBOX
	// Dx12 swap chain	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc12	= {};
	swapChainDesc12.BufferCount				= FrameCount;
	swapChainDesc12.Width					= 0; 
	swapChainDesc12.Height					= 0; 
	swapChainDesc12.Format					= dx12::RenderPlatform::ToDxgiFormat(outFmt);
	swapChainDesc12.BufferUsage				= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc12.SwapEffect				= DXGI_SWAP_EFFECT_FLIP_DISCARD; 
	swapChainDesc12.SampleDesc.Count		= 1;
	swapChainDesc12.SampleDesc.Quality		= 0;

	IDXGIFactory4* factory		= nullptr;
	res							= CreateDXGIFactory2(0, SIMUL_PPV_ARGS(&factory));
	SIMUL_ASSERT(res == S_OK);

    // Create a command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc  = {};
    queueDesc.Type                      = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags                     = D3D12_COMMAND_QUEUE_FLAG_NONE;
    res = mDeviceRef->CreateCommandQueue(&queueDesc, SIMUL_PPV_ARGS(&mQueue));
    SIMUL_ASSERT(res == S_OK);
    mQueue->SetName(L"WindowQueue");

	// Create it
	IDXGISwapChain1* swapChain	= nullptr;
	res							= factory->CreateSwapChainForHwnd
	(
        mQueue,
        mHwnd,
		&swapChainDesc12,
		nullptr,
		nullptr,
		&swapChain
	);
	SIMUL_ASSERT(res == S_OK);

	// Assign and query
	factory->MakeWindowAssociation(mHwnd, DXGI_MWA_NO_ALT_ENTER);
	swapChain->QueryInterface(__uuidof(IDXGISwapChain4), (void **)&mSwapChain);

#endif
#endif
	// Initialize render targets
    CreateRenderTargets(mDeviceRef);

#ifndef _XBOX_ONE
#ifndef _GAMING_XBOX
	SAFE_RELEASE(factory);
	SAFE_RELEASE(swapChain);
#endif
#endif

    CreateSyncObjects();

    // Create this window command list
    mDeviceRef->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[0], nullptr, SIMUL_PPV_ARGS(&mCommandList));
    mCommandList->SetName(L"WindowCommandList 2");
    mRecordingCommands = true;

    // Provide a cmd list so we can start recording commands
    auto dx12plat = (dx12::RenderPlatform*)renderPlatform;
    dx12plat->SetCommandList(mCommandList);
    dx12plat->DefaultOutputFormat = outFmt;
}

void DisplaySurface::InvalidateDeviceObjects()
{
    if (mSwapChain)
    {
        WaitForAllWorkDone();
    }
	//SAFE_RELEASE(mCommandList);
    SAFE_RELEASE(mSwapChain);
    SAFE_RELEASE_ARRAY(mBackBuffers, FrameCount);
    SAFE_RELEASE(mRTHeap);
    SAFE_RELEASE_ARRAY(mCommandAllocators, FrameCount);
    SAFE_RELEASE(mQueue);
    SAFE_RELEASE_ARRAY(mGPUFences, FrameCount);
}

unsigned DisplaySurface::GetCurrentBackBufferIndex() const
{
	UINT curIdx =0;
#ifndef _XBOX_ONE
#ifndef _GAMING_XBOX
    if(mSwapChain)
		curIdx = mSwapChain->GetCurrentBackBufferIndex();
#endif
#endif
	return curIdx;
}
void DisplaySurface::Render(simul::base::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber)
{
	if (delegatorReadWriteMutex)
		delegatorReadWriteMutex->lock_for_write();
    Resize();
    // First lets make sure is safe to start working on this frame:
   // StartFrame();
#if defined( _XBOX_ONE) || defined(_GAMING_XBOX) 
    UINT curIdx =0;
#else
    UINT curIdx = GetCurrentBackBufferIndex();
#endif
    HRESULT res = S_FALSE;

    // Set viewport 
    mCommandList->RSSetViewports(1, &mCurViewport);
    mCommandList->RSSetScissorRects(1, &mCurScissor);

    // Indicate that the back buffer will be used as a render target.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBackBuffers[curIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mCommandList->OMSetRenderTargets(1, &mRTHandles[curIdx], FALSE, nullptr);

    // Submit commands
    const float kClearColor[4] = { 0.0f,0.0f,0.0f,1.0f };
    mCommandList->ClearRenderTargetView(mRTHandles[curIdx], kClearColor, 0, nullptr);

    if (renderer)
    {
        renderer->Render(mViewId, mCommandList, &mRTHandles[curIdx], mCurScissor.right, mCurScissor.bottom,frameNumber);
    }

    // Get ready to present
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBackBuffers[curIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    EndFrame();
	if (delegatorReadWriteMutex)
		delegatorReadWriteMutex->unlock_from_write();
}

void DisplaySurface::CreateRenderTargets(ID3D12Device* device)
{
    HRESULT result = {};
	// Describe and create a render target view (RTV) descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc	= {};
	rtvHeapDesc.NumDescriptors				= FrameCount; 
	rtvHeapDesc.Type						= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags						= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result									= device->CreateDescriptorHeap(&rtvHeapDesc, SIMUL_PPV_ARGS(&mRTHeap));
	SIMUL_ASSERT(result == S_OK);

	mRTHeap->SetName(L"WindowRTHeap");
	UINT rtHandleSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRTHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT n = 0; n < FrameCount; n++)
	{
		// Store a DX12 Cpu handle for the render targets
		mRTHandles[n] = rtvHandle;

		result = mSwapChain->GetBuffer(n, __uuidof(ID3D12Resource), (LPVOID*)&mBackBuffers[n]);
		SIMUL_ASSERT(result == S_OK);
		mBackBuffers[n]->SetName(L"WindowBackBuffer");

		device->CreateRenderTargetView(mBackBuffers[n], nullptr, rtvHandle);
		rtvHandle.Offset(1, rtHandleSize);

		result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, SIMUL_PPV_ARGS(&mCommandAllocators[n]));
		SIMUL_ASSERT(result == S_OK);
	}
}

void DisplaySurface::CreateSyncObjects()
{
    mWindowEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    for (int i = 0; i < FrameCount; i++)
    {
        mFenceValues[i] = 0;
        mDeviceRef->CreateFence(mFenceValues[i], D3D12_FENCE_FLAG_NONE, SIMUL_PPV_ARGS(&mGPUFences[i]));
    }
}

void DisplaySurface::StartFrame()
{
    // If we already requested the command list just exit
    if (mRecordingCommands) { return; }

    HRESULT res = S_FALSE;
    UINT idx    = GetCurrentBackBufferIndex();

    // If the GPU is behind, wait:
    if (mGPUFences[idx]->GetCompletedValue() < mFenceValues[idx])
    {
        mGPUFences[idx]->SetEventOnCompletion(mFenceValues[idx], mWindowEvent);
        WaitForSingleObject(mWindowEvent, INFINITE);
    }
    // EndFrame will Signal this value:
    mFenceValues[idx]++;

    res = mCommandAllocators[idx]->Reset();
    SIMUL_ASSERT(res == S_OK);
    res = mCommandList->Reset(mCommandAllocators[idx], nullptr);
    SIMUL_ASSERT(res == S_OK);
    mRecordingCommands = true;
}

void DisplaySurface::EndFrame()
{
	// Perfectly valid to call this if we didn't begin the frame for this surface.
    if (!mRecordingCommands)
		return;
    mRecordingCommands = false;

    HRESULT res = S_FALSE;
    res = mCommandList->Close();
    SIMUL_ASSERT(res == S_OK);
    ID3D12CommandList* ppCommandLists[] = { mCommandList };
    mQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Cache the current idx:
    int idx = GetCurrentBackBufferIndex();

    // Present new frame
    const DWORD dwFlags = 0;
    const UINT SyncInterval = 1;
    res = mSwapChain->Present(SyncInterval, dwFlags);
    if (res != S_OK)
    {
        HRESULT removedRes = mDeviceRef->GetDeviceRemovedReason();
        SIMUL_CERR << removedRes << std::endl;
    }
    SIMUL_ASSERT(res == S_OK);

    // Signal at the end of the pipe, note that we use the cached index 
    // or we will be adding a fence for the next frame!
    mQueue->Signal(mGPUFences[idx], mFenceValues[idx]);
}

void DisplaySurface::WaitForAllWorkDone()
{
    for (int i = 0; i < FrameCount; i++)
    {
        if (mGPUFences[i]->GetCompletedValue() < mFenceValues[i])
        {
            mGPUFences[i]->SetEventOnCompletion(mFenceValues[i], mWindowEvent);
            WaitForSingleObject(mWindowEvent, INFINITE);
        }
    }
}

void DisplaySurface::Resize()
{
    RECT rect = {};
#if defined(WINVER) && !defined(_XBOX_ONE) &&!defined(_GAMING_XBOX)
    GetWindowRect(mHwnd, &rect);
#endif
    int screenWidth = abs(rect.right - rect.left);
    int screenHeight = abs(rect.bottom - rect.top);
    if (screenWidth == mCurViewport.Width && screenHeight == mCurViewport.Height)
    {
        return;
    }

    if (mRecordingCommands)
    {
        EndFrame();
    }

    int idx = (GetCurrentBackBufferIndex() + (FrameCount - 1)) % FrameCount;
    if (mGPUFences[idx]->GetCompletedValue() < mFenceValues[idx])
    {
        mGPUFences[idx]->SetEventOnCompletion(mFenceValues[idx], mWindowEvent);
        WaitForSingleObject(mWindowEvent, INFINITE);
    }

    SAFE_RELEASE(mRTHeap);
    SAFE_RELEASE_ARRAY(mBackBuffers, FrameCount);
    for (UINT i = 0; i < FrameCount; i++)
    {
        mCommandAllocators[i]->Reset();
    }
    SAFE_RELEASE_ARRAY(mCommandAllocators, FrameCount);

    // DX12 viewport
    mCurViewport.TopLeftX   = 0;
    mCurViewport.TopLeftY   = 0;
    mCurViewport.Width      = (float)screenWidth;
    mCurViewport.Height     = (float)screenHeight;
    mCurViewport.MinDepth   = 0.0f;
    mCurViewport.MaxDepth   = 1.0f;

    // DX12 scissor rect	
    mCurScissor.left        = 0;
    mCurScissor.top         = 0;
    mCurScissor.right       = screenWidth;
    mCurScissor.bottom      = screenHeight;

    viewport.w              = screenWidth;
    viewport.h              = screenHeight;
    viewport.x              = viewport.y = 0;

    // Resize the swapchain buffers
    HRESULT res = mSwapChain->ResizeBuffers
    (
        0,						// Use current back buffer count
        screenWidth, screenHeight,
        DXGI_FORMAT_UNKNOWN,	// Use current format
        0
    );
    SIMUL_ASSERT(res == S_OK);
    CreateRenderTargets(mDeviceRef);

    renderer->ResizeView(mViewId, screenWidth, screenHeight);
	StartFrame();
}

