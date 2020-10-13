#define NOMINMAX
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h> 
#include <memory.h>

#include "Platform/Core/EnvironmentVariables.h"
#ifdef SAMPLE_USE_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/Direct3D12Manager.h"
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
#include "Platform/CrossPlatform/HDRRenderer.h"
#include "Platform/CrossPlatform/SphericalHarmonics.h"
#include "Platform/CrossPlatform/View.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/CommandLineParams.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"
#include "Platform/Shaders/Sl/camera_constants.sl"
#ifdef _MSC_VER
#include "Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true,NULL,128);
#endif

#include <SDKDDKVer.h>
#include <shellapi.h>

#define STRING_OF_MACRO1(x) #x
#define STRING_OF_MACRO(x) STRING_OF_MACRO1(x)

int kOverrideWidth	= 1440;
int kOverrideHeight	= 900;

HWND hWnd			= nullptr;

using namespace simul;

//! This class manages the Device object, and makes the connection between Windows HWND's and swapchains.
//! In practice you will have your own method to do this.
#ifdef SAMPLE_USE_VULKAN
vulkan::DeviceManager deviceManager;
crossplatform::GraphicsDeviceInterface *graphicsDeviceInterface=&deviceManager;
#endif
#ifdef SAMPLE_USE_D3D12
dx12::Direct3D12Manager deviceManager;
crossplatform::GraphicsDeviceInterface *graphicsDeviceInterface=&deviceManager;
#endif
#ifdef SAMPLE_USE_D3D11
dx11::Direct3D11Manager deviceManager;
crossplatform::GraphicsDeviceInterface *graphicsDeviceInterface=&deviceManager;
#endif
#ifdef SAMPLE_USE_OPENGL
#include "OpenGL.h"
opengl::DeviceManager deviceManager;
crossplatform::GraphicsDeviceInterface *graphicsDeviceInterface=&deviceManager;


void GlfwErrorCallback(int errcode, const char* info)
{
    std::cout << "[GLFW ERROR] " << info << std::endl;
}

#endif
crossplatform::DisplaySurfaceManager displaySurfaceManager;
crossplatform::CommandLineParams commandLineParams;

//! An example of how to use simul::dx11::SimulWeatherRendererDX12 in context.
class PlatformRenderer:public crossplatform::PlatformRendererInterface
{
	//! It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
	//! distributes numerical precision to where it is better used.
	static const bool reverseDepth=true;
	//! A framebuffer to store the colour and depth textures for the view.
	crossplatform::BaseFramebuffer	*hdrFramebuffer;
	crossplatform::Texture* specularTexture = nullptr;
	crossplatform::Texture* diffuseCubemapTexture = nullptr;
	//! An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
	crossplatform::HdrRenderer		*hDRRenderer;
	
	// A simple example mesh to draw as transparent
	//crossplatform::Mesh *transparentMesh;
	crossplatform::Mesh *exampleMesh;
	crossplatform::Effect *effect;
	crossplatform::ConstantBuffer<SolidConstants> solidConstants;
	crossplatform::ConstantBuffer<CameraConstants> cameraConstants;

	//! A camera instance to generate view and proj matrices and handle mouse control.
	//! In practice you will have your own solution for this.
	crossplatform::Camera			camera;
	crossplatform::MouseCameraState	mouseCameraState;
	crossplatform::MouseCameraInput	mouseCameraInput;
	bool keydown[256];

	crossplatform::Texture *depthTexture;

public:
	int framenumber;
	//! The render platform implements the cross-platform Simul graphics API for a specific target API,
	simul::crossplatform::RenderPlatform *renderPlatform;

	PlatformRenderer():
		renderPlatform(nullptr)
		,hdrFramebuffer(NULL)
		,hDRRenderer(NULL)
		,effect(NULL)
		,framenumber(0)
	{
#ifdef SAMPLE_USE_D3D12
		renderPlatform =new dx12::RenderPlatform();
#endif
#ifdef SAMPLE_USE_D3D11
		renderPlatform =new dx11::RenderPlatform();
#endif
#ifdef SAMPLE_USE_VULKAN
		renderPlatform =new vulkan::RenderPlatform();
#endif
#ifdef SAMPLE_USE_OPENGL
		renderPlatform =new opengl::RenderPlatform();
#endif
		depthTexture=renderPlatform->CreateTexture();

		hDRRenderer		=new crossplatform::HdrRenderer();

		hdrFramebuffer=renderPlatform->CreateFramebuffer("HDR FrameBuffer");
		hdrFramebuffer->SetFormat(crossplatform::RGBA_16_FLOAT);
		hdrFramebuffer->SetDepthFormat(crossplatform::D_32_FLOAT);
		hdrFramebuffer->SetAntialiasing(1); // Set the AntiAliasing beforehand -AJR.
		// We should provide default clear values and then stick to those
		hdrFramebuffer->DefaultClearColour = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		hdrFramebuffer->DefaultClearDepth = reverseDepth ? 0.0f : 1.0f;
		hdrFramebuffer->DefaultClearStencil = 0;

		camera.SetPositionAsXYZ(0.0f,-10.0f,1.7f);
		vec3 look={0.f,1.f,0.f},up={0.f,0.f,1.f};
		camera.LookInDirection(look,up);
		camera.SetHorizontalFieldOfViewDegrees(90.f);

		// Automatic vertical fov - depends on window shape:
		camera.SetVerticalFieldOfViewDegrees(0.f);
		crossplatform::CameraViewStruct vs;
		vs.exposure			= 1.f;
		vs.farZ				= 300000.f;
		vs.nearZ			= 1.f;
		vs.gamma			= 0.44f;
		vs.InfiniteFarPlane	= false;
		vs.projection		= crossplatform::FORWARD;
		// We can leave the default camera setup in place, or change it:
#if 0
		camera.SetCameraViewStruct(vs);
#endif
		memset(keydown,0,sizeof(keydown));

		exampleMesh = renderPlatform->CreateMesh();

		// Whether run from the project directory or from the executable location, we want to be
		// able to find the shaders and textures:
		renderPlatform->PushTexturePath("");
		renderPlatform->PushTexturePath("../../../../Media/Textures");
		renderPlatform->PushTexturePath("../../Media/Textures");
		// Or from the Simul directory -e.g. by automatic builds:
#ifdef SAMPLE_USE_D3D12
		renderPlatform->PushShaderPath("Platform/DirectX12/HLSL/");
		renderPlatform->PushShaderPath("../../../../Platform/DirectX12/HLSL");
		renderPlatform->PushShaderPath("../../Platform/DirectX12/HLSL");
#endif
		renderPlatform->PushShaderPath("Platform/Shaders/SFX/");

		renderPlatform->PushTexturePath("Media/Textures");

		renderPlatform->PushShaderPath("../../../../Platform/CrossPlatform/SFX");
		renderPlatform->PushShaderPath("../../Platform/CrossPlatform/SFX");
		renderPlatform->PushShaderPath("../../../../Shaders/SFX/");
		renderPlatform->PushShaderPath("../../../../Shaders/SL/");
		renderPlatform->PushShaderPath("../../Shaders/SFX/");
		renderPlatform->PushShaderPath("../../Shaders/SL/");

		// Shader binaries: we want to use a shared common directory under Simul/Media. But if we're running from some other place, we'll just create a "shaderbin" directory.
		std::string cmake_binary_dir= STRING_OF_MACRO(CMAKE_BINARY_DIR);
		std::string cmake_source_dir = STRING_OF_MACRO(CMAKE_SOURCE_DIR);
		if(cmake_binary_dir.length())
		{
			renderPlatform->PushShaderBinaryPath(((cmake_binary_dir+ "/") + renderPlatform->GetPathName() +"/shaderbin").c_str());
			std::string platform_build_path = ((cmake_binary_dir + "/Platform/") + renderPlatform->GetPathName());
			renderPlatform->PushShaderBinaryPath((platform_build_path+"/shaderbin").c_str());
			renderPlatform->PushTexturePath((cmake_source_dir +"/Resources/Textures").c_str());
		}
		renderPlatform->PushShaderBinaryPath("shaderbin");
	}

	~PlatformRenderer()
	{	
		OnLostDevice();
		del(hDRRenderer,NULL);
		del(hdrFramebuffer,NULL);
		delete diffuseCubemapTexture;
		delete specularTexture;
		delete exampleMesh;
		delete depthTexture;
		delete renderPlatform;
	}

	// This allows live-recompile of shaders. 
	void RecompileShaders()
	{
		renderPlatform->RecompileShaders();
		hDRRenderer->RecompileShaders();
	}

	bool IsEnabled() const
	{
		return true;
	}

	void OnCreateDevice(void* pd3dDevice)
	{
#ifdef SAMPLE_USE_D3D12
		// We will provide a command list so initialization of following resource can take place
		((dx12::RenderPlatform*)renderPlatform)->SetImmediateContext((dx12::ImmediateContext*)deviceManager.GetImmediateContext());
#endif
		renderPlatform->RestoreDeviceObjects(pd3dDevice);
		exampleMesh->Initialize(renderPlatform, crossplatform::MeshType::CUBE_MESH);
		// These are for example:
		hDRRenderer->RestoreDeviceObjects(renderPlatform);
		hdrFramebuffer->RestoreDeviceObjects(renderPlatform);
		effect=renderPlatform->CreateEffect();
		effect->Load(renderPlatform,"solid");
		solidConstants.RestoreDeviceObjects(renderPlatform);
		solidConstants.LinkToEffect(effect,"SolidConstants");
		cameraConstants.RestoreDeviceObjects(renderPlatform);
	}

	// We only ever create one view in this example, but in general, this should return a new value each time it's called.
	int AddView()
	{
		static int last_view_id=0;
		// We override external_framebuffer here and pass "true" to demonstrate how external depth buffers are used.
		// In this case, we use hdrFramebuffer's depth buffer.
		return last_view_id++;
	}

	void ResizeView(int view_id,int W,int H)
    {
        kOverrideWidth  = W;
        kOverrideHeight = H;
		hDRRenderer->SetBufferSize(W,H);
		hdrFramebuffer->SetWidthAndHeight(W,H);
		hdrFramebuffer->SetAntialiasing(1);
	}
	
	void RenderTransparentTest(crossplatform::DeviceContext &deviceContext)
	{

	}

	void GenerateCubemaps(crossplatform::DeviceContext& deviceContext)
	{
		crossplatform::Texture* hdrTexture = renderPlatform->CreateTexture("Textures/abandoned_tank_farm_04_2k.hdr");
		//diffuseCubemap = renderPlatform->CreateTexture("abandoned_tank_farm_04_2k.hdr");
		delete specularTexture;
		specularTexture = renderPlatform->CreateTexture("specularTexture");
		specularTexture->ensureTextureArraySizeAndFormat(renderPlatform, 1024, 1024, 1, 8, crossplatform::PixelFormat::RGBA_16_FLOAT, true, true, true);
		// plonk the hdr into the cubemap.
		renderPlatform->LatLongTextureToCubemap(deviceContext, specularTexture, hdrTexture);
		delete hdrTexture;
		delete diffuseCubemapTexture;
		diffuseCubemapTexture = renderPlatform->CreateTexture("diffuseCubemapTexture");
		diffuseCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, 32, 32, 1, 1, crossplatform::PixelFormat::RGBA_16_FLOAT, true, true, true);

		crossplatform::SphericalHarmonics  sphericalHarmonics;

		// Now we will calculate spherical harmonics.
		sphericalHarmonics.RestoreDeviceObjects(renderPlatform);
		sphericalHarmonics.RenderMipsByRoughness(deviceContext, specularTexture);
		sphericalHarmonics.CalcSphericalHarmonics(deviceContext, specularTexture);
		// And using the harmonics, render a diffuse map:
		sphericalHarmonics.RenderEnvmap(deviceContext, diffuseCubemapTexture, -1, 0.0f);
	}

	void Render(int view_id, void* context,void* colorBuffer, int w, int h, long long frame) override
	{
		// Device context structure
		simul::crossplatform::DeviceContext	deviceContext;

		// Store back buffer, depth buffer and viewport information
        deviceContext.defaultTargetsAndViewport.num             = 1;
        deviceContext.defaultTargetsAndViewport.m_rt[0]         = colorBuffer;
        deviceContext.defaultTargetsAndViewport.rtFormats[0]    = crossplatform::UNKNOWN; //To be later defined in the pipeline
        deviceContext.defaultTargetsAndViewport.m_dt            = nullptr;
        deviceContext.defaultTargetsAndViewport.depthFormat     = crossplatform::UNKNOWN;
        deviceContext.defaultTargetsAndViewport.viewport        = { 0,0,kOverrideWidth,kOverrideHeight };
		deviceContext.frame_number					            = framenumber;
		deviceContext.platform_context				            = context;
		deviceContext.renderPlatform				            = renderPlatform;
		deviceContext.viewStruct.view_id			            = view_id;
		deviceContext.viewStruct.depthTextureStyle	            = crossplatform::PROJECTION;
		{
			deviceContext.viewStruct.view			            = camera.MakeViewMatrix();
            float aspect                                        = (float)kOverrideWidth/ (float)kOverrideHeight;
            if (reverseDepth)
            {
                deviceContext.viewStruct.proj                   = camera.MakeDepthReversedProjectionMatrix(aspect);
            }
			else
            { 
				deviceContext.viewStruct.proj		            = camera.MakeProjectionMatrix(aspect);
            }
			deviceContext.viewStruct.Init();
		}
		renderPlatform->BeginFrame(deviceContext);
		if(!diffuseCubemapTexture)
			GenerateCubemaps(deviceContext);
		// Profiling
#if DO_PROFILING 
		simul::crossplatform::SetGpuProfilingInterface(deviceContext, renderPlatform->GetGpuProfiler());
		renderPlatform->GetGpuProfiler()->SetMaxLevel(9);
		renderPlatform->GetGpuProfiler()->StartFrame(deviceContext);
#endif
		hdrFramebuffer->SetWidthAndHeight(w, h);
		hdrFramebuffer->Activate(deviceContext);
	//	hdrFramebuffer->Clear(deviceContext, 0.0f, 0.2f, 0.5f, 1.0f, reverseDepth ? 0.0f : 1.0f);
		{
			// Pre-Render Update
			static simul::core::Timer timer;
			float real_time = timer.UpdateTimeSum() / 1000.0f;
			cameraConstants.worldViewProj = deviceContext.viewStruct.viewProj;
			cameraConstants.world = mat4::identity();
			effect->SetConstantBuffer(deviceContext, &cameraConstants);
			effect->SetTexture(deviceContext,"diffuseCubemap", diffuseCubemapTexture);
			solidConstants.albedo=vec3(1.f,1.f,1.f);

			effect->SetConstantBuffer(deviceContext, &solidConstants);
			effect->Apply(deviceContext, "solid", 0);
			exampleMesh->BeginDraw(deviceContext, simul::crossplatform::ShadingMode::SHADING_MODE_SHADED);
			exampleMesh->Draw(deviceContext, 0);
			exampleMesh->EndDraw(deviceContext);
			effect->Unapply(deviceContext);
		}
		hdrFramebuffer->Deactivate(deviceContext);
		hDRRenderer->Render(deviceContext, hdrFramebuffer->GetTexture(),1.0f, 0.44f);
#if DO_PROFILING 
		renderPlatform->GetGpuProfiler()->EndFrame(deviceContext);
		renderPlatform->LinePrint(deviceContext,renderPlatform->GetGpuProfiler()->GetDebugText());
#endif
		framenumber++;
	}

	void OnLostDevice()
	{
		if(effect)
		{
			effect->InvalidateDeviceObjects();
			delete effect;
			effect=nullptr;
		}
		solidConstants.InvalidateDeviceObjects();
		cameraConstants.InvalidateDeviceObjects();
		hDRRenderer->InvalidateDeviceObjects();
		exampleMesh->InvalidateDeviceObjects();
		hdrFramebuffer->InvalidateDeviceObjects();
		renderPlatform->InvalidateDeviceObjects();
	}

	void OnDestroyDevice()
	{
		OnLostDevice();
	}

	void RemoveView(int)
	{
	}

	bool OnDeviceRemoved()
	{
		OnLostDevice();
		return true;
	}

	void OnFrameMove(double fTime,float time_step)
	{
		mouseCameraInput.forward_back_input	=(float)keydown['w']-(float)keydown['s'];
		mouseCameraInput.right_left_input	=(float)keydown['d']-(float)keydown['a'];
		mouseCameraInput.up_down_input		=(float)keydown['t']-(float)keydown['g'];
		crossplatform::UpdateMouseCamera
		(
			&camera
								,time_step
								,4.f
								,mouseCameraState
								,mouseCameraInput
			,14000.f
		);
	}

	void OnMouse(bool bLeftButtonDown
				,bool bRightButtonDown
				,bool bMiddleButtonDown
                ,int nMouseWheelDelta
                ,int xPos
				,int yPos )
	{
		mouseCameraInput.MouseButtons
			=(bLeftButtonDown?crossplatform::MouseCameraInput::LEFT_BUTTON:0)
			|(bRightButtonDown?crossplatform::MouseCameraInput::RIGHT_BUTTON:0)
			|(bMiddleButtonDown?crossplatform::MouseCameraInput::MIDDLE_BUTTON:0);
		mouseCameraInput.MouseX=xPos;
		mouseCameraInput.MouseY=yPos;
	}
	
	void OnKeyboard(unsigned wParam,bool bKeyDown)
	{
		switch (wParam) 
		{ 
			case VK_LEFT: 
			case VK_RIGHT: 
			case VK_UP: 
			case VK_DOWN:
				break;
			case 'R':
				RecompileShaders();
				break;
			default: 
				int  k=tolower(wParam);
				if(k>255)
					return;
				keydown[k]=bKeyDown?1:0;
			break; 
		}
	}
};

// Global Variables:
PlatformRenderer*			renderer	= nullptr;
HINSTANCE hInst;								// current instance
wchar_t wszWindowClass[] = L"SampleDX12";				// the main window class name

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
		if(renderer)
		{
			int xPos = GET_X_LPARAM(lParam); 
			int yPos = GET_Y_LPARAM(lParam); 
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			renderer->OnMouse((wParam&MK_LBUTTON)!=0
				,(wParam&MK_RBUTTON)!=0
				,(wParam&MK_MBUTTON)!=0
				,0,xPos,yPos);
		}
		break;
	case WM_MOUSEMOVE:
		if(renderer)
		{
			int xPos = GET_X_LPARAM(lParam); 
			int yPos = GET_Y_LPARAM(lParam); 
			renderer->OnMouse((wParam&MK_LBUTTON)!=0
				,(wParam&MK_RBUTTON)!=0
				,(wParam&MK_MBUTTON)!=0
				,0,xPos,yPos);
		}
		break;
	case WM_KEYDOWN:
		if(renderer)
			renderer->OnKeyboard((unsigned)wParam,true);
		break;
	case WM_KEYUP:
		if(renderer)
			renderer->OnKeyboard((unsigned)wParam,false);
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		//switch (wmId)
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	case WM_SIZE:
		if(renderer)
		{
			INT Width = LOWORD(lParam);
			INT Height = HIWORD(lParam);
			if(Width>8192||Height>8192||Width<0||Height<0)
				break;
			displaySurfaceManager.ResizeSwapChain(hWnd);
		}
		break;
	case WM_PAINT:
		if(renderer)
		{
			double fTime=0.0;
			float time_step=0.01f;
			renderer->OnFrameMove(fTime,time_step);
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
	wchar_t **szArgList;
	int argCount;
	szArgList = CommandLineToArgvW(GetCommandLineW(), &argCount);

#ifdef _MSC_VER
	// The following disables error dialogues in the case of a crash, this is so automated testing will not hang. See http://blogs.msdn.com/b/oldnewthing/archive/2004/07/27/198410.aspx
	SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS);
	// But that doesn't work sometimes, so:
	_set_abort_behavior(0,_WRITE_ABORT_MSG);
	_set_abort_behavior(0,_CALL_REPORTFAULT);
	// And still we might get "debug assertion failed" boxes. So we do this as well:
	_set_error_mode(_OUT_TO_STDERR);
#endif
	
	GetCommandLineParams(commandLineParams,argCount,(const wchar_t **)szArgList);
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
	// Create the window:
	{
		hInst = hInstance; // Store instance handle in our global variable
		hWnd = CreateWindowW(wszWindowClass,L"Sample", WS_OVERLAPPEDWINDOW,CW_USEDEFAULT, 0, kOverrideWidth/*commandLineParams.win_w*/, kOverrideHeight/*commandLineParams.win_h*/, NULL, NULL, hInstance, NULL);
		if (!hWnd)
			return 0;
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
	}
	// Pass "true" to graphicsDeviceInterface to use d3d debugging etc:
	graphicsDeviceInterface->Initialize(true,false,false);

	renderer=new PlatformRenderer();
	displaySurfaceManager.Initialize(renderer->renderPlatform);

	// Create an instance of our simple renderer class defined above:
	renderer->OnCreateDevice(graphicsDeviceInterface->GetDevice());
	//displaySurfaceManager.AddWindow(hWnd);
	displaySurfaceManager.SetRenderer(hWnd,renderer,-1);
#ifdef SAMPLE_USE_D3D12
    deviceManager.FlushImmediateCommandList();
#endif
	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		InvalidateRect(hWnd, NULL, TRUE);
		if(commandLineParams.quitafterframe>0&& renderer->framenumber>=commandLineParams.quitafterframe)
			break;
	}
	displaySurfaceManager.RemoveWindow(hWnd);
	renderer->OnDeviceRemoved();
	delete renderer;
	displaySurfaceManager.Shutdown();
	graphicsDeviceInterface->Shutdown();
	return (int)0;// msg.wParam;
}


int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR    lpCmdLine,
	int       nCmdShow)
{
	return WinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
