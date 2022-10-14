#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h> 
#include <memory.h>

//API Selection Marcos
#ifdef SAMPLE_USE_ALL_X64_API
#define SAMPLE_USE_D3D11 1
#define SAMPLE_USE_D3D12 1
#define SAMPLE_USE_VULKAN 1
#define SAMPLE_USE_OPENGL 1
#endif

//Platform includes
#include "Platform/Core/EnvironmentVariables.h"
#if SAMPLE_USE_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/DeviceManager.h"
#endif
#if SAMPLE_USE_D3D11
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/DirectX11/DeviceManager.h"
#endif
#if SAMPLE_USE_VULKAN
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/DeviceManager.h"
#endif
#if SAMPLE_USE_OPENGL
#include "Platform/OpenGL/RenderPlatform.h"
#include "Platform/OpenGL/DeviceManager.h"
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
#include "Platform/Core/CommandLineParams.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"
#include "Platform/Shaders/Sl/camera_constants.sl"


#ifdef _MSC_VER
#include "Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true, NULL, 128);
#endif

using namespace platform;

//Other Windows Header Files
#include <SDKDDKVer.h>
#include <shellapi.h>
#include <random>

#define STRING_OF_MACRO1(x) #x
#define STRING_OF_MACRO(x) STRING_OF_MACRO1(x)

//Per API device managers 
#if SAMPLE_USE_D3D11
dx11::DeviceManager dx11_deviceManager;
#endif
#if SAMPLE_USE_D3D12
dx12::DeviceManager dx12_deviceManager;
#endif
#if SAMPLE_USE_VULKAN
vulkan::DeviceManager vk_deviceManager;
#endif
#if SAMPLE_USE_OPENGL
opengl::DeviceManager gl_deviceManager;
#endif

crossplatform::GraphicsDeviceInterface* graphicsDeviceInterface;
crossplatform::DisplaySurfaceManager displaySurfaceManager;

platform::core::CommandLineParams commandLineParams;

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
	UNKNOWN,
	CLEAR_COLOUR,
	QUAD_COLOUR,
	TEXT,
	CHECKERBOARD,
	FIBONACCI,
	TVTESTCARD,
	MULTIVIEW
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
	crossplatform::RenderPlatform* renderPlatform = nullptr;
	crossplatform::Texture* depthTexture = nullptr;
	crossplatform::HdrRenderer* hdrRenderer = nullptr;
	crossplatform::BaseFramebuffer* hdrFramebuffer = nullptr;
	crossplatform::Effect* effect = nullptr;
	crossplatform::Effect* test = nullptr;
	crossplatform::Texture* texture = nullptr;
	crossplatform::ConstantBuffer<SceneConstants>	sceneConstants;
	crossplatform::ConstantBuffer<CameraConstants>	cameraConstants;

	//Scene Objects
	crossplatform::Camera							camera;

	//Test Object
	crossplatform::StructuredBuffer<uint>			rwSB;
	crossplatform::StructuredBuffer<vec4>			roSB;

public:
	PlatformRenderer(const crossplatform::RenderPlatformType& rpType, const TestType& tType, bool use_debug)
		:renderPlatformType(rpType), testType(tType), debug(use_debug)
	{
		//Inital RenderPlatform and RenderDoc
		//if (debug)
			crossplatform::RenderDocLoader::Load();
		
		switch (renderPlatformType)
		{
		case crossplatform::RenderPlatformType::D3D11:
		{
#if SAMPLE_USE_D3D11
			graphicsDeviceInterface = &dx11_deviceManager;
			renderPlatform = new dx11::RenderPlatform();
#endif
			break;
		}
		default:
		case crossplatform::RenderPlatformType::D3D12:
		{
#if SAMPLE_USE_D3D12
			graphicsDeviceInterface = &dx12_deviceManager;
			renderPlatform = new dx12::RenderPlatform();
#endif
			break;
		}
		case crossplatform::RenderPlatformType::Vulkan:
		{
#if SAMPLE_USE_VULKAN
			graphicsDeviceInterface = &vk_deviceManager;
			renderPlatform = new vulkan::RenderPlatform();
#endif
			break;
		}
		case crossplatform::RenderPlatformType::OpenGL:
		{
#if SAMPLE_USE_OPENGL
			graphicsDeviceInterface = &gl_deviceManager;
			renderPlatform = new opengl::RenderPlatform();
#endif
			break;
		}
		}
		graphicsDeviceInterface->Initialize(debug, false, false);
		
		//RenderPlatforn Set up
		renderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);
		renderPlatform->PushTexturePath("Textures");

		switch (renderPlatformType)
		{
		case crossplatform::RenderPlatformType::D3D11:
		{
			renderPlatform->PushShaderPath("Platform/DirectX11/Sfx/");
			renderPlatform->PushShaderPath("../../../../Platform/DirectX11/Sfx");
			renderPlatform->PushShaderPath("../../Platform/DirectX11/Sfx");
			break;
		}
		default:
		case crossplatform::RenderPlatformType::D3D12:
		{
			renderPlatform->PushShaderPath("Platform/DirectX12/Sfx/");
			renderPlatform->PushShaderPath("../../../../Platform/DirectX12/Sfx");
			renderPlatform->PushShaderPath("../../Platform/DirectX12/Sfx");
			break;
		}
		case crossplatform::RenderPlatformType::Vulkan:
		{
			renderPlatform->PushShaderPath("Platform/Vulkan/Sfx/");
			renderPlatform->PushShaderPath("../../../../Platform/Vulkan/Sfx");
			renderPlatform->PushShaderPath("../../Platform/Vulkan/Sfx");
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
			renderPlatform->PushShaderPath(((std::string(STRING_OF_MACRO(PLATFORM_SOURCE_DIR)) + "/") + renderPlatform->GetPathName() + "/Sfx").c_str());
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
		delete texture;
		delete test;
		delete effect;
		delete hdrFramebuffer;
		delete hdrRenderer;
		delete depthTexture;
		delete renderPlatform;
		graphicsDeviceInterface->Shutdown();
	}

	void OnCreateDevice()
	{
		void* device = graphicsDeviceInterface->GetDevice();
		renderPlatform->RestoreDeviceObjects(device);

		hdrRenderer->RestoreDeviceObjects(renderPlatform);
		hdrFramebuffer->RestoreDeviceObjects(renderPlatform);
		effect = renderPlatform->CreateEffect();
		effect->Load(renderPlatform, "solid");
		sceneConstants.RestoreDeviceObjects(renderPlatform);
		sceneConstants.LinkToEffect(effect, "SolidConstants");
		cameraConstants.RestoreDeviceObjects(renderPlatform);
		test = renderPlatform->CreateEffect("Test");
		texture = renderPlatform->CreateTexture();
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

	void Render(int view_id, void* context, void* colorBuffer, int w, int h, long long frame, void* context_allocator = nullptr) override
	{
		if (w * h == 0) //FramebufferGL can't deal with a viewport of {0,0,0,0}!
			return;

		// Device context structure
		platform::crossplatform::GraphicsDeviceContext	deviceContext;

		// Store back buffer, depth buffer and viewport information
		deviceContext.defaultTargetsAndViewport.num = 1;
		deviceContext.defaultTargetsAndViewport.m_rt[0] = colorBuffer;
		deviceContext.defaultTargetsAndViewport.rtFormats[0] = crossplatform::UNKNOWN; //To be later defined in the pipeline
		deviceContext.defaultTargetsAndViewport.m_dt = nullptr;
		deviceContext.defaultTargetsAndViewport.depthFormat = crossplatform::UNKNOWN;
		deviceContext.defaultTargetsAndViewport.viewport = { 0, 0, w, h };
		
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

		hdrFramebuffer->SetWidthAndHeight(w, h);
		hdrFramebuffer->Activate(deviceContext);

		switch (testType)
		{
		default:
		case TestType::CLEAR_COLOUR:
		{
			Test_ClearColour(deviceContext);
			break;
		}
		case TestType::QUAD_COLOUR:
		{
			Test_QuadColour(deviceContext, w, h);
			break;
		}
		case TestType::TEXT:
		{
			Test_Text(deviceContext, w, h);
			break;
		}
		case TestType::CHECKERBOARD:
		{
			Test_Checkerboard(deviceContext, w, h);
			break;
		}
		case TestType::FIBONACCI:
		{
			Test_Fibonacci(deviceContext);
			break;
		}
		case TestType::TVTESTCARD:
		{
			Test_TVTestCard(deviceContext, w, h);
			break;
		}
		case TestType::MULTIVIEW:
		{
			Test_Multiview(deviceContext, w, h);
			break;
		}
		}

		hdrFramebuffer->Deactivate(deviceContext);
		hdrRenderer->Render(deviceContext, hdrFramebuffer->GetTexture(), 1.0f, 0.44f);

		framenumber++;
	}

	void Test_ClearColour(crossplatform::GraphicsDeviceContext& deviceContext)
	{
		hdrFramebuffer->Clear(deviceContext, 0.00f, 0.31f, 0.57f, 1.00f, reverseDepth ? 0.0f : 1.0f);
	}

	void Test_QuadColour(crossplatform::GraphicsDeviceContext& deviceContext, int w, int h)
	{
		hdrFramebuffer->Clear(deviceContext, 0.00f, 0.31f, 0.57f, 1.00f, reverseDepth ? 0.0f : 1.0f);
		renderPlatform->GetDebugConstantBuffer().multiplier = vec4(0.0f, 0.33f, 1.0f, 1.0f);
		renderPlatform->SetConstantBuffer(deviceContext, &(renderPlatform->GetDebugConstantBuffer()));
		renderPlatform->DrawQuad(deviceContext, w / 4, h / 4, w / 2, h / 2, renderPlatform->GetDebugEffect(), renderPlatform->GetDebugEffect()->GetTechniqueByName("untextured"), "noblend");
	}

	void Test_Text(crossplatform::GraphicsDeviceContext& deviceContext, int w, int h)
	{
		int x = w / 4;
		int y = h / 4;

		std::string api = std::string("API: ") + renderPlatform->GetName();
		std::string message;
		switch (renderPlatformType)
		{
		case crossplatform::RenderPlatformType::D3D11:
			message = "All your binaries are belong to us.";
			break;
		default:
		case crossplatform::RenderPlatformType::D3D12:
			message = "It's a trap!";
			break;
		case crossplatform::RenderPlatformType::Vulkan:
			message = "You've met with a terrible fate, haven't you?"; 
			break;
		case crossplatform::RenderPlatformType::OpenGL:
			message = "It's an older code, sir, but it checks out.";
			break;
		}
		renderPlatform->Print(deviceContext, x, y, api.c_str()); y += 16;
		renderPlatform->Print(deviceContext, x, y, message.c_str()); y += 16;
	}

	void Test_Checkerboard(crossplatform::GraphicsDeviceContext& deviceContext, int w, int h)
	{
		crossplatform::EffectTechnique* checkerboard = test->GetTechniqueByName("test_checkerboard");
		crossplatform::ShaderResource res = test->GetShaderResource("rwImage");

		texture->ensureTexture2DSizeAndFormat(renderPlatform, 512, 512,1, crossplatform::PixelFormat::RGBA_8_UNORM, true);

		renderPlatform->ApplyPass(deviceContext, checkerboard->GetPass(0));
		renderPlatform->SetUnorderedAccessView(deviceContext, res, texture);
		renderPlatform->DispatchCompute(deviceContext, texture->width / 32, texture->length / 32, 1);
		renderPlatform->SetUnorderedAccessView(deviceContext, res, nullptr);
		renderPlatform->UnapplyPass(deviceContext);

		hdrFramebuffer->Clear(deviceContext, 0.5f, 0.5f, 0.5f, 1.00f, reverseDepth ? 0.0f : 1.0f);
		renderPlatform->DrawTexture(deviceContext, (w - h) / 2, 0, h, h, texture);
	}

	void Test_Fibonacci(crossplatform::GraphicsDeviceContext deviceContext)
	{
		static bool first = true;
		if (first)
		{
			rwSB.RestoreDeviceObjects(renderPlatform, 32, true, true);
			first = false;
		}

		crossplatform::EffectTechnique* fibonacci = test->GetTechniqueByName("test_fibonacci");
		crossplatform::ShaderResource res = test->GetShaderResource("rwSB");

		rwSB.ApplyAsUnorderedAccessView(deviceContext, test, res);
		renderPlatform->ApplyPass(deviceContext, fibonacci->GetPass(0));
		renderPlatform->DispatchCompute(deviceContext, 1, 1, 1);
		renderPlatform->UnapplyPass(deviceContext);
		rwSB.CopyToReadBuffer(deviceContext);

		//First buffer won't be ready as it a ring of buffers
		if (deviceContext.GetFrameNumber() < 3)
			return;

		bool pass = true;
		const uint* ptr = rwSB.OpenReadBuffer(deviceContext);
		if (!ptr)
		{
			pass = false;
		}
		if (ptr[0] != 1 && ptr[1] != 1)
		{
			pass = false;
		}
		for (int i = 2; i < rwSB.count; i++)
		{
			uint a = ptr[i - 2];
			uint b = ptr[i - 1];

			if (ptr[i] != (a + b))
			{
				pass = false;
				break;
			}
		}

		rwSB.CloseReadBuffer(deviceContext);
		vec4 colour = pass ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
		hdrFramebuffer->Clear(deviceContext, colour.x, colour.y, colour.z, colour.w, reverseDepth ? 0.0f : 1.0f);
	}

	void Test_TVTestCard(crossplatform::GraphicsDeviceContext deviceContext, int w, int h)
	{
		vec4 colours[8] = {
			vec4(1,0,0,1),
			vec4(0,1,0,1),
			vec4(0,0,1,1),
			vec4(0,1,1,1),
			vec4(1,0,1,1),
			vec4(1,1,0,1),
			vec4(1,1,1,1),
			vec4(0,0,0,0)
		};
		roSB.RestoreDeviceObjects(renderPlatform, _countof(colours), false, true, colours);
		vec4* ptr = roSB.GetBuffer(deviceContext);
		if (ptr)
			memcpy(ptr, colours, sizeof(colours));
		else
			return;

		texture->ensureTexture2DSizeAndFormat(renderPlatform, w, h, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);

		crossplatform::EffectTechnique* tvtestcard = test->GetTechniqueByName("test_tvtestcard");
		crossplatform::ShaderResource res_roSB = test->GetShaderResource("roSB");
		crossplatform::ShaderResource res_rwImage = test->GetShaderResource("rwImage");
	
		renderPlatform->ApplyPass(deviceContext, tvtestcard->GetPass(0));
		roSB.Apply(deviceContext, test, res_roSB);
		renderPlatform->SetUnorderedAccessView(deviceContext, res_rwImage, texture);
		renderPlatform->DispatchCompute(deviceContext, texture->width/32, texture->length/32, 1);
		renderPlatform->UnapplyPass(deviceContext);

		hdrFramebuffer->Clear(deviceContext, 1.0f, 0.0f, 0.0f, 1.0f, reverseDepth ? 0.0f : 1.0f);
		renderPlatform->DrawTexture(deviceContext, 0, 0, w, h, texture);
	}

	void Test_Multiview(crossplatform::GraphicsDeviceContext deviceContext, int w, int h)
	{
		crossplatform::EffectTechnique* multiview = test->GetTechniqueByName("test_multiview");
		texture->ensureTextureArraySizeAndFormat(renderPlatform, w, h, 2, 1, crossplatform::PixelFormat::RGBA_8_UNORM, false, true, false, false);

		renderPlatform->SetTopology(deviceContext, platform::crossplatform::Topology::TRIANGLESTRIP);
		renderPlatform->ApplyPass(deviceContext, multiview->GetPass(0));
		crossplatform::TargetsAndViewport targets;
		targets.num = 1;
		targets.m_rt[0] = texture->AsD3D12RenderTargetView(deviceContext, 0, 0);
		targets.textureTargets[0] = {texture, 0, 0};
		targets.viewport = { 0,0,texture->GetWidth(),texture->GetLength() };
		renderPlatform->ActivateRenderTargets(deviceContext, &targets);
		renderPlatform->Draw(deviceContext, 4, 0);
		renderPlatform->DeactivateRenderTargets(deviceContext);
		renderPlatform->UnapplyPass(deviceContext);

		hdrFramebuffer->Clear(deviceContext, 0.0f, 0.0f, 0.0f, 1.0f, reverseDepth ? 0.0f : 1.0f);
		renderPlatform->DrawTexture(deviceContext, 0, 0, w, h, texture);
	}

};
PlatformRenderer* platformRenderer;

platform::crossplatform::RenderPlatformType GetRenderPlatformTypeFromCmdLnArgs(wchar_t** szArgList, int argCount)
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
			continue;
		}
	}
	
	SIMUL_COUT << "Unknown API Type. This should be a specified on the command line arguments\n";
	SIMUL_COUT << "Defaulting to D3D12.\n";
	return crossplatform::RenderPlatformType::D3D12;
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
	return TestType::UNKNOWN;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		if (platformRenderer)
		{
			/*double fTime = 0.0;
			float time_step = 0.01f;
			renderer->OnFrameMove(fTime, time_step);*/
			displaySurfaceManager.Render(hWnd);
			displaySurfaceManager.EndFrame();
			platformRenderer->renderPlatform->EndFrame();
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

	platform::crossplatform::RenderPlatformType x64_API = GetRenderPlatformTypeFromCmdLnArgs(szArgList, argCount);

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