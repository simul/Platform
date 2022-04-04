#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <shellapi.h>			// For CommandLineToArgvW

#include "Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true,NULL,128);
wchar_t wszWindowClass[] = L"SampleDX12";	

#endif
#include <stdlib.h>
#include "Platform/Core/CommandLineParams.h"
using namespace platform;
using namespace core;
CommandLineParams commandLineParams;

#ifdef _MSC_VER

// from #include <Windowsx.h>
#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	switch (message)
	{
	case WM_MOUSEWHEEL:
		break;
	case WM_MOUSEMOVE:
		break;
	case WM_KEYDOWN:
		break;
	case WM_KEYUP:
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		//switch (wmId)
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	case WM_SIZE:
		break;
	case WM_PAINT:
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR    lpCmdLine,
	int       nCmdShow)
{
	wchar_t **szArgList;
	int argCount;
	szArgList = CommandLineToArgvW(GetCommandLineW(), &argCount);

	// The following disables error dialogues in the case of a crash, this is so automated testing will not hang. See http://blogs.msdn.com/b/oldnewthing/archive/2004/07/27/198410.aspx
	SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS);
	// But that doesn't work sometimes, so:
	_set_abort_behavior(0,_WRITE_ABORT_MSG);
	_set_abort_behavior(0,_CALL_REPORTFAULT);
	// And still we might get "debug assertion failed" boxes. So we do this as well:
	_set_error_mode(_OUT_TO_STDERR);

	platform::core::GetCommandLineParams(commandLineParams,argCount,(const wchar_t **)szArgList);
	if(commandLineParams.logfile_utf8.length())
		debug_buffer.setLogFile(commandLineParams.logfile_utf8.c_str());
	// Initialize the Window class:
	{
		WNDCLASSEXW wcex;
		wcex.cbSize			= sizeof(WNDCLASSEXW);
		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= WndProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInstance;
		wcex.hIcon			= 0;
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= NULL;
		wcex.lpszMenuName	= 0;
		wcex.lpszClassName	= wszWindowClass;
		wcex.hIconSm		= 0;
		RegisterClassExW(&wcex);
	}
	HWND hWnd			= nullptr;
	// Create the window:
	{
		int kOverrideWidth	= 1440;
		int kOverrideHeight	= 900;
		HINSTANCE hInst = hInstance; // Store instance handle in our global variable
		hWnd = CreateWindowW(wszWindowClass,L"Sample", WS_OVERLAPPEDWINDOW,CW_USEDEFAULT, 0, kOverrideWidth/*commandLineParams.win_w*/, kOverrideHeight/*commandLineParams.win_h*/, NULL, NULL, hInstance, NULL);
		if (!hWnd)
			return 0;
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
	}
	//simul::crossplatform::RenderDocLoader::Load();
	//simul::crossplatform::WinPixGpuCapturerLoader::Load();

	// Pass "true" to graphicsDeviceInterface to use d3d debugging etc:
	//graphicsDeviceInterface->Initialize(commandLineParams("debug"),false,false);

	//renderer=new PlatformRenderer();
	//displaySurfaceManager.Initialize(renderer->renderPlatform);

	// Create an instance of our simple renderer class defined above:
	//renderer->OnCreateDevice(graphicsDeviceInterface->GetDevice());
	//displaySurfaceManager.AddWindow(hWnd);
	//displaySurfaceManager.SetRenderer(hWnd,renderer,-1);
#ifdef SAMPLE_USE_D3D12
	//deviceManager.FlushImmediateCommandList();
#endif
	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		InvalidateRect(hWnd, NULL, TRUE);
		//if(commandLineParams.quitafterframe>0
			//&&renderer->framenumber>=commandLineParams.quitafterframe)
			//break;
	}
	//displaySurfaceManager.RemoveWindow(hWnd);
	//renderer->OnDeviceRemoved();
	//delete renderer;
	//displaySurfaceManager.Shutdown();
	//graphicsDeviceInterface->Shutdown();
	return (int)0;// msg.wParam;
}


int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR    lpCmdLine,
	int       nCmdShow)
{
	return WinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
#endif