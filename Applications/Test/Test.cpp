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
#define GET_X_LPARAM(lp)				((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)				((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam)	((short)HIWORD(wParam))



enum class TestType
{
	CLEAR_COLOUR,
};

class PlatformRenderer : public crossplatform::PlatformRendererInterface
{
public:
	crossplatform::RenderPlatformType renderPlatformType = crossplatform::RenderPlatformType::Unknown;
	TestType testType;
	bool debug = false;
	const bool reverseDepth = true;
	int framenumber = 0;

	//Render Primitives
	crossplatform::RenderPlatform*					renderPlatform	= nullptr;
	crossplatform::Texture*							depthTexture	= nullptr;
	crossplatform::HdrRenderer*						hdrRenderer		= nullptr;
	crossplatform::BaseFramebuffer*					hdrFramebuffer	= nullptr;
	crossplatform::Effect*							effect			= nullptr;
	crossplatform::ConstantBuffer<SceneConstants>	sceneConstants;
	crossplatform::ConstantBuffer<CameraConstants>	cameraConstants;

	//Scene Objects
	crossplatform::Camera							camera;

public:
	PlatformRenderer(const crossplatform::RenderPlatformType& rpType, const TestType& tType, bool use_debug)
		:renderPlatformType(rpType), testType(tType), debug(use_debug)
	{
		//Inital RenderPlatform and RenderDoc
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
			crossplatform::RenderDocLoader::Load();

		//RenderPlatforn Set up
		renderPlatform->SetShaderBuildMode(simul::crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);
		renderPlatform->PushTexturePath("Textures");

		switch (renderPlatformType)
		{
		case crossplatform::RenderPlatformType::D3D11:
		{
			renderPlatform->PushShaderPath("Platform/DirectX11/HLSL/");
			renderPlatform->PushShaderPath("../../../../Platform/DirectX11/HLSL");
			renderPlatform->PushShaderPath("../../Platform/DirectX11/HLSL");
			break;
		}
		default:
		case crossplatform::RenderPlatformType::D3D12:
		{
			renderPlatform->PushShaderPath("Platform/DirectX12/HLSL/");
			renderPlatform->PushShaderPath("../../../../Platform/DirectX12/HLSL");
			renderPlatform->PushShaderPath("../../Platform/DirectX12/HLSL");
			break;
		}
		case crossplatform::RenderPlatformType::Vulkan:
		{
			renderPlatform->PushShaderPath("Platform/Vulkan/GLSL/");
			renderPlatform->PushShaderPath("../../../../Platform/Vulkan/GLSL");
			renderPlatform->PushShaderPath("../../Platform/Vulan/GLSL");
			break;
		}
		case crossplatform::RenderPlatformType::OpenGL:
		{
			renderPlatform->PushShaderPath("Platform/OpenGL/GLSL/");
			renderPlatform->PushShaderPath("../../../../Platform/OpenGL/GLSL");
			renderPlatform->PushShaderPath("../../Platform/OpenGL/GLSL");
			break;
		}
		}

		renderPlatform->PushShaderPath("Platform/Shaders/SFX/");
		renderPlatform->PushShaderPath("../../../../Platform/CrossPlatform/SFX");
		renderPlatform->PushShaderPath("../../Platform/CrossPlatform/SFX");
		renderPlatform->PushShaderPath("../../../../Shaders/SFX/");
		renderPlatform->PushShaderPath("../../../../Shaders/SL/");
		renderPlatform->PushShaderPath("../../Shaders/SFX/");
		renderPlatform->PushShaderPath("../../Shaders/SL/");

		// Shader binaries: we want to use a shared common directory under Simul/Media. But if we're running from some other place, we'll just create a "shaderbin" directory.
		std::string cmake_binary_dir = STRING_OF_MACRO(CMAKE_BINARY_DIR);
		std::string cmake_source_dir = STRING_OF_MACRO(CMAKE_SOURCE_DIR);
		if (cmake_binary_dir.length())
		{
			renderPlatform->PushShaderPath(((std::string(STRING_OF_MACRO(PLATFORM_SOURCE_DIR)) + "/") + renderPlatform->GetPathName() + "/HLSL").c_str());
			renderPlatform->PushShaderPath(((std::string(STRING_OF_MACRO(PLATFORM_SOURCE_DIR)) + "/") + renderPlatform->GetPathName() + "/GLSL").c_str());
			renderPlatform->PushShaderBinaryPath(((cmake_binary_dir + "/") + renderPlatform->GetPathName() + "/shaderbin").c_str());
			std::string platform_build_path = ((cmake_binary_dir + "/Platform/") + renderPlatform->GetPathName());
			renderPlatform->PushShaderBinaryPath((platform_build_path + "/shaderbin").c_str());
			renderPlatform->PushTexturePath((cmake_source_dir + "/Resources/Textures").c_str());
		}
		renderPlatform->PushShaderBinaryPath((std::string("shaderbin/") + renderPlatform->GetPathName()).c_str());

		//Set up HdrRenderer and Depth texture
		depthTexture = renderPlatform->CreateTexture("Depth-Stencil"); //Calls new
		hdrRenderer = new crossplatform::HdrRenderer();

		//Set up BaseFramebuffer
		hdrFramebuffer = renderPlatform->CreateFramebuffer(); //Calls new
		hdrFramebuffer->SetFormat(crossplatform::RGBA_16_FLOAT);
		hdrFramebuffer->SetDepthFormat(crossplatform::D_32_FLOAT);
		hdrFramebuffer->SetAntialiasing(1);
		hdrFramebuffer->DefaultClearColour = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		hdrFramebuffer->DefaultClearDepth = reverseDepth ? 0.0f : 1.0f;
		hdrFramebuffer->DefaultClearStencil = 0;

		vec3 look = { 0.0f, 1.0f, 0.0f }, up = { 0.0f, 0.0f, 1.0f };
		camera.LookInDirection(look, up);
		camera.SetHorizontalFieldOfViewDegrees(90.0f);
		camera.SetVerticalFieldOfViewDegrees(0.0f);// Automatic vertical fov - depends on window shape

		crossplatform::CameraViewStruct vs;
		vs.exposure = 1.0f;
		vs.gamma = 0.44f;
		vs.projection = reverseDepth ? crossplatform::DEPTH_REVERSE : crossplatform::FORWARD;
		vs.nearZ = 0.1f;
		vs.farZ = 300000.f;
		vs.InfiniteFarPlane = true;
		camera.SetCameraViewStruct(vs);
	}

	~PlatformRenderer()
	{
		delete hdrFramebuffer;
		delete hdrRenderer;
		delete depthTexture;
		delete renderPlatform;
		graphicsDeviceInterface->Shutdown();
	}

	void OnCreateDevice()
	{
#ifdef SAMPLE_USE_D3D12
		if (renderPlatformType == crossplatform::RenderPlatformType::D3D12)
		{
			// We will provide a command list so initialization of following resource can take place
			((dx12::RenderPlatform*)renderPlatform)->SetImmediateContext((dx12::ImmediateContext*)dx12_deviceManager.GetImmediateContext());
		}
#endif
		void* device = graphicsDeviceInterface->GetDevice();
		renderPlatform->RestoreDeviceObjects(device);

		hdrRenderer->RestoreDeviceObjects(renderPlatform);
		hdrFramebuffer->RestoreDeviceObjects(renderPlatform);
		effect = renderPlatform->CreateEffect();
		effect->Load(renderPlatform, "solid");
		sceneConstants.RestoreDeviceObjects(renderPlatform);
		sceneConstants.LinkToEffect(effect, "SolidConstants");
		cameraConstants.RestoreDeviceObjects(renderPlatform);

#ifdef SAMPLE_USE_D3D12
		if (renderPlatformType == crossplatform::RenderPlatformType::D3D12)
		{
			dx12_deviceManager.FlushImmediateCommandList();
		}
#endif
	}

	void OnLostDevice()
	{
		if (effect)
		{
			effect->InvalidateDeviceObjects();
			delete effect;
			effect = nullptr;
		}
		sceneConstants.InvalidateDeviceObjects();
		cameraConstants.InvalidateDeviceObjects();
		hdrRenderer->InvalidateDeviceObjects();
		hdrFramebuffer->InvalidateDeviceObjects();
		renderPlatform->InvalidateDeviceObjects();
	}

	void OnDestroyDevice()
	{
		OnLostDevice();
	}

	bool OnDeviceRemoved()
	{
		OnLostDevice();
		return true;
	}

	int AddView() override 
	{
		static int last_view_id = 0;
		return last_view_id++;
	};
	void RemoveView(int id) override {};
	void ResizeView(int view_id, int W, int H) override {};

	void Render(int view_id, void* context, void* colorBuffer, int w, int h, long long frame) override
	{
		// Device context structure
		simul::crossplatform::GraphicsDeviceContext	deviceContext;

		// Store back buffer, depth buffer and viewport information
		deviceContext.defaultTargetsAndViewport.num = 1;
		deviceContext.defaultTargetsAndViewport.m_rt[0] = colorBuffer;
		deviceContext.defaultTargetsAndViewport.rtFormats[0] = crossplatform::UNKNOWN; //To be later defined in the pipeline
		deviceContext.defaultTargetsAndViewport.m_dt = nullptr;
		deviceContext.defaultTargetsAndViewport.depthFormat = crossplatform::UNKNOWN;
		deviceContext.defaultTargetsAndViewport.viewport = { 0, 0, w, h };
		deviceContext.frame_number = framenumber;
		deviceContext.platform_context = context;
		deviceContext.renderPlatform = renderPlatform;
		deviceContext.viewStruct.view_id = view_id;
		deviceContext.viewStruct.depthTextureStyle = crossplatform::PROJECTION;
		{
			deviceContext.viewStruct.view = camera.MakeViewMatrix();
			float aspect = (float)kOverrideWidth / (float)kOverrideHeight;
			if (reverseDepth)
			{
				deviceContext.viewStruct.proj = camera.MakeDepthReversedProjectionMatrix(aspect);
			}
			else
			{
				deviceContext.viewStruct.proj = camera.MakeProjectionMatrix(aspect);
			}
			deviceContext.viewStruct.Init();
		}

		//Begin frame
		renderPlatform->BeginFrame(deviceContext);

		switch (testType)
		{
		default:
		case TestType::CLEAR_COLOUR:
		{
			Test_ClearColour(deviceContext, w, h);
			break;
		}
		}

		hdrRenderer->Render(deviceContext, hdrFramebuffer->GetTexture(), 1.0f, 0.44f);

		framenumber++;
	}

	void Test_ClearColour(crossplatform::GraphicsDeviceContext& deviceContext, int w, int h)
	{
		hdrFramebuffer->SetWidthAndHeight(w, h);
		hdrFramebuffer->Activate(deviceContext);
		hdrFramebuffer->Clear(deviceContext, 1.0f, 0.0f, 1.0f, 1.0f, reverseDepth ? 0.0f : 1.0f);
		hdrFramebuffer->Deactivate(deviceContext);
	}

};
PlatformRenderer* platformRenderer;

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

TestType GetTestTypeFromCmdLnArgs(wchar_t** szArgList, int argCount)
{
	bool testFound = false;
	const size_t tagSize = std::string("-X:").size();
	for (int i = 0; i < argCount; i++)
	{
		std::wstring arg = szArgList[i];
		if (arg.find(L"-t:") != std::string::npos || arg.find(L"-T:") != std::string::npos)
		{
			std::wstring value = arg.erase(0, tagSize);
			return static_cast<TestType>(_wtoi(value.c_str()));
		}
	}
	if(!testFound)
		{
			SIMUL_COUT << "Unknown TestType. This should be a specified on the command line arguments\n";
			SIMUL_COUT << "Defaulting to TestType::CLEAR_COLOUR.\n";
			return TestType::CLEAR_COLOUR;
		}
}

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
			break;*/
	case WM_PAINT:
		if (platformRenderer)
		{
			/*double fTime = 0.0;
			float time_step = 0.01f;
			renderer->OnFrameMove(fTime, time_step);*/
			displaySurfaceManager.Render(hWnd);
			displaySurfaceManager.EndFrame();
		}
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
	wchar_t** szArgList;
	int argCount;
	szArgList = CommandLineToArgvW(GetCommandLineW(), &argCount);

	simul::crossplatform::RenderPlatformType x64_API = GetRenderPlatformTypeFromCmdLnArgs(szArgList, argCount);

	TestType test = GetTestTypeFromCmdLnArgs(szArgList, argCount);

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

	platformRenderer = new PlatformRenderer(x64_API, test, commandLineParams("debug"));
	platformRenderer->OnCreateDevice();
	displaySurfaceManager.Initialize(platformRenderer->renderPlatform);
	displaySurfaceManager.SetRenderer(hWnd, platformRenderer, -1);

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
	displaySurfaceManager.RemoveWindow(hWnd);
	displaySurfaceManager.Shutdown();
	platformRenderer->OnDeviceRemoved();
	delete platformRenderer;
	return 0;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR    lpCmdLine,
	int       nCmdShow)
{
	return WinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}