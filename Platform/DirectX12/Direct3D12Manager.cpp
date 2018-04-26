#include "Simul/Platform/DirectX12/Direct3D12Manager.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX12/MacrosDx1x.h"
#include "Simul/Platform/DirectX12/Utilities.h"
#include "Simul/Platform/DirectX12/SimulDirectXHeader.h"

#include <iomanip>

using namespace simul;
using namespace dx12;

Window::Window():
	ConsoleWindowHandle(0),
	IRendererRef(nullptr),
	WindowUID(-1),
	vsync(false),
    MustResize(false),
	SwapChain(nullptr),
	DeviceRef(nullptr)
{
}

Window::~Window()
{
	Release();
}

void Window::RestoreDeviceObjects(ID3D12Device* d3dDevice, bool m_vsync_enabled, int numerator, int denominator)
{
	if (!d3dDevice)
		return;

	HRESULT res = S_FALSE;
	RECT rect	= {};

#if defined(WINVER) && !defined(_XBOX_ONE)
	GetWindowRect(ConsoleWindowHandle, &rect);
#endif

	int screenWidth			= abs(rect.right - rect.left);
	int screenHeight		= abs(rect.bottom - rect.top);

	// Viewport
	CurViewport.TopLeftX		= 0;
	CurViewport.TopLeftY		= 0;
	CurViewport.Width		= (float)screenWidth;
	CurViewport.Height		= (float)screenHeight;
	CurViewport.MinDepth		= 0.0f;
	CurViewport.MaxDepth		= 1.0f;

	// Scissor
	CurScissor.left		= 0;
	CurScissor.top		= 0;
	CurScissor.right		= screenWidth;
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
	swapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void **)&SwapChain);

#endif

	// Initialize render targets
	CreateRenderTarget(d3dDevice);		

#ifndef _XBOX_ONE
	SAFE_RELEASE(factory);
	SAFE_RELEASE(swapChain);
#endif
}
#pragma optimize("",off)

void Window::ResizeSwapChain(ID3D12Device* d3dDevice)
{
    MustResize = false;
    Sleep(256);

	// Backbuffers
	SAFE_RELEASE(RTHeap);
	SAFE_RELEASE_ARRAY(BackBuffers, FrameCount);

	// Cmd allocators
	for (UINT i = 0; i < FrameCount; i++)
	{
		CommandAllocators[i]->Reset();
	}
	SAFE_RELEASE_ARRAY(CommandAllocators, FrameCount);
	
	RECT rect = {};
#if defined(WINVER) &&!defined(_XBOX_ONE)
	if (!GetWindowRect(ConsoleWindowHandle, &rect))
	{
		return;
	}
#endif
	int W	=abs(rect.right-rect.left);
	int H	=abs(rect.bottom-rect.top);

	// DX12 viewport
	CurViewport.TopLeftX	= 0;
	CurViewport.TopLeftY	= 0;
	CurViewport.Width		= (float)W;
	CurViewport.Height		= (float)H;
	CurViewport.MinDepth	= 0.0f;
	CurViewport.MaxDepth	= 1.0f;

	// DX12 scissor rect	
	CurScissor.left		= 0;
	CurScissor.top		= 0;
	CurScissor.right	= W;
	CurScissor.bottom	= H;

	// Resize the swapchain buffers
	HRESULT res = SwapChain->ResizeBuffers
	(
		0,						// Use current back buffer count
		W,H,
		DXGI_FORMAT_UNKNOWN,	// Use current format
		0
	);	
	SIMUL_ASSERT(res == S_OK);

	CreateRenderTarget(d3dDevice);

	//if (IRendererRef)
		//IRendererRef->ResizeView(WindowUID, W, H);
}

void Window::CreateRenderTarget(ID3D12Device* d3dDevice)
{
	HRESULT result = S_OK;
	if(!d3dDevice || !SwapChain)
		return;

	// Describe and create a render target view (RTV) descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc	= {};
	rtvHeapDesc.NumDescriptors				= FrameCount; 
	rtvHeapDesc.Type						= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags						= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result									= d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, SIMUL_PPV_ARGS(&RTHeap));
	SIMUL_ASSERT(result == S_OK);

	RTHeap->SetName(L"Dx12BackbufferRtHeap");
	UINT rtHandleSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Back buffers creation:
	// 1) Get the back buffer resources from the swap chain
	// 2) Create DX12 descriptor for each back buffer
	// 3) Create command allocators
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(RTHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT n = 0; n < FrameCount; n++)
	{
		// Store a DX12 Cpu handle for the render targets
		RTHandles[n] = rtvHandle;

		// 1)
		result = SwapChain->GetBuffer(n, __uuidof(ID3D12Resource), (LPVOID*)&BackBuffers[n]);
		SIMUL_ASSERT(result == S_OK);

		// 2)
		d3dDevice->CreateRenderTargetView(BackBuffers[n], nullptr, rtvHandle);
		BackBuffers[n]->SetName(L"Dx12BackBuffer");
		rtvHandle.Offset(1, rtHandleSize);

		// 3)
		result = d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, SIMUL_PPV_ARGS(&CommandAllocators[n]));
		SIMUL_ASSERT(result == S_OK);
	}
}

void Window::SetRenderer(crossplatform::PlatformRendererInterface *ci,int vw_id)
{
	if(IRendererRef==ci)
		return;
	if (IRendererRef)
	{
		IRendererRef->RemoveView(WindowUID);
	}
	WindowUID		=vw_id;
	IRendererRef	=ci;
	if(!SwapChain)
		return;

	DXGI_SWAP_CHAIN_DESC	swapDesc;
	DXGI_SURFACE_DESC		surfaceDesc;
	SwapChain->GetDesc(&swapDesc);
	surfaceDesc.Format		=swapDesc.BufferDesc.Format;
	surfaceDesc.SampleDesc	=swapDesc.SampleDesc;
	surfaceDesc.Width		=swapDesc.BufferDesc.Width;
	surfaceDesc.Height		=swapDesc.BufferDesc.Height;
	if (WindowUID < 0)
	{
		WindowUID				=IRendererRef->AddView();
	}
	
    // IRendererRef->ResizeView(WindowUID,surfaceDesc.Width,surfaceDesc.Height);
}

void Window::Release()
{
	// TO-DO: actual gpu wait here...
	Sleep(1000);

	SAFE_RELEASE(SwapChain);
	SAFE_RELEASE_ARRAY(BackBuffers,FrameCount);
	SAFE_RELEASE(RTHeap);
	SAFE_RELEASE_ARRAY(CommandAllocators,FrameCount);
	CommandQueueRef=nullptr;
}

void Window::SetCommandQueue(ID3D12CommandQueue *commandQueue) 
{
	CommandQueueRef = commandQueue;
}

UINT Window::GetCurrentIndex()
{
    return SwapChain->GetCurrentBackBufferIndex();
}

Direct3D12Manager::Direct3D12Manager():
	mDevice(nullptr),
	mCommandQueue(nullptr),
	mCommandList(nullptr),
	mFence(nullptr)
{
}

Direct3D12Manager::~Direct3D12Manager()
{
	Shutdown();
}

void Direct3D12Manager::Initialize(bool use_debug,bool instrument, bool default_driver )
{
	SIMUL_COUT << "=========================================\n";
	SIMUL_COUT << "Initializing Directx12 manager with: \n";
	SIMUL_COUT << "-Device Debug = " << (use_debug ? "enabled" : "disabled") << std::endl;

	HRESULT res	= S_FALSE;

#ifndef _XBOX_ONE

	// Debug layer
	UINT dxgiFactoryFlags = 0;
	if (use_debug)
	{
		ID3D12Debug* debugController = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(SIMUL_PPV_ARGS(&debugController))))
		{
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			debugController->EnableDebugLayer();

			// Enable GPU validation (it will report a list of errors if ocurred after ExecuteCommandList())
			bool doGPUValidation = false;
			SIMUL_COUT << "-Gpu Validation = " << (doGPUValidation ? "enabled" : "disabled") << std::endl;
			if (doGPUValidation)
			{
				ID3D12Debug1* debugController1 = nullptr;
				debugController->QueryInterface(SIMUL_PPV_ARGS(&debugController1));
				debugController1->SetEnableGPUBasedValidation(true);
			}
		}
		SAFE_RELEASE(debugController);
	}

	// Create a DirectX graphics interface factory.
	IDXGIFactory4* factory	= nullptr;
	res						= CreateDXGIFactory2(dxgiFactoryFlags, SIMUL_PPV_ARGS(&factory));
	SIMUL_ASSERT(res == S_OK);

	bool mUseWarpDevice = false;
	// Create device with the warp adapter
	if (mUseWarpDevice)
	{
		IDXGIAdapter* warpAdapter	= nullptr;
		factory->EnumWarpAdapter(SIMUL_PPV_ARGS(&warpAdapter));
		res							= D3D12CreateDevice(warpAdapter,D3D_FEATURE_LEVEL_11_0, SIMUL_PPV_ARGS(&mDevice));
		SIMUL_ASSERT(res == S_OK);
		SAFE_RELEASE(warpAdapter);
	}
	// Create device with hardware adapter
	else
	{
		IDXGIAdapter1* hardwareAdapter			= nullptr;
		DXGI_ADAPTER_DESC1 hardwareAdapterDesc	= {};
		int curAdapterIdx						= 0;
		bool adapterFound						= false;

		while (factory->EnumAdapters1(curAdapterIdx, &hardwareAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			// Query description
			hardwareAdapter->GetDesc1(&hardwareAdapterDesc);

			// Ignore warp adapter
			if (hardwareAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				curAdapterIdx++;
				continue;
			}

			// Check if has the right feature level
			res = D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(res))
			{
				adapterFound = true;
				break;
			}
			curAdapterIdx++;
		}
		res = D3D12CreateDevice(hardwareAdapter,D3D_FEATURE_LEVEL_11_0, SIMUL_PPV_ARGS(&mDevice));
		SIMUL_ASSERT(res == S_OK);

		// We must crete the device with debug flags if we want break on severity
		// or mute warnings
		if(use_debug)
		{
			ID3D12InfoQueue* infoQueue = nullptr;
			mDevice->QueryInterface(SIMUL_PPV_ARGS(&infoQueue));
			
			// Set break on_x settings
			bool breakOnWarning = true;
			SIMUL_COUT << "-Break on Warning = " << (breakOnWarning ? "enabled" : "disabled") << std::endl;
			if (breakOnWarning)
			{
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			}
			else
			{
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
			}

			// Filter msgs
			bool filterMsgs = true;
			if (filterMsgs)
			{
				D3D12_MESSAGE_ID msgs[] =
				{
					D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
					D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE
				};
				D3D12_INFO_QUEUE_FILTER filter	= {};
				filter.DenyList.pIDList			= msgs;
				filter.DenyList.NumIDs			= _countof(msgs);
				infoQueue->AddStorageFilterEntries(&filter);
			}
			SAFE_RELEASE(infoQueue);
		}

		// Store information about the GPU
		char gpuDesc[128];
		int gpuMem = (int)(hardwareAdapterDesc.DedicatedVideoMemory / 1024 / 1024);
		size_t stringLength;
		wcstombs_s(&stringLength, gpuDesc, 128, hardwareAdapterDesc.Description, 128);

		// Log
		SIMUL_COUT << "-Adapter: " << gpuDesc << std::endl;
		SIMUL_COUT << "-Adapter memory: " << gpuMem << "(MB)" << std::endl;

		// Enumerate outputs(monitors)
		IDXGIOutput* output = nullptr;
		int outputIdx		= 0;
		while (hardwareAdapter->EnumOutputs(outputIdx, &output) != DXGI_ERROR_NOT_FOUND)
		{
			outputs[outputIdx] = output;
			SIMUL_ASSERT(res == S_OK);
			outputIdx++;
			if (outputIdx>100)
			{
				std::cerr << "Tried 100 outputs: no adaptor was found." << std::endl;
				return;
			}
		}
		SAFE_RELEASE(hardwareAdapter);
	}
	SAFE_RELEASE(factory);

	// Create a command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc	= {};
	queueDesc.Type						= D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags						= D3D12_COMMAND_QUEUE_FLAG_NONE;
	res									= mDevice->CreateCommandQueue(&queueDesc, SIMUL_PPV_ARGS(&mCommandQueue));
	SIMUL_ASSERT(res == S_OK);

	// The window will create the swap chain latter...

#endif
}

int Direct3D12Manager::GetNumOutputs()
{
	return (int)outputs.size();
}

crossplatform::Output Direct3D12Manager::GetOutput(int i)
{
	unsigned numModes;
	crossplatform::Output o;
	IDXGIOutput *output=outputs[i];
#ifndef _XBOX_ONE
	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	HRESULT result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	SIMUL_ASSERT(result==S_OK);

	DXGI_OUTPUT_DESC outputDesc;
	output->GetDesc(&outputDesc);
	o.width		=abs(outputDesc.DesktopCoordinates.right-outputDesc.DesktopCoordinates.left);
	o.height	=abs(outputDesc.DesktopCoordinates.top-outputDesc.DesktopCoordinates.bottom);

	// Now get extended information about what monitor this is:
	
#if defined(WINVER) &&!defined(_XBOX_ONE)
	MONITORINFOEX monitor;
    monitor.cbSize = sizeof(monitor);
    if (::GetMonitorInfo(outputDesc.Monitor, &monitor) && monitor.szDevice[0])
    {
		DISPLAY_DEVICE dispDev;
        memset(&dispDev, 0, sizeof(dispDev));
		dispDev.cb = sizeof(dispDev);

       if (::EnumDisplayDevices(monitor.szDevice, 0, &dispDev, 0))
       {
           o.monitorName=base::WStringToUtf8(dispDev.DeviceName);
           o.desktopX	=monitor.rcMonitor.left;
           o.desktopY	=monitor.rcMonitor.top;
       }
   }
#endif
	// Create a list to hold all the possible display modes for this monitor/video card combination.
	DXGI_MODE_DESC* displayModeList;
	displayModeList = new DXGI_MODE_DESC[numModes];

	// Now fill the display mode list structures.
	result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	SIMUL_ASSERT(result==S_OK);
	
	// It is useful to get the refresh rate from the video card/monitor.
	//Each computer may be slightly different so we will need to query for that information.
	//We query for the numerator and denominator values and then pass them to DirectX during the setup and it will calculate the proper refresh rate.
	// If we don't do this and just set the refresh rate to a default value which may not exist on all computers then DirectX will respond by performing
	// a blit instead of a buffer flip which will degrade performance and give us annoying errors in the debug output.


	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	for(i=0; i<(int)numModes; i++)
	{
		if(displayModeList[i].Width == (unsigned int)o.width)
		{
			if(displayModeList[i].Height == (unsigned int)o.height)
			{
				o.numerator = displayModeList[i].RefreshRate.Numerator;
				o.denominator = displayModeList[i].RefreshRate.Denominator;
			}
		}
	}
	// Release the display mode list.
	delete [] displayModeList;
	displayModeList = 0;
#endif
	return o;
}
void Direct3D12Manager::Shutdown()
{
	// TO-DO: wait for the GPU to complete last work
	for(OutputMap::iterator i=outputs.begin();i!=outputs.end();i++)
	{
		SAFE_RELEASE(i->second);
	}
	outputs.clear();
	
	for(WindowMap::iterator i=windows.begin();i!=windows.end();i++)
	{
		SetFullScreen(i->second->ConsoleWindowHandle,false,0);
		delete i->second;
	}
	windows.clear();

	ReportMessageFilterState();

	SAFE_RELEASE(mDevice);
	//SAFE_RELEASE(m_commandQueue);
	//SAFE_RELEASE(m_commandList);
	//SAFE_DELETE(m_fence);
}

void Direct3D12Manager::RemoveWindow(HWND hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	SetFullScreen(hwnd,false,0);
	delete w;
	windows.erase(hwnd);
}

IDXGISwapChain* Direct3D12Manager::GetSwapChain(HWND h)
{
	if(windows.find(h)==windows.end())
		return NULL;
	Window *w=windows[h];
	if(!w)
		return NULL;
	return w->SwapChain;
}

void Direct3D12Manager::Render(HWND h)
{
    // Check that the window exists:
	if (windows.find(h) == windows.end())
	{
        SIMUL_CERR << "No window exists for HWND " << std::hex << h << std::endl;
		return;
	}
	Window* w = windows[h];

    // First lets check if there is a pending resize:
    bool resizeRenderer = false;
	if (w->MustResize)
	{
		w->ResizeSwapChain(mDevice);
		// Reset the frame values
		for (int i = 0; i < FrameCount; i++)
		{
			mFenceValues[i] = 0;
		}	
        resizeRenderer = true;
	}

    if (h != w->ConsoleWindowHandle)
	{
		SIMUL_CERR<<"Window for HWND "<<std::hex<<h<<" has hwnd "<<w->ConsoleWindowHandle <<std::endl;
		return;
	}

    UINT curIdx = w->GetCurrentIndex();
    HRESULT res = S_FALSE;

	// To start, first we have to setup dx12 for the new frame
	// Reset command allocators	
	res = w->CommandAllocators[curIdx]->Reset();
	SIMUL_ASSERT(res == S_OK);

	// Reset command list
	res =  mCommandList->Reset(w->CommandAllocators[curIdx],nullptr);
	SIMUL_ASSERT(res == S_OK); 

	// Set viewport 
	mCommandList->RSSetViewports(1, &w->CurViewport);
	mCommandList->RSSetScissorRects(1, &w->CurScissor);

	// Indicate that the back buffer will be used as a render target.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(w->BackBuffers[curIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	mCommandList->OMSetRenderTargets(1, &w->RTHandles[curIdx], FALSE, nullptr);
	
	// Submit commands
	const float kClearColor[4] = { 0.0f,0.0f,0.0f,1.0f };
	mCommandList->ClearRenderTargetView(w->RTHandles[curIdx], kClearColor , 0, nullptr);

	if (w->IRendererRef)
	{
        // Only call resize on the render interface during command list recording:
        if (resizeRenderer)
        {
            //w->IRendererRef->ResizeView(w->WindowUID, w->CurViewport.Width, w->CurViewport.Height);
            //resizeRenderer = false;
        }
		// sw->IRendererRef->Render(w->WindowUID,mCommandList,&w->RTHandles[mCurFrameIdx],w->CurScissor.right,w->CurScissor.bottom);
	}

	// Get ready to present
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(w->BackBuffers[curIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Closing  the command list and executing it with the recorded commands
	res = mCommandList->Close();
	SIMUL_ASSERT(res == S_OK);
	ID3D12CommandList* ppCommandLists[] = { mCommandList };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	static DWORD dwFlags		= 0;
	static UINT SyncInterval	= 1;									
	res = w->SwapChain->Present(SyncInterval,dwFlags);			
	SIMUL_ASSERT(res == S_OK);

	MoveToNextFrame(w);
}

void Direct3D12Manager::SetRenderer(HWND hwnd,crossplatform::PlatformRendererInterface *ci, int view_id)
{
	if(windows.find(hwnd)==windows.end())
		return;
	Window *w=windows[hwnd];
	if(!w)
		return;
	w->SetRenderer(ci,  view_id);
}

int Direct3D12Manager::GetViewId(HWND hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return -1;
	Window *w=windows[hwnd];
	return w->WindowUID;
}

Window *Direct3D12Manager::GetWindow(HWND hwnd)
{
	if(windows.find(hwnd)==windows.end())
		return NULL;
	Window *w=windows[hwnd];
	return w;
}

void Direct3D12Manager::ReportMessageFilterState()
{

}

void Direct3D12Manager::SetFullScreen(HWND hwnd,bool fullscreen,int which_output)
{
	HRESULT res = S_FALSE;
	Window *w=(Window*)GetWindow(hwnd);
	if(!w)
		return;
	IDXGIOutput *output=outputs[which_output];
	if(!w->SwapChain)
		return;
	BOOL current_fullscreen;
	res = w->SwapChain->GetFullscreenState(&current_fullscreen, NULL);
	SIMUL_ASSERT(res == S_OK);
	if((current_fullscreen==TRUE)==fullscreen)
		return;
	res = w->SwapChain->SetFullscreenState(fullscreen, NULL);
	ResizeSwapChain(hwnd);
}

void Direct3D12Manager::ResizeSwapChain(HWND hwnd)
{
    if (windows.find(hwnd) == windows.end())
    {
		return;
    }
	Window* w = windows[hwnd];
    w->MustResize = true;
}

void* Direct3D12Manager::GetDevice()
{
	return mDevice;
}

void* Direct3D12Manager::GetDeviceContext()
{ 
	return 0; 
}

void* Direct3D12Manager::GetCommandList()
{
	return mCommandList;
}

void* Direct3D12Manager::GetCommandQueue()
{
	return mCommandQueue;
}

void Direct3D12Manager::AddWindow(HWND hwnd)
{
	if(windows.find(hwnd)!=windows.end())
		return;
	Window *window	= new Window;
	windows[hwnd]=window;
	window->ConsoleWindowHandle = hwnd;

	window->SetCommandQueue(mCommandQueue);
	
	crossplatform::Output o = GetOutput(0);
	// TO-DO: VSync
	window->RestoreDeviceObjects(mDevice, false, o.numerator, o.denominator);

	// Create the command list
	mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, window->CommandAllocators[mCurFrameIdx], nullptr, SIMUL_PPV_ARGS(&mCommandList));

	mCommandList->SetName(L"Dx12CommandList");
}

void simul::dx12::Direct3D12Manager::InitialWaitForGpu()
{
	// Close the command list and execute it to begin initial copies
	mCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { mCommandList };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		mDevice->CreateFence(mFenceValues[mCurFrameIdx], D3D12_FENCE_FLAG_NONE, SIMUL_PPV_ARGS(&mFence));

		mFenceValues[mCurFrameIdx]++;

		// Create an event handle to use for frame synchronization.
		mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (mFenceEvent == nullptr)
		{
			HRESULT result = HRESULT_FROM_WIN32(GetLastError());
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForGpu();
	}
}

// Wait for pending GPU work to complete.
void Direct3D12Manager::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	mCommandQueue->Signal(mFence, mFenceValues[mCurFrameIdx]);

	// Wait until the fence has been processed.
	mFence->SetEventOnCompletion(mFenceValues[mCurFrameIdx], mFenceEvent);
	DWORD ret = WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	mFenceValues[mCurFrameIdx]++;
}

// Prepare to render the next frame.
void Direct3D12Manager::MoveToNextFrame(Window *window)
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = mFenceValues[mCurFrameIdx];
	mCommandQueue->Signal(mFence, currentFenceValue);

	// Update the frame index.
#ifdef _XBOX_ONE
	mCurFrameIdx = -1;
#else
	mCurFrameIdx = window->SwapChain->GetCurrentBackBufferIndex();
#endif 

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	DWORD ret = 0x1234F0AD;
	if (mFence->GetCompletedValue() < mFenceValues[mCurFrameIdx])
	{
		mFence->SetEventOnCompletion(mFenceValues[mCurFrameIdx], mFenceEvent);
		ret = WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	mFenceValues[mCurFrameIdx] = currentFenceValue + 1;
}