#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h> 
#include <memory.h>

//API Selection Marcos
#ifdef SAMPLE_USE_ALL_X64_API
#define SAMPLE_USE_D3D11
#define SAMPLE_USE_D3D12
#define SAMPLE_USE_VULKAN
#define SAMPLE_USE_OPENGL
#endif

//Platform includes
#include "Platform/Core/EnvironmentVariables.h"
#ifdef SAMPLE_USE_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/DeviceManager.h"
#include "Platform/DirectX12/Texture.h"
#endif
#ifdef SAMPLE_USE_D3D11
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/DirectX11/Direct3D11Manager.h"
#include "Platform/DirectX11/Texture.h"
#endif
#ifdef SAMPLE_USE_VULKAN
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/DeviceManager.h"
#include "Platform/Vulkan/Texture.h"
#endif
#ifdef SAMPLE_USE_OPENGL
#include "Platform/OpenGL/RenderPlatform.h"
#include "Platform/OpenGL/DeviceManager.h"
#include "Platform/OpenGL/Texture.h"
#endif 
#include "Platform/CrossPlatform/RenderDocLoader.h"
#include "Platform/CrossPlatform/HDRRenderer.h"
#include "Platform/CrossPlatform/SphericalHarmonics.h"
#include "Platform/CrossPlatform/View.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/CommandLineParams.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"
#include "Platform/Shaders/Sl/camera_constants.sl"
#include "Platform/CrossPlatform/AccelerationStructure.h"

#ifdef _MSC_VER
#include "Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true, NULL, 128);
#endif

using namespace simul;

//Other Windows Header Files
#include <SDKDDKVer.h>
#include <shellapi.h>
#include <random>

#define STRING_OF_MACRO1(x) #x
#define STRING_OF_MACRO(x) STRING_OF_MACRO1(x)

//Per API device managers 
#ifdef SAMPLE_USE_D3D11
dx11::Direct3D11Manager dx11_deviceManager;
#endif
#ifdef SAMPLE_USE_D3D12
dx12::DeviceManager dx12_deviceManager;
#endif
#ifdef SAMPLE_USE_VULKAN
vulkan::DeviceManager vk_deviceManager;
#endif
#ifdef SAMPLE_USE_OPENGL
opengl::DeviceManager gl_deviceManager;
#endif

crossplatform::GraphicsDeviceInterface* graphicsDeviceInterface;
crossplatform::DisplaySurfaceManager displaySurfaceManager;

crossplatform::CommandLineParams commandLineParams;

HWND hWnd = nullptr;
HINSTANCE hInst;
wchar_t wszWindowClass[] = L"Test";
int kOverrideWidth = 1440;
int kOverrideHeight = 900;

// from #include <Windowsx.h>
#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))

class PlatformRenderer : public crossplatform::PlatformRendererInterface
{
public:
	crossplatform::RenderPlatform* renderPlatform = nullptr;
	crossplatform::RenderPlatformType renderPlatformType = crossplatform::RenderPlatformType::Unknown;
	bool debug = false;

public:
	PlatformRenderer(const crossplatform::RenderPlatformType& type, bool use_debug)
		:renderPlatformType(type), debug(use_debug)
	{
		switch (renderPlatformType)
		{
		case crossplatform::RenderPlatformType::D3D11:
		{
			graphicsDeviceInterface = &dx11_deviceManager;
			renderPlatform = new dx11::RenderPlatform();
			break;
		}
		default:
		case crossplatform::RenderPlatformType::D3D12:
		{
			graphicsDeviceInterface = &dx12_deviceManager;
			renderPlatform = new dx12::RenderPlatform();
			break;
		}
		case crossplatform::RenderPlatformType::Vulkan:
		{
			graphicsDeviceInterface = &vk_deviceManager;
			renderPlatform = new vulkan::RenderPlatform();
			break;
		}
		case crossplatform::RenderPlatformType::OpenGL:
		{
			graphicsDeviceInterface = &gl_deviceManager;
			renderPlatform = new opengl::RenderPlatform();
			break;
		}
		}

		graphicsDeviceInterface->Initialize(debug, false, false);
		if (debug)
		{
			crossplatform::RenderDocLoader::Load();
		}
	}
	~PlatformRenderer()
	{
		delete renderPlatform;
	}

	int AddView() override { return 0; };
	void RemoveView(int id) override {};
	void Render(int view_id, void* context, void* colorBuffer, int w, int h, long long frame) override {};
	void ResizeView(int view_id, int W, int H) override {};

};
PlatformRenderer* platformRenderer;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	switch (message)
	{
	/*case WM_MOUSEWHEEL:
		if (renderer)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			renderer->OnMouse((wParam & MK_LBUTTON) != 0
				, (wParam & MK_RBUTTON) != 0
				, (wParam & MK_MBUTTON) != 0
				, 0, xPos, yPos);
		}
		break;
	case WM_MOUSEMOVE:
		if (renderer)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			renderer->OnMouse((wParam & MK_LBUTTON) != 0
				, (wParam & MK_RBUTTON) != 0
				, (wParam & MK_MBUTTON) != 0
				, 0, xPos, yPos);
		}
		break;
	case WM_KEYDOWN:
		if (renderer)
			renderer->OnKeyboard((unsigned)wParam, true);
		break;
	case WM_KEYUP:
		if (renderer)
			renderer->OnKeyboard((unsigned)wParam, false);
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		//switch (wmId)
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	case WM_SIZE:
		if (renderer)
		{
			INT Width = LOWORD(lParam);
			INT Height = HIWORD(lParam);
			if (Width > 8192 || Height > 8192 || Width < 0 || Height < 0)
				break;
			displaySurfaceManager.ResizeSwapChain(hWnd);
		}
		break;
	case WM_PAINT:
		if (renderer)
		{
			double fTime = 0.0;
			float time_step = 0.01f;
			renderer->OnFrameMove(fTime, time_step);
			displaySurfaceManager.Render(hWnd);
			displaySurfaceManager.EndFrame();
		}
		break;*/
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

simul::crossplatform::RenderPlatformType GetRenderPlatformTypeFromCmdLnArgs(wchar_t** szArgList, int argCount)
{
	for (int i = 0; i < argCount; i++)
	{
		std::wstring arg = szArgList[i];
		if (arg.find(L"-D3D11") != std::wstring::npos || arg.find(L"-d3d11") != std::wstring::npos ||
			arg.find(L"-DX11") != std::wstring::npos || arg.find(L"-dx11") != std::wstring::npos)
		{
			return crossplatform::RenderPlatformType::D3D11;
		}
		else if (arg.find(L"-D3D12") != std::wstring::npos || arg.find(L"-d3d12") != std::wstring::npos ||
			arg.find(L"-DX12") != std::wstring::npos || arg.find(L"-dx12") != std::wstring::npos)
		{
			return crossplatform::RenderPlatformType::D3D12;
		}
		else if (arg.find(L"-VULKAN") != std::wstring::npos || arg.find(L"-vulkan") != std::wstring::npos ||
			arg.find(L"-VK") != std::wstring::npos || arg.find(L"-vk") != std::wstring::npos)
		{
			return crossplatform::RenderPlatformType::Vulkan;
		}
		else if (arg.find(L"-OPENGL") != std::wstring::npos || arg.find(L"-opengl") != std::wstring::npos ||
			arg.find(L"-GL") != std::wstring::npos || arg.find(L"-gl") != std::wstring::npos)
		{
			return crossplatform::RenderPlatformType::OpenGL;
		}
		else
		{
			SIMUL_COUT << "Unknown API Type. This should be a specified on the command line arguments\n";
			SIMUL_COUT << "Defaulting to D3D12.\n";
			return crossplatform::RenderPlatformType::D3D12;
		}
	}
}

int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR    lpCmdLine,
	int       nCmdShow)
{
	wchar_t** szArgList;
	int argCount;
	szArgList = CommandLineToArgvW(GetCommandLineW(), &argCount);

	simul::crossplatform::RenderPlatformType x64_API = GetRenderPlatformTypeFromCmdLnArgs(szArgList, argCount);

#ifdef _MSC_VER
	// The following disables error dialogues in the case of a crash, this is so automated testing will not hang. See http://blogs.msdn.com/b/oldnewthing/archive/2004/07/27/198410.aspx
	SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
	// But that doesn't work sometimes, so:
	_set_abort_behavior(0, _WRITE_ABORT_MSG);
	_set_abort_behavior(0, _CALL_REPORTFAULT);
	// And still we might get "debug assertion failed" boxes. So we do this as well:
	_set_error_mode(_OUT_TO_STDERR);
#endif

	GetCommandLineParams(commandLineParams, argCount, (const wchar_t**)szArgList);
	if (commandLineParams.logfile_utf8.length())
		debug_buffer.setLogFile(commandLineParams.logfile_utf8.c_str());
	// Initialize the Window class:
	{
		WNDCLASSEXW wcex;
		wcex.cbSize = sizeof(WNDCLASSEXW);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = 0;
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = NULL;
		wcex.lpszMenuName = 0;
		wcex.lpszClassName = wszWindowClass;
		wcex.hIconSm = 0;
		RegisterClassExW(&wcex);
	}
	// Create the window:
	{
		hInst = hInstance; // Store instance handle in our global variable
		hWnd = CreateWindowW(wszWindowClass, wszWindowClass, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, kOverrideWidth/*commandLineParams.win_w*/, kOverrideHeight/*commandLineParams.win_h*/, NULL, NULL, hInstance, NULL);
		if (!hWnd)
			return 0;
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
	}

	platformRenderer = new PlatformRenderer(x64_API, commandLineParams("debug"));
	displaySurfaceManager.Initialize(platformRenderer->renderPlatform);

	// Create an instance of our simple renderer class defined above:
	//platformRenderer->OnCreateDevice(graphicsDeviceInterface->GetDevice());
	//displaySurfaceManager.AddWindow(hWnd);
	//displaySurfaceManager.SetRenderer(hWnd, platformRenderer, -1);

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		InvalidateRect(hWnd, NULL, TRUE);
		//if (commandLineParams.quitafterframe > 0 && renderer->framenumber >= commandLineParams.quitafterframe)
		//	break;
	}
	delete platformRenderer;
	return (int)0;// msg.wParam;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR    lpCmdLine,
	int       nCmdShow)
{
	return WinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}