#include "SwapChain.h"
#include "Simul/Platform/DirectX12/SimulDirectXHeader.h"

using namespace simul;
using namespace dx12;

SwapChain::SwapChain()
	:pSwapChain(nullptr)
{
    for (int i = 0; i < FrameCount; i++)
    {
        BackBuffers[i]          = nullptr;
        GPUFences[i]            = nullptr;
        CommandAllocators[i]    = nullptr;
        FenceValues[i]          = 0;
    }
}


SwapChain::~SwapChain()
{
	InvalidateDeviceObjects();
}

void SwapChain::InvalidateDeviceObjects()
{
	// TO-DO: actual gpu wait here...
	Sleep(1000);

	SAFE_RELEASE(pSwapChain);
	SAFE_RELEASE_ARRAY(BackBuffers,FrameCount);
	SAFE_RELEASE(RTHeap);
	SAFE_RELEASE_ARRAY(CommandAllocators,FrameCount);
	CommandQueueRef=nullptr;
	crossplatform::SwapChain::InvalidateDeviceObjects();
}

void SwapChain::RestoreDeviceObjects(crossplatform::RenderPlatform *r,crossplatform::DeviceContext &deviceContext,cp_hwnd h,crossplatform::PixelFormat f,int count)
{
	if (!r)
		return;
	crossplatform::SwapChain::RestoreDeviceObjects(r,deviceContext,h,f,count);
	HRESULT res = S_FALSE;
	RECT rect	= {};

#if defined(WINVER) && !defined(_XBOX_ONE)
	GetWindowRect(ConsoleWindowHandle, &rect);
#endif

	int screenWidth			= abs(rect.right - rect.left);
	int screenHeight		= abs(rect.bottom - rect.top);
	if(size.x==screenWidth&&size.y==screenHeight&&pSwapChain!=nullptr)
		return;
	SAFE_RELEASE(pSwapChain);
	size.x=screenWidth;
	size.y=screenHeight;

	// Viewport
	CurViewport.TopLeftX		= 0;
	CurViewport.TopLeftY		= 0;
	CurViewport.Width		    = (float)screenWidth;
	CurViewport.Height		    = (float)screenHeight;
	CurViewport.MinDepth		= 0.0f;
	CurViewport.MaxDepth		= 1.0f;

	// Scissor
	CurScissor.left		= 0;
	CurScissor.top		= 0;
	CurScissor.right	= screenWidth;
	CurScissor.bottom	= screenHeight;

#ifndef _XBOX_ONE
	// Dx12 swap chain	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc12	= {};
	swapChainDesc12.BufferCount				= FrameCount;
	swapChainDesc12.Width					= 0; 
	swapChainDesc12.Height					= 0; 
	swapChainDesc12.Format					= DXGI_FORMAT_R16G16B16A16_FLOAT;
	swapChainDesc12.BufferUsage				= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc12.SwapEffect				= DXGI_SWAP_EFFECT_FLIP_DISCARD; 
	swapChainDesc12.SampleDesc.Count		= 1;
	swapChainDesc12.SampleDesc.Quality		= 0;

	IDXGIFactory4* factory		= nullptr;
	res							= CreateDXGIFactory2(0, SIMUL_PPV_ARGS(&factory));
	SIMUL_ASSERT(res == S_OK);

	// Create it
	IDXGISwapChain1* swapChain	= nullptr;
	res							= factory->CreateSwapChainForHwnd
	(
		CommandQueueRef,
		ConsoleWindowHandle,
		&swapChainDesc12,
		nullptr,
		nullptr,
		&swapChain
	);
	SIMUL_ASSERT(res == S_OK);

	// Assign and query
	factory->MakeWindowAssociation(ConsoleWindowHandle, DXGI_MWA_NO_ALT_ENTER);
	swapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void **)&pSwapChain);

#endif

	// Initialize render targets
//	CreateRenderTarget();		

#ifndef _XBOX_ONE
	SAFE_RELEASE(factory);
	SAFE_RELEASE(swapChain);
#endif

    CreateSyncObjects();

    // Create this window command list
    DeviceRef->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocators[0], nullptr, SIMUL_PPV_ARGS(&CommandList));
    CommandList->SetName(L"WindowCommandList");
    CommandList->Close();
    RecordingCommands = false;
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
}

void SwapChain::CreateSyncObjects()
{
    WindowEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    for (int i = 0; i < FrameCount; i++)
    {
        FenceValues[i] = 0;
        DeviceRef->CreateFence(FenceValues[i], D3D12_FENCE_FLAG_NONE, SIMUL_PPV_ARGS(&GPUFences[i]));
    }
}