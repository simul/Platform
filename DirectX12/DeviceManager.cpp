#include "Platform/DirectX12/DeviceManager.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/DirectX12/SimulDirectXHeader.h"

//#define MAGIC_ENUM_RANGE_MIN 0
//#define MAGIC_ENUM_RANGE_MAX 1536
#include "Platform/External/magic_enum/include/magic_enum.hpp"

#include <iomanip>
#ifndef _GAMING_XBOX
#include <dxgidebug.h>
#endif

#pragma comment(lib,"dxguid")

using namespace platform;
using namespace dx12;

/////////////////
//DeviceManager//
/////////////////

DeviceManager::DeviceManager():
	mDevice(nullptr)
{
}

DeviceManager::~DeviceManager()
{
	Shutdown();
}

void DeviceManager::Initialize(bool use_debug, bool instrument, bool default_driver)
{
	SIMUL_COUT << "Initializing DX12 D3D12 DirectX12 manager with: \n";
	SIMUL_COUT << "-Device Debug = " << (use_debug ? "enabled" : "disabled") << std::endl;

	HRESULT res	= S_FALSE;

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
				ID3D12Debug3* debug3 = nullptr; 
				debugController->QueryInterface(SIMUL_PPV_ARGS(&debug3));
				debug3->SetEnableGPUBasedValidation(true);
				debug3->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_DISABLE_STATE_TRACKING);
			}
#ifdef __ID3D12Debug5_INTERFACE_DEFINED__
			ID3D12Debug5* debug5 = nullptr; 
			debugController->QueryInterface(SIMUL_PPV_ARGS(&debug5));
			if (debug5)
			{
				debug5->SetEnableAutoName(TRUE);
			}
#endif
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
#if defined(NTDDI_WIN10_CO) && (NTDDI_VERSION >= NTDDI_WIN10_CO)
		D3D_FEATURE_LEVEL featureLevel=D3D_FEATURE_LEVEL_12_2;
#else
		D3D_FEATURE_LEVEL featureLevel=D3D_FEATURE_LEVEL_12_1;
#endif
		while (factory->EnumAdapters1(curAdapterIdx, &hardwareAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			// Query description
			hardwareAdapter->GetDesc1(&hardwareAdapterDesc);

			// Ignore warp adapter
			if (hardwareAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				curAdapterIdx++;
				SAFE_RELEASE(hardwareAdapter);
				continue;
			}

			// Check if has the right feature level
#if defined(NTDDI_WIN10_CO) && (NTDDI_VERSION >= NTDDI_WIN10_CO)
			res = D3D12CreateDevice(hardwareAdapter, featureLevel, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(res))
			{
				adapterFound = true;
				break;
			}
			featureLevel = D3D_FEATURE_LEVEL_12_1;
#endif
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
		res = D3D12CreateDevice(hardwareAdapter, featureLevel, SIMUL_PPV_ARGS(&mDevice));
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
#if SIMUL_ENABLE_PIX
				static bool breakOnWarning = false;
#else
				static bool breakOnWarning = true;
#endif // SIMUL_ENABLE_PIX

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
						D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
						,D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE
					#if defined(NTDDI_WIN10_CO) && (NTDDI_VERSION >= NTDDI_WIN10_CO)
						,D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED
					#endif
					};
					D3D12_INFO_QUEUE_FILTER filter	= {};
					filter.DenyList.pIDList			= msgs;
					filter.DenyList.NumIDs			= _countof(msgs);
					infoQueue->AddStorageFilterEntries(&filter);
				}

#if defined(NTDDI_WIN10_CO) && (NTDDI_VERSION >= NTDDI_WIN10_CO)
				ID3D12InfoQueue1* infoQueue1 = nullptr;
				mDevice->QueryInterface(SIMUL_PPV_ARGS(&infoQueue1));
				if (infoQueue1)
				{
					auto MessageCallbackFunction = [](D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext)
					{
						std::string category	= std::string(magic_enum::enum_name<D3D12_MESSAGE_CATEGORY>(Category));
						std::string severity	= std::string(magic_enum::enum_name<D3D12_MESSAGE_SEVERITY>(Severity));
						//magic_enum is really slow on this enum and overflows the MSVC's step count for template processing in Debug builds.
						std::string id = "";//std::string(magic_enum::enum_name<D3D12_MESSAGE_ID>(ID)); 
						std::string description = pDescription;
						
						category = category.substr(category.find_last_of('_') + 1);
						severity = severity.substr(severity.find_last_of('_') + 1);

						std::string errorMessage;
						errorMessage += ("D3D12 " + severity + ": " + description + " [ " + severity + " " + category + " #" + std::to_string(ID) + ": " + id + " ]");
						if (Severity < D3D12_MESSAGE_SEVERITY_WARNING)
						{
							SIMUL_BREAK(errorMessage);
						}
					};
					infoQueue1->RegisterMessageCallback(MessageCallbackFunction, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &mCallbackCookie);
					SAFE_RELEASE(infoQueue1);
				}
#endif

				SAFE_RELEASE(infoQueue);
			}
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12Options;
		mDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12Options, sizeof(d3d12Options));
		SIMUL_ASSERT_WARN(((d3d12Options.MinPrecisionSupport & D3D12_SHADER_MIN_PRECISION_SUPPORT_16_BIT) == D3D12_SHADER_MIN_PRECISION_SUPPORT_16_BIT), "D3D12: No 16 bit precision in shaders.");

#if !defined(_GAMING_XBOX_XBOXONE)
		D3D12_FEATURE_DATA_D3D12_OPTIONS4 d3d12Options4;
		mDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &d3d12Options4, sizeof(d3d12Options4));
		SIMUL_ASSERT_WARN((bool)d3d12Options4.Native16BitShaderOpsSupported, "D3D12: No native 16 bit shaders ops.");
#endif

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
		while ((res = hardwareAdapter->EnumOutputs(outputIdx, &output)) != DXGI_ERROR_NOT_FOUND)
		{
			mOutputs[outputIdx] = output;
			SIMUL_ASSERT(SUCCEEDED (res ));
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
#endif
}

bool DeviceManager::IsActive() const
{
	return mDevice != nullptr;
}

void DeviceManager::Shutdown()
{
	if(!mDevice)
		return;

#if defined(NTDDI_WIN10_CO) && (NTDDI_VERSION >= NTDDI_WIN10_CO) && !defined(_GAMING_XBOX)
	ID3D12InfoQueue1* infoQueue1 = nullptr;
	mDevice->QueryInterface(SIMUL_PPV_ARGS(&infoQueue1));
	if (infoQueue1)
	{
		infoQueue1->UnregisterMessageCallback(mCallbackCookie);
		SAFE_RELEASE(infoQueue1);
	}
#endif

	// TO-DO: wait for the GPU to complete last work
	for(OutputMap::iterator i=mOutputs.begin();i!=mOutputs.end();i++)
	{
		SAFE_RELEASE(i->second);
	}
	mOutputs.clear();

	ReportMessageFilterState();
	
	SAFE_RELEASE(mDevice);
#ifndef _GAMING_XBOX
	IDXGIDebug1 *dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
	{
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL| DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	}
	dxgiDebug->Release();
#endif
}

void* DeviceManager::GetDevice()
{
	return mDevice;
}

void* DeviceManager::GetDeviceContext()
{ 
	return 0; 
}

int DeviceManager::GetNumOutputs()
{
	return (int)mOutputs.size();
}

crossplatform::Output DeviceManager::GetOutput(int i)
{
	crossplatform::Output o;
	
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
	
#if defined(WINVER) && !defined(_GAMING_XBOX)
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
	return o;
}

void DeviceManager::ReportMessageFilterState()
{
}