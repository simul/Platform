
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h> 
#include <memory.h>

#include "Platform/Core/EnvironmentVariables.h"
#ifdef SAMPLE_USE_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/DeviceManager.h"
#include "Platform/DirectX12/Texture.h"
#endif
#ifdef SAMPLE_USE_D3D11
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/DirectX11/DeviceManager.h"
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
#ifdef SAMPLE_USE_GLES
#include "Platform/GLES/RenderPlatform.h"
#include "Platform/GLES/DeviceManager.h"
#include "Platform/GLES/Texture.h"
#endif 
#include "Platform/CrossPlatform/RenderDocLoader.h"
#include "Platform/CrossPlatform/WinPixGpuCapturerLoader.h"
#include "Platform/CrossPlatform/HDRRenderer.h"
#include "Platform/CrossPlatform/SphericalHarmonics.h"
#include "Platform/CrossPlatform/View.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "Platform/CrossPlatform/RenderDelegater.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/CommandLineParams.h"
#include "Platform/CrossPlatform/DisplaySurfaceManager.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
#include "Platform/CrossPlatform/BaseAccelerationStructure.h"
#include "Platform/CrossPlatform/AccelerationStructureManager.h"

#include "Shaders/raytrace.sl"
#ifdef _MSC_VER
#include "Platform/Windows/VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true,NULL,128);
#endif

#include <SDKDDKVer.h>
#include <shellapi.h>
#include <random>

#define STRING_OF_MACRO1(x) #x
#define STRING_OF_MACRO(x) STRING_OF_MACRO1(x)

int kOverrideWidth	= 1440;
int kOverrideHeight	= 900;

HWND hWnd			= nullptr;

using namespace platform;

//! This class manages the Device object, and makes the connection between Windows HWND's and swapchains.
//! In practice you will have your own method to do this.
#ifdef SAMPLE_USE_VULKAN
vulkan::DeviceManager deviceManager;
crossplatform::GraphicsDeviceInterface *graphicsDeviceInterface=&deviceManager;
#endif
#ifdef SAMPLE_USE_D3D12
dx12::DeviceManager deviceManager;
crossplatform::GraphicsDeviceInterface *graphicsDeviceInterface=&deviceManager;
#endif
#ifdef SAMPLE_USE_D3D11
dx11::DeviceManager deviceManager;
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
#ifdef SAMPLE_USE_GLES
gles::DeviceManager deviceManager;
crossplatform::GraphicsDeviceInterface* graphicsDeviceInterface = &deviceManager;


void GlfwErrorCallback(int errcode, const char* info)
{
	std::cout << "[GLFW ERROR] " << info << std::endl;
}

#endif
crossplatform::DisplaySurfaceManager displaySurfaceManager;
int framenumber = 0;
//! The render platform implements the cross-platform Simul graphics API for a specific target API,
platform::crossplatform::RenderPlatform* renderPlatform = nullptr;
platform::core::CommandLineParams commandLineParams;

//! An example of how to use platform::dx11::SimulWeatherRendererDX12 in context.
class PlatformRenderer:public crossplatform::RenderDelegaterInterface
{
	//! It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
	//! distributes numerical precision to where it is better used.
	static const bool reverseDepth=true;
	//! A framebuffer to store the colour and depth textures for the view.
	crossplatform::Texture* hdrTexture =nullptr;
	crossplatform::BaseFramebuffer	*hdrFramebuffer = nullptr;
	crossplatform::Texture* specularCubemapTexture	= nullptr;
	crossplatform::Texture* diffuseCubemapTexture	= nullptr;
	//! An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
	crossplatform::HdrRenderer		*hDRRenderer	=nullptr;
	crossplatform::MeshRenderer		*meshRenderer	=nullptr;
	// An example mesh to draw.
	crossplatform::Mesh *exampleMesh				= nullptr;
	crossplatform::Mesh *environmentMesh			= nullptr;
	crossplatform::Effect *effect					= nullptr;
	crossplatform::Effect *raytrace_effect					= nullptr;
	crossplatform::ConstantBuffer<SceneConstants> sceneConstants;
	crossplatform::ConstantBuffer<CameraConstants> cameraConstants;

	// For raytracing, if available:
	crossplatform::BottomLevelAccelerationStructure		*bl_accelerationStructure = nullptr;
	crossplatform::BottomLevelAccelerationStructure		*bl_accelerationStructureExample = nullptr;
	crossplatform::TopLevelAccelerationStructure		*tl_accelerationStructure = nullptr;
	crossplatform::TopLevelAccelerationStructure		*tl_accelerationStructureExample = nullptr;
	crossplatform::AccelerationStructureManager			*accelerationStructureManager = nullptr;
	crossplatform::Texture* rtTargetTexture	= nullptr;
	crossplatform::ConstantBuffer<crossplatform::ConstantBufferWithSlot<RayGenConstantBuffer,0>> rayGenConstants;

	crossplatform::StructuredBuffer<Light> lightsStructuredBuffer;
	//! A camera instance to generate view and proj matrices and handle mouse control.
	//! In practice you will have your own solution for this.
	crossplatform::Camera			camera;
	crossplatform::MouseCameraState	mouseCameraState;
	crossplatform::MouseCameraInput	mouseCameraInput;
	bool keydown[256];
	bool show_cubemaps=false;
	bool show_textures=false;

	crossplatform::Texture *depthTexture;
	crossplatform::RenderPlatform* renderPlatform = nullptr;
public:

	PlatformRenderer(crossplatform::RenderPlatform *r)
	{
		renderPlatform = r;
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
		vs.nearZ			= 0.1f;
		vs.gamma			= 0.44f;
		vs.InfiniteFarPlane	= true;
		vs.projection		= crossplatform::DEPTH_REVERSE;
		// We can leave the default camera setup in place, or change it:
#if 1
		camera.SetCameraViewStruct(vs);
#endif
		meshRenderer=new crossplatform::MeshRenderer();
		memset(keydown,0,sizeof(keydown));

		exampleMesh = renderPlatform->CreateMesh();
		environmentMesh= renderPlatform->CreateMesh();

		bl_accelerationStructure=renderPlatform->CreateBottomLevelAccelerationStructure();
		bl_accelerationStructureExample = renderPlatform->CreateBottomLevelAccelerationStructure();
		tl_accelerationStructure=renderPlatform->CreateTopLevelAccelerationStructure();
		tl_accelerationStructureExample = renderPlatform->CreateTopLevelAccelerationStructure();
		accelerationStructureManager = renderPlatform->CreateAccelerationStructureManager();
		rtTargetTexture = renderPlatform->CreateTexture("rtTargetTexture");
		renderPlatform->SetShaderBuildMode(platform::crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);
		// Whether run from the project directory or from the executable location, we want to be
		// able to find the shaders and textures:
		renderPlatform->PushTexturePath("");
		renderPlatform->PushTexturePath("../../../../Media/Textures");
		renderPlatform->PushTexturePath("../../Media/Textures");
		renderPlatform->PushTexturePath("Media/Textures");
		renderPlatform->PushTexturePath("models");
		// Or from the Simul directory -e.g. by automatic builds:
#ifdef SAMPLE_USE_D3D12
		renderPlatform->PushShaderPath("Platform/DirectX12/HLSL/");
		renderPlatform->PushShaderPath("../../../../Platform/DirectX12/HLSL");
		renderPlatform->PushShaderPath("../../Platform/DirectX12/HLSL");
#endif
		renderPlatform->PushShaderPath("Platform/Shaders/SFX/");
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
			renderPlatform->PushShaderPath(((std::string(STRING_OF_MACRO(PLATFORM_SOURCE_DIR))+"/")+renderPlatform->GetPathName() + "/HLSL").c_str());
			renderPlatform->PushShaderBinaryPath(((cmake_binary_dir+ "/") + renderPlatform->GetPathName() +"/shaderbin").c_str());
			std::string platform_build_path = ((cmake_binary_dir + "/Platform/") + renderPlatform->GetPathName());
			renderPlatform->PushShaderBinaryPath((platform_build_path+"/shaderbin").c_str());
			renderPlatform->PushTexturePath((cmake_source_dir +"/Resources/Textures").c_str());
		}
		renderPlatform->PushShaderBinaryPath((std::string("shaderbin/")+ renderPlatform->GetPathName()).c_str());
	}
	
	~PlatformRenderer()
	{	
		OnLostDevice();
		del(hDRRenderer,NULL);
		del(hdrFramebuffer,NULL);
		delete hdrTexture;
		delete diffuseCubemapTexture;
		delete specularCubemapTexture;
		delete rtTargetTexture;
		delete tl_accelerationStructure;
		delete tl_accelerationStructureExample;
		delete bl_accelerationStructure;
		delete bl_accelerationStructureExample;
		delete exampleMesh;
		delete environmentMesh;
		delete depthTexture;
		delete renderPlatform;
	}
	void ReloadMeshes()
	{
		//exampleMesh->Initialize(renderPlatform, crossplatform::MeshType::CUBE_MESH);
		exampleMesh->Load("models/DamagedHelmet/DamagedHelmet.gltf",0.1f,crossplatform::AxesStandard::OpenGL);//Sponza/Sponza.gltf");
		environmentMesh->Load("models/Sponza/Sponza.gltf", 1.0f, crossplatform::AxesStandard::OpenGL);
	}
	// This allows live-recompile of shaders. 
	void RecompileShaders()
	{
		renderPlatform->RecompileShaders();
		hDRRenderer->RecompileShaders();
		effect->Load(renderPlatform, "solid");
		raytrace_effect->Load(renderPlatform,"raytrace");
		renderPlatform->ClearTextures();
		//ReloadMeshes();
		// Force regen of cubemaps.
		delete diffuseCubemapTexture;
		diffuseCubemapTexture=nullptr;
	}

	bool IsEnabled() const
	{
		return true;
	}

	Light lights[10];
	void OnCreateDevice(void* pd3dDevice)
	{
		renderPlatform->RestoreDeviceObjects(pd3dDevice);
		ReloadMeshes();
		rtTargetTexture->ensureTexture2DSizeAndFormat(renderPlatform,kOverrideWidth,kOverrideHeight,1,crossplatform::PixelFormat::RGBA_8_UNORM,true);
		bl_accelerationStructure->SetMesh(environmentMesh);
		bl_accelerationStructureExample->SetMesh(exampleMesh);
		math::Matrix4x4 translation = math::Matrix4x4::RotationX(3.1415926536f);
		math::Matrix4x4 translation2 = math::Matrix4x4::RotationX(3.1415926536f);

		//No direct translate function that won't overwrite or mess with current values
		translation.m30 += 0.f; //math::Matrix4x4::Translation(0.0f, 1.0f, 2.0f);
		translation.m31 += 1.f;
		translation.m32 += 2.0f;

		translation2.m30 += 0.f; //math::Matrix4x4::Translation(0.0f, 0.0f, 2.0f);
		translation2.m31 += 0.f;
		translation2.m32 += 2.0f;


		tl_accelerationStructure->SetInstanceDescs({ {bl_accelerationStructureExample, translation}, {bl_accelerationStructure, math::Matrix4x4::IdentityMatrix()} });
		tl_accelerationStructureExample->SetInstanceDescs({ {bl_accelerationStructureExample, translation2} });

		accelerationStructureManager->AddTopLevelAcclelerationStructure(tl_accelerationStructure, tl_accelerationStructure->GetID());
		accelerationStructureManager->AddTopLevelAcclelerationStructure(tl_accelerationStructureExample, tl_accelerationStructureExample->GetID());

		rayGenConstants.RestoreDeviceObjects(renderPlatform);

		// These are for example:
		hDRRenderer->RestoreDeviceObjects(renderPlatform);
		hdrFramebuffer->RestoreDeviceObjects(renderPlatform);
		meshRenderer->RestoreDeviceObjects(renderPlatform);
		effect=renderPlatform->CreateEffect();
		effect->Load(renderPlatform,"solid");
		raytrace_effect=renderPlatform->CreateEffect();
		raytrace_effect->Load(renderPlatform,"raytrace");
		sceneConstants.RestoreDeviceObjects(renderPlatform);
		sceneConstants.LinkToEffect(effect,"SolidConstants");
		cameraConstants.RestoreDeviceObjects(renderPlatform);
		lightsStructuredBuffer.RestoreDeviceObjects(renderPlatform,10);

		lights[0].colour = vec4(0.4f, 0.3f, 0.1f, 0);
		lights[0].direction = normalize(vec3(-.5f,.2f,-1.0f));
		lights[0].is_point = lights[0].is_spot = 0;

		std::random_device rd;   // non-deterministic generator
		std::mt19937 gen(rd());  // to seed mersenne twister.
		std::uniform_real_distribution<float> dist(0, 1.0f); 

		for(int i=1;i<10;i++)
		{
			lights[i].colour = 0.25f*vec4(dist(gen), dist(gen), dist(gen), 0);
			lights[i].position = vec3(12.0f*dist(gen)-6.0f, 4.0f * dist(gen) - 2.0f, 10.0f*dist(gen));
			lights[i].is_point =1.0;
			lights[i].is_spot = 0;
			lights[i].radius=1.0f+dist(gen)*1.0f;
		}
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
	void GenerateCubemaps(crossplatform::GraphicsDeviceContext& deviceContext)
	{
		if(!hdrTexture)
			hdrTexture = renderPlatform->CreateTexture("Textures/environment.hdr");

		if(!specularCubemapTexture)
		{
			specularCubemapTexture = renderPlatform->CreateTexture("specularCubemapTexture");
		}
		std::shared_ptr<std::vector<std::vector<uint8_t>>> nodata;
		specularCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, 1024, 1024, 1, 11, crossplatform::PixelFormat::RGBA_16_FLOAT,nodata, true, true, false, true);
		// plonk the hdr into the cubemap.
		renderPlatform->LatLongTextureToCubemap(deviceContext, specularCubemapTexture, hdrTexture);
		if(!diffuseCubemapTexture)
			diffuseCubemapTexture = renderPlatform->CreateTexture("diffuseCubemapTexture");
		diffuseCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, 32, 32, 1, 1, crossplatform::PixelFormat::RGBA_16_FLOAT,nodata, true, true, false, true);
	
		crossplatform::SphericalHarmonics  sphericalHarmonics;

		// Now we will calculate spherical harmonics.
		sphericalHarmonics.RestoreDeviceObjects(renderPlatform);
		sphericalHarmonics.RenderMipsByRoughness(deviceContext, specularCubemapTexture);
		sphericalHarmonics.CalcSphericalHarmonics(deviceContext, specularCubemapTexture);
		// And using the harmonics, render a diffuse map:
		sphericalHarmonics.RenderEnvmap(deviceContext, diffuseCubemapTexture, -1, 1.0f);
		//delete hdrTexture;
		//hdrTexture=nullptr;
	}

	void Render(int view_id, void* context,void* colorBuffer, int w, int h, long long frame, void* context_allocator = nullptr) override
	{
		// Device context structure
		platform::crossplatform::GraphicsDeviceContext	deviceContext;

		// Store back buffer, depth buffer and viewport information
        deviceContext.defaultTargetsAndViewport.num             = 1;
        deviceContext.defaultTargetsAndViewport.m_rt[0]         = colorBuffer;
        deviceContext.defaultTargetsAndViewport.rtFormats[0]    = crossplatform::UNKNOWN; //To be later defined in the pipeline
        deviceContext.defaultTargetsAndViewport.m_dt            = nullptr;
        deviceContext.defaultTargetsAndViewport.depthFormat     = crossplatform::UNKNOWN;
        deviceContext.defaultTargetsAndViewport.viewport        = { 0,0,w,h };
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

		// Pre-Render Update
		static platform::core::Timer timer;
		float real_time = timer.UpdateTimeSum() / 1000.0f;

		lights[0].direction.x = .05f * sin(real_time * 1.64f);
		lights[0].direction.y = .02f * sin(real_time * 1.1f);
		lights[0].direction.z = -1.0;
		lights[0].direction = normalize(lights[0].direction);

		math::Matrix4x4 translation2 = math::Matrix4x4::RotationX(3.1415926536f);
		translation2.m30 += sin(real_time); 
		translation2.m31 += 0.f;
		translation2.m32 += 2.0f;
		tl_accelerationStructureExample->SetInstanceDescs({ {bl_accelerationStructureExample, translation2} });

		accelerationStructureManager->GenerateCombinedAccelerationStructure(); // Preferably the top level super acceleration structure won't have the have its blas's updated each time, however that will require a more elegant solution
		accelerationStructureManager->BuildCombinedAccelerationStructure(deviceContext);

		//if(framenumber ==0)
		//	platform::crossplatform::RenderDocLoader::StartCapture(deviceContext.renderPlatform,(void*)hWnd);
		renderPlatform->BeginFrame();
		if(!diffuseCubemapTexture)
		{
			GenerateCubemaps(deviceContext);
		}
		// Profiling
#if DO_PROFILING 
		platform::crossplatform::SetGpuProfilingInterface(deviceContext, renderPlatform->GetGpuProfiler());
		renderPlatform->GetGpuProfiler()->SetMaxLevel(9);
		renderPlatform->GetGpuProfiler()->StartFrame(deviceContext);
#endif
		hdrFramebuffer->SetWidthAndHeight(w, h);
		hdrFramebuffer->Activate(deviceContext);
		hdrFramebuffer->Clear(deviceContext, 1.0f, 1.0f, 1.0f, 1.0f, reverseDepth ? 0.0f : 1.0f);
		{
			cameraConstants.world = deviceContext.viewStruct.model;
			cameraConstants.worldViewProj = deviceContext.viewStruct.viewProj;
			cameraConstants.invWorldViewProj=deviceContext.viewStruct.invViewProj;
			cameraConstants.view = deviceContext.viewStruct.view;
			cameraConstants.proj = deviceContext.viewStruct.proj;
			cameraConstants.viewPosition = deviceContext.viewStruct.cam_pos;
			renderPlatform->SetConstantBuffer(deviceContext, &cameraConstants);
			mat4::mul(cameraConstants.worldViewProj, cameraConstants.world, *((mat4*)(&deviceContext.viewStruct.viewProj)));
			mat4::mul(cameraConstants.modelView, cameraConstants.world, *((mat4*)(&deviceContext.viewStruct.view)));
			renderPlatform->SetConstantBuffer(deviceContext, &cameraConstants);
			
			rayGenConstants.viewport={-1.f,-1.f,1.f,1.f};
			rayGenConstants.stencil={-0.8f,-0.8f,0.8f,0.8f};
			renderPlatform->SetConstantBuffer(deviceContext,&rayGenConstants);

			auto rayPass			=raytrace_effect->GetTechniqueByIndex(0)->GetPass(0);
			auto res_scene			=raytrace_effect->GetShaderResource("Scene");
			auto res_targetTexture	=raytrace_effect->GetShaderResource("RenderTarget");
			auto res_lights			=raytrace_effect->GetShaderResource("lights");

			renderPlatform->ApplyPass(deviceContext,rayPass);
			renderPlatform->SetStructuredBuffer(deviceContext, &lightsStructuredBuffer, res_lights);
			renderPlatform->SetAccelerationStructure(deviceContext,res_scene, accelerationStructureManager->GetCombinedAccelerationStructure());
			renderPlatform->SetUnorderedAccessView(deviceContext,res_targetTexture,rtTargetTexture);
			renderPlatform->DispatchRays(deviceContext,uint3(kOverrideWidth,kOverrideHeight,1));
			renderPlatform->UnapplyPass(deviceContext);

			
		}

		{
			vec4 unity2(1.0f, 1.0f);
			vec4 unity4(1.0f, 1.0f, 1.0f, 1.0f);
			vec4 zero4(0, 0, 0, 0);
			sceneConstants.lightCount = 10;
			sceneConstants.max_roughness_mip = (float)specularCubemapTexture->mips;
			renderPlatform->SetConstantBuffer(deviceContext, &sceneConstants);

			lightsStructuredBuffer.SetData(deviceContext, lights);
			renderPlatform->SetStructuredBuffer(deviceContext, &lightsStructuredBuffer, effect->GetShaderResource("lights"));
			effect->Apply(deviceContext, "solid", 0);
			// pass raytraced rtTargetTexture as shadow.
			meshRenderer->Render(deviceContext, exampleMesh, mat4::translationColumnMajor(vec3(sin(real_time), 0.0f, 2.0f)), diffuseCubemapTexture, specularCubemapTexture, rtTargetTexture);
			meshRenderer->Render(deviceContext, exampleMesh, mat4::translationColumnMajor(vec3(0.0f, 1.0f, 2.0f)), diffuseCubemapTexture, specularCubemapTexture, rtTargetTexture);
			meshRenderer->Render(deviceContext, environmentMesh, mat4::identity(), diffuseCubemapTexture, specularCubemapTexture, rtTargetTexture);

			effect->Unapply(deviceContext);
		}

		renderPlatform->DrawTexture(deviceContext, 0, 0, 256, 256, rtTargetTexture);

		if(show_cubemaps)
		{
			float x = -.8f, m = -1.0f;
			static float r=0.15f;
			for(int m=0;m<specularCubemapTexture->mips;m++)
				renderPlatform->DrawCubemap(deviceContext, specularCubemapTexture, x += r, -.2f, .2f, 1.f, 1.f, float(m));
		}
		if(show_textures)
		{
			static int s = 128;
			auto &textures= renderPlatform->GetTextures();
			int l=(w-8)/(s+2);
			int d=int(textures.size()+l-1)/l;
			int x = 8, y = h -(s+2)*d;
			for(auto t: textures)
			{
				renderPlatform->DrawTexture(deviceContext,x,y,s,s, t.second,1.f,false,1.f,true);
				x+= s + 2;
				if(x+s>=w-8)
				{
					x=8;
					y+=s+2;
				}
			}
		}	
		hdrFramebuffer->Deactivate(deviceContext);

		hDRRenderer->Render(deviceContext, hdrFramebuffer->GetTexture(),1.0f, 0.44f);
#if DO_PROFILING 
		renderPlatform->GetGpuProfiler()->EndFrame(deviceContext);
		renderPlatform->LinePrint(deviceContext,renderPlatform->GetGpuProfiler()->GetDebugText());
#endif
		//if (framenumber == 0)
		//	platform::crossplatform::RenderDocLoader::FinishCapture();
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
		if(raytrace_effect)
		{
			raytrace_effect->InvalidateDeviceObjects();
			delete raytrace_effect;
			raytrace_effect=nullptr;
		}
		sceneConstants.InvalidateDeviceObjects();
		cameraConstants.InvalidateDeviceObjects();
		lightsStructuredBuffer.InvalidateDeviceObjects();
		hDRRenderer->InvalidateDeviceObjects();
		exampleMesh->InvalidateDeviceObjects();
		environmentMesh->InvalidateDeviceObjects();
		hdrFramebuffer->InvalidateDeviceObjects();
		meshRenderer->InvalidateDeviceObjects();
		rayGenConstants.InvalidateDeviceObjects();

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
		static float cam_speed=4.0f;

		crossplatform::UpdateMouseCamera
		(
			&camera
			,time_step
			,cam_speed
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
		if(!bKeyDown)
		switch (wParam) 
		{ 
			case VK_LEFT: 
			case VK_RIGHT: 
			case VK_UP: 
			case VK_DOWN:
				break;
			case 'C':
				show_cubemaps=!show_cubemaps;
				break;
			case 'T':
				show_textures=!show_textures;
				break;
			case 'R':
				RecompileShaders();
				break;
			default: 
			break;
		}
		int  k=tolower(wParam);
		if(k>255)
			return;
		keydown[k]=bKeyDown?1:0;
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

			renderPlatform->BeginFrame(framenumber);
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
	//platform::crossplatform::RenderDocLoader::Load();
	platform::crossplatform::WinPixGpuCapturerLoader::Load();
	
	// Pass "true" to graphicsDeviceInterface to use d3d debugging etc:
	graphicsDeviceInterface->Initialize(commandLineParams("debug"),false,false);

#ifdef SAMPLE_USE_D3D12
	renderPlatform = new dx12::RenderPlatform();
#endif
#ifdef SAMPLE_USE_D3D11
	renderPlatform = new dx11::RenderPlatform();
#endif
#ifdef SAMPLE_USE_VULKAN
	renderPlatform = new vulkan::RenderPlatform();
#endif
#ifdef SAMPLE_USE_OPENGL
	renderPlatform = new opengl::RenderPlatform();
#endif
#ifdef SAMPLE_USE_GLES
	renderPlatform = new gles::RenderPlatform();
#endif
	renderer = new PlatformRenderer(renderPlatform);
	displaySurfaceManager.Initialize(renderPlatform);

	// Create an instance of our simple renderer class defined above:
	renderer->OnCreateDevice(graphicsDeviceInterface->GetDevice());
	//displaySurfaceManager.AddWindow(hWnd);
	displaySurfaceManager.SetRenderer(renderer);
	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		InvalidateRect(hWnd, NULL, TRUE);
		if(commandLineParams.quitafterframe>0&& renderPlatform->GetFrameNumber()>=commandLineParams.quitafterframe)
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
