#include "Platform/DirectX12/DeviceManager.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/DirectX12/SimulDirectXHeader.h"

#include <iomanip>
#ifndef _XBOX_ONE
#ifndef _GAMING_XBOX
#include <dxgidebug.h>
#endif
#endif
using namespace simul;
using namespace dx12;


#pragma comment(lib,"dxguid")

DeviceManager::DeviceManager():
	mDevice(nullptr)
{
}

DeviceManager::~DeviceManager()
{
	Shutdown();
}

bool DeviceManager::IsActive() const
{
	return mGraphicsQueue != nullptr;
}

void DeviceManager::Initialize(bool use_debug,bool instrument, bool default_driver )
{
	SIMUL_COUT << "Initializing DX12 D3D12 DirectX12 manager with: \n";
	SIMUL_COUT << "-Device Debug = " << (use_debug ? "enabled" : "disabled") << std::endl;

	HRESULT res	= S_FALSE;

#ifndef _XBOX_ONE
#ifndef _GAMING_XBOX
	// Debug layer
    UINT dxgiFactoryFlags = 0;
	if (use_debug)
	{
		ID3D12Debug* debugController = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(SIMUL_PPV_ARGS(&debugController))))
		{
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			debugController->EnableDebugLayer();
			
			// Enable GPU validation (it will report a list of errors if occurred after ExecuteCommandList())
			static bool doGPUValidation = false;
			SIMUL_COUT << "-Gpu Validation = " << (doGPUValidation ? "enabled" : "disabled") << std::endl;
			if (doGPUValidation)
			{
				ID3D12Debug3* debugController1 = nullptr;
				debugController->QueryInterface(SIMUL_PPV_ARGS(&debugController1));
				debugController1->SetEnableGPUBasedValidation(true);
				debugController1->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_DISABLE_STATE_TRACKING);
			}
		}
		ID3D12DeviceRemovedExtendedDataSettings *pDredSettings=nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDredSettings))))
		{
			// Turn on AutoBreadcrumbs and Page Fault reporting
			pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		}
		SAFE_RELEASE(debugController);
	}

	// Create a DirectX graphics interface factory.
	IDXGIFactory4* factory	= nullptr;
	res						= CreateDXGIFactory2(dxgiFactoryFlags, SIMUL_PPV_ARGS(&factory));
	SIMUL_ASSERT(res == S_OK);

	static bool mUseWarpDevice = false;
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
		D3D_FEATURE_LEVEL featureLevel=D3D_FEATURE_LEVEL_12_1;
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
			res = D3D12CreateDevice(hardwareAdapter, featureLevel, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(res))
			{
				adapterFound = true;
				break;
			}
			featureLevel = D3D_FEATURE_LEVEL_12_0;
			res = D3D12CreateDevice(hardwareAdapter, featureLevel, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(res))
			{
				adapterFound = true;
				break;
			}
			featureLevel=D3D_FEATURE_LEVEL_11_1;
			res = D3D12CreateDevice(hardwareAdapter, featureLevel, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(res))
			{
				adapterFound = true;
				break;
			}
			featureLevel=D3D_FEATURE_LEVEL_11_0;
			res = D3D12CreateDevice(hardwareAdapter, featureLevel, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(res))
			{
				adapterFound = true;
				break;
			}
			curAdapterIdx++;
		}
		res = D3D12CreateDevice(hardwareAdapter,featureLevel, SIMUL_PPV_ARGS(&mDevice));
		mDevice->SetName (L"D3D12 Device");
		SIMUL_ASSERT(res == S_OK);

		// We must create the device with debug flags if we want break on severity
		// or mute warnings
		if(use_debug)
		{
			ID3D12InfoQueue* infoQueue = nullptr;
			mDevice->QueryInterface(SIMUL_PPV_ARGS(&infoQueue));
			if(infoQueue)
			{
				// Set break on_x settings
#if SIMUL_D3D12_ENABLE_PIX
				static bool breakOnWarning = false;
#else
				static bool breakOnWarning = true;
#endif // SIMUL_D3D12_ENABLE_PIX

				SIMUL_COUT << "-Break on Warning = " << (breakOnWarning ? "enabled" : "disabled") << std::endl;
				if (breakOnWarning)
				{
                    SIMUL_COUT << "PIX does not like having breakOnWarning enabled, so disable it if using PIX. \n";

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
		}

		// Store information about the GPU
		char gpuDesc[128];
		int gpuMem = (int)(hardwareAdapterDesc.DedicatedVideoMemory / 1024 / 1024);
		size_t stringLength;
		wcstombs_s(&stringLength, gpuDesc, 128, hardwareAdapterDesc.Description, 128);

		// Log
		SIMUL_COUT << "-Adapter: " << gpuDesc << std::endl;
		SIMUL_COUT << "-Adapter memory: " << gpuMem << "(MB)" << std::endl;


		// Enumerate mOutputs(monitors)
		IDXGIOutput* output = nullptr;
		int outputIdx		= 0;
		while (hardwareAdapter->EnumOutputs(outputIdx, &output) != DXGI_ERROR_NOT_FOUND)
		{
			mOutputs[outputIdx] = output;
			SIMUL_ASSERT(res == S_OK);
			outputIdx++;
			if (outputIdx>100)
			{
				std::cerr << "Tried 100 mOutputs: no adaptor was found." << std::endl;
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
	res									= mDevice->CreateCommandQueue(&queueDesc, SIMUL_PPV_ARGS(&mGraphicsQueue));
	SIMUL_ASSERT(res == S_OK);
    mGraphicsQueue->SetName(L"Main CommandQueue");

	// Asynchronous compute:
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	res = mDevice->CreateCommandQueue(&queueDesc, SIMUL_PPV_ARGS(&m_computeCommandQueue));
	SIMUL_ASSERT(res == S_OK);
	m_computeCommandQueue->SetName(L"Aynchronous Compute CommandQueue");

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	res = mDevice->CreateCommandQueue(&queueDesc, SIMUL_PPV_ARGS(&m_copyCommandQueue));
	SIMUL_ASSERT(res == S_OK);
	m_copyCommandQueue->SetName(L"Copy CommandQueue");
	
	for (int i = 0; i < FrameCount; ++i)
	{
		mComputeContexts[i].RestoreDeviceObjects(mDevice);
		wchar_t buffer[64] = {};
		swprintf_s (buffer, L"ComputeContext CommandList %i", i);
		V_CHECK(mComputeContexts[i].ICommandList->SetName(buffer));
		swprintf_s (buffer, L"ComputeContext Allocator %i", i);
		V_CHECK(mComputeContexts[i].IAllocator->SetName(buffer));
	}
#endif
#endif
}

void D3D12ComputeContext::RestoreDeviceObjects(ID3D12DeviceType *mDevice)
{
	V_CHECK (mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,  SIMUL_PPV_ARGS(&IAllocator)));
	V_CHECK (mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, IAllocator, nullptr, SIMUL_PPV_ARGS(&ICommandList)));
	V_CHECK (mDevice->CreateFence(0, D3D12_FENCE_FLAG_SHARED, SIMUL_PPV_ARGS (&IFence)));//__uuidof(**(ppType)), IID_PPV_ARGS_Helper(ppType)
	
	
 	V_CHECK(ICommandList->Close());
}

int DeviceManager::GetNumOutputs()
{
	return (int)mOutputs.size();
}

crossplatform::Output DeviceManager::GetOutput(int i)
{
	crossplatform::Output o;
	
#ifndef _XBOX_ONE
#ifndef _GAMING_XBOX
	unsigned numModes;
	IDXGIOutput *output = mOutputs[i];

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	HRESULT result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	SIMUL_ASSERT(result==S_OK);

	DXGI_OUTPUT_DESC outputDesc;
	output->GetDesc(&outputDesc);
	o.width		=abs(outputDesc.DesktopCoordinates.right-outputDesc.DesktopCoordinates.left);
	o.height	=abs(outputDesc.DesktopCoordinates.top-outputDesc.DesktopCoordinates.bottom);

	// Now get extended information about what monitor this is:
	
#if defined(WINVER) &&!defined(_XBOX_ONE) &&!defined(_GAMING_XBOX)
	MONITORINFOEX monitor;
    monitor.cbSize = sizeof(monitor);
    if (::GetMonitorInfo(outputDesc.Monitor, &monitor) && monitor.szDevice[0])
    {
		DISPLAY_DEVICE dispDev;
        memset(&dispDev, 0, sizeof(dispDev));
		dispDev.cb = sizeof(dispDev);

       if (::EnumDisplayDevices(monitor.szDevice, 0, &dispDev, 0))
       {
#ifdef _UNICODE
           o.monitorName=platform::core::WStringToUtf8(dispDev.DeviceName);
#else
           o.monitorName=dispDev.DeviceName;
#endif
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
#endif
	return o;
}

void D3D12ComputeContext::InvalidateDeviceObjects()
{
	SAFE_RELEASE(IAllocator);
	SAFE_RELEASE(ICommandList);
	SAFE_RELEASE(IFence);
}


void DeviceManager::Shutdown()
{
	if(!mDevice)
		return;
	for (int i = 0; i < FrameCount; ++i)
	{
		mComputeContexts[i].InvalidateDeviceObjects();
	}
	// TO-DO: wait for the GPU to complete last work
	for(OutputMap::iterator i=mOutputs.begin();i!=mOutputs.end();i++)
	{
		SAFE_RELEASE(i->second);
	}
	mOutputs.clear();

	ReportMessageFilterState();
	SAFE_RELEASE(mIContext.IAllocator);
	SAFE_RELEASE(mIContext.ICommandList);
	SAFE_RELEASE(mGraphicsQueue);
	m_computeCommandQueue.Reset();
	m_copyCommandQueue.Reset();
	for(int i=0;i<FrameCount;i++)
	{
		//asyncComputeFrames[i].InvalidateDeviceObjects();
	}
	m_computeCommandQueue.Reset();
	
	SAFE_RELEASE(mDevice);
#ifndef _XBOX_ONE
#ifndef _GAMING_XBOX
	IDXGIDebug1 *dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL| DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    }
	dxgiDebug->Release();
#endif
#endif
}

void DeviceManager::ReportMessageFilterState()
{
}

void* DeviceManager::GetDevice()
{
	return mDevice;
}

void* DeviceManager::GetDeviceContext()
{ 
	return 0; 
}

void D3D12ComputeContext::StartFrame()
{
	if(active)
		return;
	V_CHECK(IAllocator->Reset());
	V_CHECK(ICommandList->Reset(IAllocator, nullptr));
	active=true;
}
void D3D12ComputeContext::EndFrame()
{
	if(!active)
		return;
	V_CHECK(ICommandList->Close());
	active=false;
}

void DeviceManager::GetComputeContext(crossplatform::DeviceContext &computeContext)
{
	//m_computeCommandQueue->Wait (asyncComputeFrames[lastFrameIndex].fence, asyncComputeFrames[lastFrameIndex].fenceValue);

	//computeContext.ApiCallCounter=0;
	
	if(mComputeContexts[computeFrame].fenceValue>0)
	{
		m_computeCommandQueue->Wait(mComputeContexts[computeFrame].IFence, mComputeContexts[computeFrame].fenceValue);
		while(mComputeContexts[computeFrame].IFence->GetCompletedValue()<mComputeContexts[computeFrame].fenceValue)
		{
		}
	}
	computeContext.completed_frame=mComputeContexts[computeFrame].fenceValue;
	mComputeContexts[computeFrame].StartFrame();
	mComputeContexts[computeFrame].fenceValue=computeContext.frame_number;
	m_computeCommandQueue->Signal(mComputeContexts[computeFrame].IFence, 0);
	computeContext.platform_context=mComputeContexts[computeFrame].ICommandList;
	computeContext.platform_context_queue=(void*)m_computeCommandQueue.Get();
}

void* DeviceManager::GetImmediateContext()
{
    if (!mIContext.ICommandList)
    {
        V_CHECK (mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, SIMUL_PPV_ARGS(&mIContext.IAllocator)));
        V_CHECK (mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mIContext.IAllocator, nullptr, SIMUL_PPV_ARGS(&mIContext.ICommandList)));
        V_CHECK (mIContext.ICommandList->Close());
        mIContext.IRecording = false;
	}
    if (mIContext.IRecording)
    {
        FlushImmediateCommandList();
    }

    mIContext.IAllocator->Reset();
    mIContext.ICommandList->Reset(mIContext.IAllocator, nullptr);
    mIContext.IRecording = true;

    return &mIContext;
}

void DeviceManager::EndAsynchronousFrame()
{
	if(computeFrame<FrameCount&&mComputeContexts[computeFrame].active)
	{
		mComputeContexts[computeFrame].EndFrame();
		ID3D12GraphicsCommandList* pCommandList = mComputeContexts[computeFrame].ICommandList;
		ID3D12CommandList *comm[]={pCommandList};
		m_computeCommandQueue->ExecuteCommandLists(1,comm);
		m_computeCommandQueue->Signal(mComputeContexts[computeFrame].IFence, mComputeContexts[computeFrame].fenceValue);
		lastFrameIndex=computeFrame;
		computeFrame++;
		if(computeFrame>=FrameCount)
			computeFrame=0;
	}
}

void DeviceManager::FlushImmediateCommandList()
{
	EndAsynchronousFrame();
    if (!mIContext.IRecording)
    {
        return;
    }
    mIContext.IRecording    = false;
    HRESULT res             = mIContext.ICommandList->Close();
    ID3D12CommandList* ppCommandLists[] = { mIContext.ICommandList };
    mGraphicsQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    
    // Wait until completed
    ID3D12Fence* pFence;
    res = mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, SIMUL_PPV_ARGS(&pFence));
    mGraphicsQueue->Signal(pFence, 64);
    // ugly spinlock wait
    while(pFence->GetCompletedValue() != 64) {}
    pFence->Release();
}

void* DeviceManager::GetCommandQueue()
{
	return mGraphicsQueue;
}




