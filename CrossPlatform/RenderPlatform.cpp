#include "RenderPlatform.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/CrossPlatform/TextRenderer.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/Layout.h"
#include "Platform/CrossPlatform/Material.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/CrossPlatform/Framebuffer.h"
#include "Platform/CrossPlatform/DisplaySurface.h"
#include "Platform/CrossPlatform/BaseAccelerationStructure.h"
#include "Platform/CrossPlatform/TopLevelAccelerationStructure.h"
#include "Platform/CrossPlatform/BottomLevelAccelerationStructure.h"
#include "Platform/CrossPlatform/AccelerationStructureManager.h"
#include "Platform/CrossPlatform/ShaderBindingTable.h"
#include "Effect.h"

#if PLATFORM_STD_FILESYSTEM==0
#define SIMUL_FILESYSTEM 0
#elif PLATFORM_STD_FILESYSTEM==1
#define SIMUL_FILESYSTEM 1
#include <filesystem>
#endif

#include <algorithm>
#include <array>

using namespace std::literals;
using namespace std::string_literals;
using namespace std::literals::string_literals;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#ifdef _MSC_VER
#define __STDC_LIB_EXT1__
#endif
#include "Platform/External/stb/stb_image_write.h"
#include "Platform/Math/Float16.h"

#ifdef _MSC_VER
#define isinf !_finite
#else
#include <cmath>		// for isinf()
#endif
#include <Platform/Core/CommandLine.h>

using namespace platform;
using namespace crossplatform;

std::map<unsigned long long,std::string> RenderPlatform::ResourceMap;
std::atomic<int> RenderPlatform::numPlatforms=0;

RenderPlatform::RenderPlatform(platform::core::MemoryInterface *m)
	:mirrorY(false)
	,mirrorY2(false)
	,mirrorYText(false)
	,memoryInterface(m)
	,textured(nullptr)
	,untextured(nullptr)
	,showVolume(nullptr)
	,gpuProfiler(nullptr)
	,can_save_and_restore(true)
	,mLastFrame(-1)
	,textRenderer(nullptr)
{
	immediateContext.renderPlatform=this;
	computeContext.renderPlatform=this;
#if defined(WIN32) && !defined(_GAMING_XBOX)
	effectCompileThread = std::thread(&RenderPlatform::recompileAsync,this);
#endif
	numPlatforms++;
}
RenderPlatform::~RenderPlatform()
{
	numPlatforms--;
	recompileThreadActive=false;
	effectCompileThread.join();
	allocator.Shutdown();
	InvalidateDeviceObjects();
	delete gpuProfiler;

	for (auto i = materials.begin(); i != materials.end(); i++)
	{
		Material *mat = i->second;
		delete mat;
	}
	materials.clear();
}

void RenderPlatform::recompileAsync()
{
	while(recompileThreadActive)
	{
		if(!effectsToCompile.size())
		{
			Sleep(1000);
			std::this_thread::yield();
			continue;
		}
		std::string effect_name=effectsToCompile[0].effect_name;
		auto callback=effectsToCompile[0].callback;
		effectsToCompile.erase(effectsToCompile.begin());
		if(!effect_name.length())
			continue;
		// Do next shader compile.
		if(RecompileEffect(effect_name))
		{
			if(recompileThreadActive)
			{
				if(callback)
					callback();
			}
		}
	}
}
static std::string recompiling_effect_name;

float RenderPlatform::GetRecompileStatus(std::string &txt)
{
	if(!effectsToCompile.size())
		return 0.0f;
	txt=recompiling_effect_name;
	return (float)effectsToCompile.size();
}

static bool RewriteOutput(std::string str)
{
	std::cerr<<str.c_str();
	return true;
}


bool RenderPlatform::RecompileEffect(std::string effect_filename)
{
#if defined(WIN32) && !defined(_GAMING_XBOX)
	recompiling_effect_name=effect_filename;
	std::string filename_fx=effect_filename;
	if(filename_fx.find(".")>=filename_fx.length())
		filename_fx+=".sfx";
	auto buildMode = GetShaderBuildMode();
	if ((buildMode & crossplatform::BUILD_IF_CHANGED) == 0)
		return false;
	int index= platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(filename_fx.c_str(),GetShaderPathsUtf8());
	std::string filenameInUseUtf8=filename_fx;
	if(index==-2||index>=(int)GetShaderPathsUtf8().size())
	{
		filenameInUseUtf8=filename_fx;
		SIMUL_CERR<<"Failed to find shader source file "<<effect_filename.c_str()<<std::endl;
		recompiling_effect_name="";
		return false;
	}
	else if(index<GetShaderPathsUtf8().size())
		filenameInUseUtf8=(GetShaderPathsUtf8()[index]+"/")+filename_fx;
	std::string shaderbin = GetShaderBinaryPathsUtf8().back();
	std::string exe_dir = platform::core::GetExeDirectory();
	std::string SIMUL= std::filesystem::weakly_canonical((exe_dir + "/../../..").c_str()).generic_string();
	std::string PLATFORM= std::filesystem::weakly_canonical((exe_dir + "/../../../Platform").c_str()).generic_string();
#ifdef _MSC_VER
	std::string sfxcmd="{SIMUL}/build/bin/Release/Sfx.exe";
#else
	std::string sfxcmd="{SIMUL}/build/bin/Sfx";
	if(SIMUL=="")
		SIMUL="/home/roderick/Documents/Simul/4.2/Simul";
#endif
	std::string name=std::string(GetName());
	std::string json_file=PLATFORM+"/"s+name+"/"s+GetSfxConfigFilename();
	std::string command=sfxcmd+" -I\"{SIMUL}/..;{SIMUL};{SIMUL}/Platform/"+name+"/Sfx;{SIMUL}/Platform/CrossPlatform/Shaders\""
											" -O\""+shaderbin+"\""
												" -P\""+json_file+"\""
												" -m\"" + shaderbin + "/intermediate\" ";
	command+= std::string(" -EPLATFORM=") + PLATFORM;
	if ((buildMode & crossplatform::ALWAYS_BUILD) != 0)
		command+=" -F";
	if (platform::core::SimulInternalChecks)
		command += " -V";
	command+=" "s+filenameInUseUtf8.c_str();
	platform::core::find_and_replace(command,"{SIMUL}",SIMUL);

	platform::core::OutputDelegate cc=std::bind(&RewriteOutput,std::placeholders::_1);
	bool result= platform::core::RunCommandLine(command.c_str(),  cc);
	
	recompiling_effect_name="";
	return result;
#else
	return false;
#endif
}

void RenderPlatform::ScheduleRecompileEffects(std::vector<std::string> effect_names,std::function <void()> f)
{
	if(!effect_names.size())
		return;
	for(size_t i=0;i<effect_names.size()-1;i++)
		effectsToCompile.push_back({effect_names[i],nullptr});
	effectsToCompile.push_back({effect_names.back(),f});
}

void RenderPlatform::ScheduleRecompileEffect(std::string effect_name,std::function <void()> f) 
{
	effectsToCompile.push_back({effect_name,f});
}

bool RenderPlatform::HasRenderingFeatures(RenderingFeatures r) const
{
	return (renderingFeatures & r) == r;
}

std::string RenderPlatform::GetPathName() const
{
	std::string pathname;
	pathname=GetName();
	pathname.erase(remove_if(pathname.begin(), pathname.end(), isspace), pathname.end());
	return pathname;
}

crossplatform::ContextState *RenderPlatform::GetContextState(crossplatform::DeviceContext &deviceContext)
{
	return &deviceContext.contextState;
}

ID3D12GraphicsCommandList* RenderPlatform::AsD3D12CommandList()
{
	return nullptr;
}

ID3D12Device* RenderPlatform::AsD3D12Device()
{
	return nullptr;
}

ID3D11Device *RenderPlatform::AsD3D11Device()
{
	return nullptr;
}

vk::Device *RenderPlatform::AsVulkanDevice()
{
	return nullptr;
}
vk::Instance* RenderPlatform::AsVulkanInstance() 
{
	return nullptr;
}

GraphicsDeviceContext &RenderPlatform::GetImmediateContext()
{
	if (!immediateContext.contextState.contextActive)
	{
		//SIMUL_CERR << "Immediate context is not active.\n";
		// Reset so it is active, because we will now be executing commands on it probably.
		ResetImmediateCommandList();
	}
	return immediateContext;
}

void RenderPlatform::RestoreDeviceObjects(void*)
{
	ERRNO_BREAK
	#if SIMUL_FILESYSTEM
	if (!initializedDefaultShaderPaths)
	{
		std::string exe_dir = platform::core::GetExeDirectory();
		std::string binary_dir = std::filesystem::weakly_canonical((exe_dir + "/../..").c_str()).generic_string();
		std::string source_dir = std::filesystem::weakly_canonical((exe_dir + "/../../..").c_str()).generic_string();
		std::string render_platform = GetPathName();
		if (exe_dir.length())
		{
			std::string simul_dir = std::filesystem::weakly_canonical((exe_dir + "../../../").c_str()).generic_string();
			PushShaderPath((simul_dir + "/../").c_str());
		}
		//std::string local_shader_binary_path = "shaderbin/"s + render_platform;
		//PushShaderBinaryPath(local_shader_binary_path.c_str());
		if (binary_dir.length())
		{
			std::string shader_binary_path = binary_dir + "/"s + render_platform + "/shaderbin"s;
			std::string platform_build_path = binary_dir + "/Platform"s;
			std::string this_platform_build_path = platform_build_path+"/"s + render_platform;
			//PushShaderBinaryPath((platform_build_path+"/shaderbin/"s+render_platform).c_str());
			PushShaderBinaryPath((binary_dir + "/shaderbin/"s+render_platform).c_str());
			PushTexturePath((source_dir + "/Resources/Textures").c_str());
		}
		std::string cmake_binary_dir = PLATFORM_STRING_OF_MACRO(PLATFORM_BUILD_DIR);
		std::string cmake_source_dir = PLATFORM_STRING_OF_MACRO(CMAKE_SOURCE_DIR);
		std::string platform_source_dir = PLATFORM_STRING_OF_MACRO(PLATFORM_SOURCE_DIR);
		if (cmake_binary_dir.length())
		{
			std::string platform_build_path = ((cmake_binary_dir + "/Platform/") + GetPathName());
			//PushShaderBinaryPath(((cmake_binary_dir + "/") + GetPathName() + "/shaderbin").c_str());
			//PushShaderBinaryPath((platform_build_path + "/shaderbin").c_str());
			PushTexturePath((cmake_source_dir + "/Resources/Textures").c_str());
		}
		//PushShaderBinaryPath((std::string("shaderbin/") + GetPathName()).c_str());

		initializedDefaultShaderPaths = true;
	}
	#endif

	crossplatform::RenderStateDesc desc;
	memset(&desc,0,sizeof(desc));
	desc.type=crossplatform::BLEND;
	desc.blend.numRTs=1;
	desc.blend.RenderTarget[0].blendOperation		=BLEND_OP_NONE;
	desc.blend.RenderTarget[0].RenderTargetWriteMask=15;
	desc.blend.RenderTarget[0].SrcBlend				=crossplatform::BLEND_ONE;
	desc.blend.RenderTarget[0].DestBlend			=crossplatform::BLEND_ZERO;
	desc.blend.RenderTarget[0].SrcBlendAlpha		=crossplatform::BLEND_ONE;
	desc.blend.RenderTarget[0].DestBlendAlpha		=crossplatform::BLEND_ZERO;
	RenderState *opaque=CreateRenderState(desc);
	standardRenderStates[STANDARD_OPAQUE_BLENDING]=opaque;

	desc.blend.RenderTarget[0].blendOperation		=BLEND_OP_ADD;
	desc.blend.RenderTarget[0].SrcBlend				=crossplatform::BLEND_SRC_ALPHA;
	desc.blend.RenderTarget[0].DestBlend			=crossplatform::BLEND_INV_SRC_ALPHA;
	desc.blend.RenderTarget[0].SrcBlendAlpha		=crossplatform::BLEND_SRC_ALPHA;
	desc.blend.RenderTarget[0].DestBlendAlpha		=crossplatform::BLEND_INV_SRC_ALPHA;
	RenderState *alpha=CreateRenderState(desc);
	standardRenderStates[STANDARD_ALPHA_BLENDING]=alpha;
	
	ERRNO_BREAK
	memset(&desc,0,sizeof(desc));
	desc.type=crossplatform::DEPTH;
	desc.depth.comparison	=crossplatform::DepthComparison::DEPTH_GREATER_EQUAL;
	desc.depth.test			=true;
	desc.depth.write		=true;

	RenderState *depth_ge=CreateRenderState(desc);
	standardRenderStates[STANDARD_DEPTH_GREATER_EQUAL]=depth_ge;
	desc.depth.comparison	=crossplatform::DepthComparison::DEPTH_LESS_EQUAL;
	RenderState *depth_le=CreateRenderState(desc);
	standardRenderStates[STANDARD_DEPTH_LESS_EQUAL]=depth_le;

	desc.depth.write			=false;
	RenderState *depth_tle=CreateRenderState(desc);
	standardRenderStates[STANDARD_TEST_DEPTH_LESS_EQUAL]=depth_tle;
	
	
	desc.depth.comparison	=crossplatform::DepthComparison::DEPTH_GREATER_EQUAL;
	RenderState *depth_tge=CreateRenderState(desc);
	standardRenderStates[STANDARD_TEST_DEPTH_GREATER_EQUAL]=depth_tge;

	desc.depth.test			=false;
	RenderState *depth_no=CreateRenderState(desc);
	standardRenderStates[STANDARD_DEPTH_DISABLE]=depth_no;

	desc.type=crossplatform::RASTERIZER;
	desc.rasterizer.viewportScissor=ViewportScissor::VIEWPORT_SCISSOR_DISABLE;
	desc.rasterizer.cullFaceMode=CullFaceMode::CULL_FACE_BACK;
	desc.rasterizer.frontFace=FRONTFACE_COUNTERCLOCKWISE;
	desc.rasterizer.polygonMode=POLYGON_MODE_FILL;
	desc.rasterizer.polygonOffsetMode=POLYGON_OFFSET_DISABLE;
	standardRenderStates[STANDARD_FRONTFACE_COUNTERCLOCKWISE]=CreateRenderState(desc);
	desc.rasterizer.frontFace=FRONTFACE_CLOCKWISE;
	standardRenderStates[STANDARD_FRONTFACE_CLOCKWISE]=CreateRenderState(desc);
	
	desc.rasterizer.cullFaceMode=CullFaceMode::CULL_FACE_NONE;
	standardRenderStates[STANDARD_DOUBLE_SIDED]=CreateRenderState(desc);
	
	SAFE_DELETE(textRenderer);
	
	textRenderer=new TextRenderer;
	
	textRenderer->RestoreDeviceObjects(this);
	debugConstants.RestoreDeviceObjects(this);
	
	if(!gpuProfiler)
		gpuProfiler=CreateGpuProfiler();
	gpuProfiler->RestoreDeviceObjects(this);
	textureQueryResult.InvalidateDeviceObjects();
	textureQueryResult.RestoreDeviceObjects(this,1,true,true,nullptr,"texture query");

	for (auto i = materials.begin(); i != materials.end(); i++)
	{
		crossplatform::Material *mat = (crossplatform::Material*)(i->second);
		mat->SetEffect(solidEffect);
	}
/*	for (auto s : shaders)
	{
		s.second->Release();
		delete s.second;
	}
	shaders.clear();*/
	
	Destroy(debugEffect);
	
	debugEffect=CreateEffect("debug");

	Destroy(solidEffect);
	solidEffect=CreateEffect("solid");
	
	Destroy(copyEffect);
	copyEffect=CreateEffect("copy");
	
	Destroy(mipEffect);
	mipEffect=CreateEffect("mip");
	
	textRenderer->LoadShaders();
	
	if(debugEffect)
	{
		textured				=debugEffect->GetTechniqueByName("textured");
		untextured				=debugEffect->GetTechniqueByName("untextured");
		showVolume				=debugEffect->GetTechniqueByName("show_volume");
		draw_cubemap_sphere		=debugEffect->GetTechniqueByName("draw_cubemap_sphere");
		volumeTexture			=debugEffect->GetShaderResource("volumeTexture");
		imageTexture			=debugEffect->GetShaderResource("imageTexture");
		debugEffect_cubeTexture	=debugEffect->GetShaderResource("cubeTexture");
	}		
	debugConstants.LinkToEffect(debugEffect,"DebugConstants");
}

void RenderPlatform::InvalidateDeviceObjects()
{
	if(gpuProfiler)
		gpuProfiler->InvalidateDeviceObjects();
	for (auto e : destroyEffects)
	{
		SAFE_DELETE(e);
	}
	destroyEffects.clear();
	if(textRenderer)
		textRenderer->InvalidateDeviceObjects();
	for(std::map<StandardRenderState,RenderState*>::iterator i=standardRenderStates.begin();i!=standardRenderStates.end();i++)
		SAFE_DELETE(i->second);
	standardRenderStates.clear();
	SAFE_DELETE(textRenderer);
	debugConstants.InvalidateDeviceObjects();
	SAFE_DELETE(debugEffect);
	
	SAFE_DELETE(solidEffect);
	
	SAFE_DELETE(copyEffect);
	SAFE_DELETE(mipEffect);
	
	textured=nullptr;
	untextured=nullptr;
	showVolume=nullptr;
	textureQueryResult.InvalidateDeviceObjects();
	
	for (auto i = materials.begin(); i != materials.end(); i++)
	{
		Material *mat = i->second;
		mat->InvalidateDeviceObjects();
	}
	/*for(auto s:shaders)
	{
		s.second->Release();
		delete s.second;
	}
	shaders.clear();*/
	for(auto s:sharedSamplerStates)
	{
		s.second->InvalidateDeviceObjects();
		delete s.second;
	}
	sharedSamplerStates.clear();
	for (auto t : textures)
	{
		SAFE_DELETE(t.second);
	}
	textures.clear();
	last_begin_frame_number=0;
}

void RenderPlatform::NotifyEffectRecompiled()
{
	recompiled=true;
}

void RenderPlatform::RecompileShaders()
{
	ScheduleRecompileEffects({"debug","solid","copy","mip"},[this]{NotifyEffectRecompiled();});
	textRenderer->RecompileShaders();
}

void RenderPlatform::LoadShaders()
{
/*	for (auto s : shaders)
	{
		s.second->Release();
		delete s.second;
	}
	shaders.clear();*/
	
	Destroy(debugEffect);
	debugEffect=CreateEffect("debug");

	Destroy(solidEffect);
	solidEffect=CreateEffect("solid");
	
	Destroy(copyEffect);
	copyEffect=CreateEffect("copy");
	
	Destroy(mipEffect);
	mipEffect=CreateEffect("mip");
	
	textRenderer->LoadShaders();
	
	if(debugEffect)
	{
		textured				=debugEffect->GetTechniqueByName("textured");
		untextured				=debugEffect->GetTechniqueByName("untextured");
		showVolume				=debugEffect->GetTechniqueByName("show_volume");
		draw_cubemap_sphere		=debugEffect->GetTechniqueByName("draw_cubemap_sphere");
		volumeTexture			=debugEffect->GetShaderResource("volumeTexture");
		imageTexture			=debugEffect->GetShaderResource("imageTexture");
		debugEffect_cubeTexture	=debugEffect->GetShaderResource("cubeTexture");
	}		
	debugConstants.LinkToEffect(debugEffect,"DebugConstants");
	
}

void RenderPlatform::PushTexturePath(const char *path_utf8)
{
#if SIMUL_FILESYSTEM
	std::filesystem::path path(path_utf8);
	std::error_code ec;
	auto canonical=std::filesystem::weakly_canonical(path,ec);
	std::string str = canonical.generic_string();
	char c = str.back();
	if (c != '\\' && c != '/')
		str += '/';
	texturePathsUtf8.push_back(str);
#else
	texturePathsUtf8.push_back(path_utf8);
#endif
}

void RenderPlatform::PopTexturePath()
{ 
	texturePathsUtf8.pop_back();
}

void RenderPlatform::ActivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::TargetsAndViewport* tv)
{
	deviceContext.GetFrameBufferStack().push(tv);
	deviceContext.renderPlatform->SetViewports(deviceContext, 1, &tv->viewport);
	int4 scissor={0,0,tv->viewport.w,tv->viewport.h};
	SetScissor(deviceContext,scissor);
}

void RenderPlatform::DeactivateRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext)
{
	deviceContext.GetFrameBufferStack().pop();
	auto *tv=deviceContext.GetCurrentTargetsAndViewport();
	SetViewports(deviceContext, 1, &tv->viewport);
	int4 scissor={0,0,tv->viewport.w,tv->viewport.h};
	SetScissor(deviceContext,scissor);
}

void RenderPlatform::PushRenderTargets(GraphicsDeviceContext &deviceContext, TargetsAndViewport *tv)
{
	deviceContext.GetFrameBufferStack().push(tv);
}

void RenderPlatform::PopRenderTargets(GraphicsDeviceContext &deviceContext)
{
	deviceContext.GetFrameBufferStack().pop();
}

void RenderPlatform::LatLongTextureToCubemap(DeviceContext &deviceContext,Texture *destination,Texture *source)
{
	debugEffect->SetTexture(deviceContext,"imageTexture",source);
	debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget2DArray",destination);
	debugEffect->Apply(deviceContext,"lat_long_to_cubemap",0);
	int D=6;
	int W=(destination->width+3)/4;
	DispatchCompute(deviceContext,W,W,D);
	debugEffect->Unapply(deviceContext);
	debugEffect->UnbindTextures(deviceContext);
}

void RenderPlatform::PushShaderPath(const char *path_utf8)
{
#if SIMUL_FILESYSTEM
	std::string dir = std::filesystem::weakly_canonical(path_utf8).generic_string();
	char c = dir.back();
	if (c != '\\' && c != '/')
		dir += '/';
	shaderPathsUtf8.push_back(dir+"/");
#else
	shaderPathsUtf8.push_back(path_utf8);
#endif
}
void RenderPlatform::PushShaderBinaryPath(const char* path_utf8)
{
#if SIMUL_FILESYSTEM
	std::string str = std::filesystem::weakly_canonical(path_utf8).generic_string();
	char c = str.back();
	if (c != '\\' && c != '/')
		str += '/';
	shaderBinaryPathsUtf8.push_back(str);
//	SIMUL_COUT << "Shader binary path: " << str.c_str() << std::endl;
#else
	shaderBinaryPathsUtf8.push_back(path_utf8);
#endif
}

std::vector<std::string> RenderPlatform::GetShaderPathsUtf8()
{
	return shaderPathsUtf8;
}
void RenderPlatform::SetShaderPathsUtf8(const std::vector<std::string> &pathsUtf8)
{
	shaderPathsUtf8.clear();
	shaderPathsUtf8=pathsUtf8;
	initializedDefaultShaderPaths = false;
}

void RenderPlatform::PopShaderPath()
{
	shaderPathsUtf8.pop_back();
}

void RenderPlatform::PopShaderBinaryPath()
{
	shaderBinaryPathsUtf8.pop_back();
}

std::vector<std::string> RenderPlatform::GetShaderBinaryPathsUtf8()
{
	return shaderBinaryPathsUtf8;
}

void RenderPlatform::SetShaderBuildMode			(ShaderBuildMode s)
{
	shaderBuildMode=s;
}

ShaderBuildMode RenderPlatform::GetShaderBuildMode() const
{
	return shaderBuildMode;
}

void RenderPlatform::BeginEvent			(DeviceContext &,const char *name){}

void RenderPlatform::EndEvent			(DeviceContext &){}

void RenderPlatform::EnsureContextFrameHasBegun(DeviceContext& deviceContext)
{
	if (frameNumber != deviceContext.GetFrameNumber())
	{
		// Call start render at least once per frame to make sure the bins 
		// release objects!
		if(deviceContext.AsGraphicsDeviceContext())
			ContextFrameBegin(*deviceContext.AsGraphicsDeviceContext());

		deviceContext.SetFrameNumber(frameNumber);
	}
}
void RenderPlatform::ContextFrameBegin(GraphicsDeviceContext &deviceContext)
{
	if (!frame_started)
		BeginFrame();
	if(gpuProfiler && !gpuProfileFrameStarted)
	{
		gpuProfiler->StartFrame(deviceContext);
		gpuProfileFrameStarted = true;
	}
	// Note: we generate the mips first because for some reason, doing that in the same frame
	// as loading a texture SOMETIMES in D3D12, cause the mip to not generate.
	FinishGeneratingTextureMips(deviceContext);
	FinishLoadingTextures(deviceContext);
	allocator.CheckForReleases();
	last_begin_frame_number=GetFrameNumber();
	deviceContext.SetFrameNumber(last_begin_frame_number);
} 

void RenderPlatform::EndFrame()
{
	if (!frame_started)
	{
		SIMUL_CERR<<"EndFrame(): frame had not started.\n";
	}
	frame_started = false;
}

void RenderPlatform::BeginFrame()
{
	if (frame_started)
	{
		SIMUL_BREAK("BeginFrame(): frame had already started.");
	}
	frameNumber++;
	frame_started = true;
	if(recompiled)
	{
		LoadShaders();
		recompiled=false;
	}
}

bool RenderPlatform::FrameStarted() const
{
	return frame_started;
}

long long RenderPlatform::GetFrameNumber() const
{
	return frameNumber;
}

void RenderPlatform::DispatchComputeAuto(DeviceContext &deviceContext,int3 d)
{
	int3 threads=deviceContext.contextState.currentEffectPass->numThreads;
	d += int3(threads.x - 1, threads.y - 1, threads.z - 1);
	d /= threads;
	DispatchCompute(deviceContext, d.x, d.y, d.z);
}

void RenderPlatform::BeginFrame(long long f)
{
	if (f==frameNumber)
	{
		SIMUL_BREAK("BeginFrame(long long): frame already at this number.");
	}
	BeginFrame();
	frameNumber = f;
}

void RenderPlatform::Clear(GraphicsDeviceContext &deviceContext,vec4 colour_rgba)
{
	crossplatform::EffectTechnique *clearTechnique=clearTechnique=debugEffect->GetTechniqueByName("clear");
	if (deviceContext.AsMultiviewGraphicsDeviceContext())
		clearTechnique=clearTechnique=debugEffect->GetTechniqueByName("clear_multiview");

	debugConstants.debugColour=colour_rgba;
	//if(debugConstants.debugColour.x+debugConstants.debugColour.y+debugConstants.debugColour.z==0)
	//{
	//	SIMUL_BREAK("");
	//}
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,clearTechnique,0);
	DrawQuad(deviceContext);
	debugEffect->Unapply(deviceContext);
}

void RenderPlatform::ClearTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,const vec4& colour)
{
	// Silently return if not initialized
	if(!texture->IsValid())
		return;
	bool cleared				= false;
	debugConstants.debugColour=colour;
	debugConstants.texSize=uint4(texture->width,texture->length,texture->depth,1);
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	// Clear the texture: how we do this depends on what kind of texture it is.
	// Does it have rendertargets? We can clear each of these in turn.
	auto *graphicsDeviceContext=deviceContext.AsGraphicsDeviceContext();
	if(texture->HasRenderTargets()&&texture->arraySize&&graphicsDeviceContext!=nullptr)
	{
		int total_num=texture->arraySize*(texture->IsCubemap()?6:1);
		for(int i=0;i<total_num;i++)
		{
			int w=texture->width;
			int l=texture->length;
			int d=texture->depth;
			for(int j=0;j<texture->mips;j++)
			{
				debugConstants.texSize=uint4(w,l,d,1);
				debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
				platform::crossplatform::SubresourceRange rng={TextureAspectFlags::COLOUR, j, 1, i, 1 };
				ShaderResourceType tp=texture->GetShaderResourceTypeForRTVAndDSV();
				platform::crossplatform::TextureView tv={tp,rng};
				texture->activateRenderTarget(*graphicsDeviceContext, tv);
				debugEffect->Apply(*graphicsDeviceContext,"clear",0);
					DrawQuad(*graphicsDeviceContext);
				debugEffect->Unapply(*graphicsDeviceContext);
				texture->deactivateRenderTarget(*graphicsDeviceContext);
				w=(w+1)/2;
				l=(l+1)/2;
				d=(d+1)/2;
			}
		}
		debugEffect->UnbindTextures(deviceContext);
		cleared = true;
	}
	// Otherwise, is it computable? We can set the colour value with a compute shader.
	// Is it mappable? We can set the colour from CPU memory.
	else if (texture->IsComputable() && !cleared)
	{
		int a=texture->NumFaces();
		if(a==0)
			a=1;
#if 1
		for(int i=0;i<a;i++)
		{
			int w=texture->width;
			int l=texture->length;
			int d=texture->depth;
			for(int j=0;j<texture->mips;j++)
			{
				const char* techname = nullptr;
				int W=(w+4-1)/4;
				int L=(l+4-1)/4;
				int D=(d+4-1)/4;
		
				if(texture->dim==2)
				{
					W=(w+8-1)/8;
					L=(l+8-1)/8;
					D=1;
					if (a == 1)
					{
						techname = "compute_clear";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget", texture, { TextureAspectFlags::COLOUR, j, i, 1 });
					}
					else
					{
						techname = "compute_clear_2d_array";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget2DArray", texture, { TextureAspectFlags::COLOUR,  j, i, 1 });
					}
				}
				else if(texture->dim==3)
				{
					if(texture->GetFormat()==PixelFormat::RGBA_8_UNORM||texture->GetFormat()==PixelFormat::RGBA_8_UNORM_SRGB||texture->GetFormat()==PixelFormat::BGRA_8_UNORM)
					{
						techname = "compute_clear_3d_u8";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget3DU8", texture, { TextureAspectFlags::COLOUR, j, 0, 1 });
					}
					else
					{
						techname="compute_clear_3d";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget3D", texture, { TextureAspectFlags::COLOUR, j, 0, 1 });
					}
				}
				else
				{
					SIMUL_BREAK_ONCE("Can't clear texture dim.");
				}
				debugConstants.texSize=uint4(w,l,d,1);
				debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
				debugEffect->Apply(deviceContext,techname,0);
				DispatchCompute(deviceContext,W,L,D);
				debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget",nullptr);
				debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget3D",nullptr);
				w=(w+1)/2;
				l=(l+1)/2;
				d=(d+1)/2;
				debugEffect->Unapply(deviceContext);
			}
		}
#endif
		debugEffect->UnbindTextures(deviceContext);
	}
	//Finally, is the texture a depth stencil? In this case, we call the specified API's clear function in order to clear it.
	else if (texture->IsDepthStencil() && !cleared&&graphicsDeviceContext!=nullptr)
	{
		texture->ClearDepthStencil(*graphicsDeviceContext, colour.x, 0);
	}
	else
	{
		SIMUL_CERR_ONCE<<("No method was found to clear this texture.\n");
	}
}
#include "Platform/Core/StringFunctions.h"
void RenderPlatform::GenerateMips(GraphicsDeviceContext &deviceContext,Texture *t,bool wrap,int array_idx)
{
	if(!t||!t->IsValid())
		return;
	auto _imageTexture=mipEffect->GetShaderResource("inputTexture");
	auto pass=mipEffect->GetTechniqueByName("mip")->GetPass("mip");
	ApplyPass(deviceContext,pass);
	vec4 white(1.0, 1.0, 1.0, 1.0);
	vec4 semiblack(0, 0, 0, 0.5);
	for(int i=0;i<t->mips-1;i++)
	{
		int m1=i+1;
		t->activateRenderTarget(deviceContext, { t->GetShaderResourceTypeForRTVAndDSV(), { TextureAspectFlags::COLOUR, m1, 1, array_idx, 1 } });
		SetTexture(deviceContext, _imageTexture, t, { TextureAspectFlags::COLOUR,  0, 1, array_idx, 1 });
		DrawQuad(deviceContext);
		//Print(deviceContext,0,0,platform::core::QuickFormat("%d",m1),white,semiblack);
		t->deactivateRenderTarget(deviceContext);
	}
	UnapplyPass(deviceContext);
}

vec4 RenderPlatform::TexelQuery(DeviceContext &deviceContext,int query_id,uint2 pos,Texture *texture)
{
	if((int)query_id>=textureQueryResult.count)
	{
		textureQueryResult.InvalidateDeviceObjects();
		textureQueryResult.RestoreDeviceObjects(this,(int)query_id+1,true,true,nullptr,"texel query");
	}
	debugConstants.queryPos=pos;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	textureQueryResult.ApplyAsUnorderedAccessView(deviceContext,debugEffect,debugEffect->GetShaderResource("textureQueryResults"));
	debugEffect->SetTexture(deviceContext,"imageTexture",texture);
	debugEffect->Apply(deviceContext,"texel_query",0);
	DispatchCompute(deviceContext,textureQueryResult.count,1,1);
	debugEffect->Unapply(deviceContext);
	textureQueryResult.CopyToReadBuffer(deviceContext);
	const vec4 *result=textureQueryResult.OpenReadBuffer(deviceContext);
	vec4 r;
	if(result)
		r=result[query_id];
	textureQueryResult.CloseReadBuffer(deviceContext);
	return r;
}

void RenderPlatform::HeightMapToNormalMap(GraphicsDeviceContext &deviceContext,Texture *heightMap,Texture *normalMap,float scale)
{
	if(!heightMap||!heightMap->IsValid())
		return;
	if(!normalMap||!normalMap->IsValid())
		return;
	auto _imageTexture=debugEffect->GetShaderResource("imageTexture");
	auto pass=debugEffect->GetTechniqueByName("height_to_normal")->GetPass(0);
	debugConstants.texSize=uint4(heightMap->width,heightMap->length,1,1);
	debugConstants.multiplier=vec4(scale,scale,scale,scale);
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	ApplyPass(deviceContext,pass);
	normalMap->activateRenderTarget(deviceContext,{normalMap->GetShaderResourceTypeForRTVAndDSV(),{TextureAspectFlags::COLOUR,0,1,0,1 }});
	SetTexture(deviceContext,_imageTexture,heightMap,{TextureAspectFlags::COLOUR,0,1,0,1});
	DrawQuad(deviceContext);
	//Print(deviceContext,0,0,platform::core::QuickFormat("%d",m1),white,semiblack);
	normalMap->deactivateRenderTarget(deviceContext);
	
	UnapplyPass(deviceContext);
}

        
std::vector<std::string> RenderPlatform::GetTexturePathsUtf8()
{
	return texturePathsUtf8;
}

platform::core::MemoryInterface *RenderPlatform::GetMemoryInterface()
{
	return memoryInterface;
}

void RenderPlatform::SetMemoryInterface(platform::core::MemoryInterface *m)
{
	// TODO: shutdown old memory, test for leaks at RenderPlatform shutdown.
	memoryInterface=m;
	allocator.SetExternalAllocator(m);
}

crossplatform::Effect *RenderPlatform::GetDebugEffect()
{
	return debugEffect;
}

crossplatform::Effect* RenderPlatform::GetCopyEffect()
{
	return copyEffect;
}

ConstantBuffer<DebugConstants> &RenderPlatform::GetDebugConstantBuffer()
{
	return debugConstants;
}

bool RenderPlatform::IsDepthFormat(PixelFormat f)
{
	switch(f)
	{
	case D_32_FLOAT: 
	case D_24_UNORM_S_8_UINT:
	case D_16_UNORM:
	case D_32_FLOAT_S_8_UINT:
	case D_32_UINT:
		return true;
	default:
		return false;
	};
}


PixelFormat RenderPlatform::ToColourFormat(PixelFormat f)
{
	switch(f)
	{
	case D_32_FLOAT:
		return R_32_FLOAT;
	case D_16_UNORM:
		return R_16_FLOAT;
	case D_32_FLOAT_S_8_UINT:
		return R_32_FLOAT;
	default:
		return f;
	};
}
bool RenderPlatform::IsStencilFormat(PixelFormat f)
{
	switch(f)
	{
	case D_24_UNORM_S_8_UINT:
	case D_32_FLOAT_S_8_UINT:
		return true;
	default:
		return false;
	};
}

uint32_t RenderPlatform::GetLayerCountFromRenderTargets(const GraphicsDeviceContext& deviceContext, uint32_t maxArrayLayerCount)
{
	// Get the current targets:
	const crossplatform::TargetsAndViewport* targets = &deviceContext.defaultTargetsAndViewport;
	if (!deviceContext.targetStack.empty())
	{
		targets = deviceContext.targetStack.top();
	}

	if (targets->num > 1)
		SIMUL_CERR << "Using ViewInstancing with " << targets->num << " render targets. Only using the first one.\n";

	uint32_t rtLayerCount = 1;
	crossplatform::Texture* target = targets->textureTargets[0].texture;
	if (target)
		rtLayerCount = target->IsCubemap() ? target->arraySize * 6 : target->arraySize;

	SIMUL_ASSERT(rtLayerCount <= maxArrayLayerCount);
	
	return static_cast<uint32_t>(rtLayerCount);
}

uint32_t RenderPlatform::GetViewMaskFromRenderTargets(const GraphicsDeviceContext& deviceContext, uint32_t maxArrayLayerCount)
{
	uint32_t rtLayerCount = GetLayerCountFromRenderTargets(deviceContext, maxArrayLayerCount);

	uint32_t viewMask = (uint32_t)pow<uint32_t, uint32_t>(2, rtLayerCount) - 1;
	return viewMask;
}

bool RenderPlatform::SaveTextureDataToDisk(const char* filename, int width, int height, PixelFormat format, const void* data)
{
	crossplatform::PixelFormatType type = GetElementType(format);
	uint64_t elementCount = static_cast<uint64_t>(GetElementCount(format));
	uint64_t elementSize = static_cast<uint64_t>(GetElementSize(format));
	uint64_t texelCount = static_cast<uint64_t>(width * height); //Assume 2D texture only.
	uint64_t texelSize = elementCount * elementSize;
	uint64_t size = texelCount * texelSize;

	std::vector<uint8_t> imageData;
	imageData.reserve(texelCount * elementCount);

	if (data)
	{
		for (uint64_t i = 0; i < size; i += elementSize)
		{
			uint8_t u8value = 0;
			void* _ptr = (void*)((uint64_t)data + i);
			switch (type)
			{
				using namespace crossplatform;
				case PixelFormatType::DOUBLE:
				{
					double value = std::clamp<double>(*(double*)_ptr, 0.0, 1.0);
					u8value = static_cast<uint8_t>(value * UINT8_MAX);
					break;
				}
				case PixelFormatType::FLOAT:
				{
					float value = std::clamp<float>(*(float*)_ptr, 0.0f, 1.0f);
					u8value = static_cast<uint8_t>(value * UINT8_MAX);
					break;
				}
				case PixelFormatType::HALF:
				{
					float value = std::clamp<float>(math::ToFloat32(*(uint16_t*)_ptr), 0.0f, 1.0f);
					u8value = static_cast<uint8_t>(value * UINT8_MAX);
					break;
				}
				case PixelFormatType::UINT:
				{
					uint32_t value = std::clamp<uint32_t>(*(uint32_t*)_ptr, 0, UINT32_MAX);
					u8value = static_cast<uint8_t>(static_cast<float>(value) / static_cast<float>(UINT32_MAX));
					break;
				}
				case PixelFormatType::USHORT:
				{
					uint16_t value = std::clamp<uint16_t>(*(uint16_t*)_ptr, 0, UINT16_MAX);
					u8value = static_cast<uint8_t>(static_cast<float>(value) / static_cast<float>(UINT16_MAX));
					break;
				}
				case PixelFormatType::UCHAR:
				{
					u8value = std::clamp<uint8_t>(*(uint8_t*)_ptr, 0, UINT8_MAX);
					break;
				}
				case PixelFormatType::INT:
				{
					int32_t value = std::clamp<int32_t>(*(int32_t*)_ptr, 0, INT32_MAX);
					u8value = static_cast<uint8_t>(static_cast<float>(value) / static_cast<float>(INT32_MAX));

					break;
				}
				case PixelFormatType::SHORT:
				{
					int16_t value = std::clamp<int16_t>(*(int16_t*)_ptr, 0, INT16_MAX);
					u8value = static_cast<uint8_t>(static_cast<float>(value) / static_cast<float>(INT16_MAX));
					break;
				}
				case PixelFormatType::CHAR:
				{
					u8value = std::clamp<int8_t>(*(int8_t*)_ptr, 0, INT8_MAX);
					break;
				}
			}
			imageData.push_back(u8value);
		}
	}

	int res = stbi_write_png(filename, width, height, (int)elementCount, imageData.data(), (int)(elementCount * width));
	if (res != 1)
	{
		SIMUL_BREAK_ONCE("Failed to save screenshot data to disk.");
		return false;
	}
	return true;
}

void RenderPlatform::DrawLine(GraphicsDeviceContext &deviceContext,const float *startp, const float *endp,const float *colour,float width)
{
	PosColourVertex line_vertices[2];
	line_vertices[0].pos= vec3(startp)-deviceContext.viewStruct.cam_pos;
	line_vertices[0].colour=colour;
	line_vertices[1].pos= vec3(endp)-deviceContext.viewStruct.cam_pos;
	line_vertices[1].colour=colour;

	DrawLines(deviceContext,line_vertices,2,true,false,true);
}

static float length(const vec3 &u)
{
	float size=u.x*u.x+u.y*u.y+u.z*u.z;
	return sqrt(size);
}
void RenderPlatform::DrawCircle(GraphicsDeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill)
{
	vec3 pos=GetCameraPosVector(deviceContext.viewStruct.view);
	vec3 d=dir;
	d/=length(d);
	pos+=d;
	float radius=rads;
	DrawCircle(deviceContext,pos,dir,radius,colr,fill);
}

void RenderPlatform::DrawCircle(GraphicsDeviceContext &deviceContext,const float *pos,const float *dir,float radius,const float *colr,bool fill)
{
	PosColourVertex line_vertices[37];
	math::Vector3 direction(dir);
	direction.Unit();
	math::Vector3 z(0,0,1.f);
	math::Vector3 y(0,1.f,0);
	math::Vector3 x=z^direction;
	if(x.Magnitude()>.1f)
		x.Unit();
	else
		x=direction^y;
	x*=radius;
	y=(direction^x);
	int l=0;
	for(int j=0;j<36;j++)
	{
		float angle					=(float(j)/35.0f)*2.0f*3.1415926536f;
		line_vertices[l].pos		=pos+(x*cos(angle)+y*sin(angle));
		line_vertices[l++].colour	=colr;
	}
	DrawLines(deviceContext,line_vertices,36,true,false,false);
}

void RenderPlatform::SetModelMatrix(GraphicsDeviceContext &deviceContext, const double *m, const crossplatform::PhysicalLightRenderData &physicalLightRenderData)
{
	platform::crossplatform::Frustum frustum = platform::crossplatform::GetFrustumFromProjectionMatrix((const float*)deviceContext.viewStruct.proj);
	SetStandardRenderState(deviceContext, frustum.reverseDepth ? crossplatform::STANDARD_DEPTH_GREATER_EQUAL : crossplatform::STANDARD_DEPTH_LESS_EQUAL);
}

void RenderPlatform::ClearTextures()
{
	//textures.clear();
	//unfinishedTextures.clear();
	//unMippedTextures.clear();
}

crossplatform::Texture* RenderPlatform::GetOrCreateTexture(const char* filename,bool gen_mips)
{
	auto i = textures.find(filename);
	if (i != textures.end())
		return i->second;
	crossplatform::Texture* t=CreateTexture(filename, gen_mips);
	textures[filename] = t;
	// special textures:
	if (std::string(filename) == "white")
	{
		t->ensureTexture2DSizeAndFormat(this,1,1, 1,PixelFormat::RGBA_8_UNORM,false,false,false,1,0,true,vec4(1.f,1.f,1.f,1.f));
		unsigned white_rgba8=0xFFFFFFFF;
		t->setTexels(GetImmediateContext(),&white_rgba8,0,1);
	}
	else if (std::string(filename) == "black")
	{
		t->ensureTexture2DSizeAndFormat(this, 1, 1, 1, PixelFormat::RGBA_8_UNORM, false, false, false, 1, 0, true, vec4(1.f, 1.f, 1.f, 1.f));
		unsigned black_rgba8 = 0;
		t->setTexels(GetImmediateContext(), &black_rgba8, 0, 1);
	}
	else if (std::string(filename) == "blue")
	{
		t->ensureTexture2DSizeAndFormat(this, 1, 1, 1, PixelFormat::RGBA_8_UNORM, false, false, false, 1, 0, true, vec4(1.f, 1.f, 1.f, 1.f));
		unsigned blue_rgba8 = 0x7FFF7F7F;
		t->setTexels(GetImmediateContext(), &blue_rgba8, 0, 1);
	}
	return t;
}

Material *RenderPlatform::GetOrCreateMaterial(const char *name)
{
	auto i = materials.find(name);
	if (i != materials.end())
		return i->second;
	crossplatform::Material *mat = new crossplatform::Material(name);
	mat->SetEffect(solidEffect);
	materials[name]=mat;
	return mat;
}

BottomLevelAccelerationStructure* RenderPlatform::CreateBottomLevelAccelerationStructure()
{
	return new BottomLevelAccelerationStructure(this);
}

TopLevelAccelerationStructure* RenderPlatform::CreateTopLevelAccelerationStructure()
{
	return new TopLevelAccelerationStructure(this);
}

AccelerationStructureManager* RenderPlatform::CreateAccelerationStructureManager()
{
	return new AccelerationStructureManager(this);
}

ShaderBindingTable* RenderPlatform::CreateShaderBindingTable()
{
	return new ShaderBindingTable();
}

Mesh *RenderPlatform::CreateMesh()
{
	return new Mesh(this);
}			

Texture* RenderPlatform::CreateTexture(const char* fileNameUtf8, bool gen_mips)
{
	crossplatform::Texture* tex = createTexture();
	if (fileNameUtf8 && strlen(fileNameUtf8) > 0)
	{
		if (strstr(fileNameUtf8, ".") != nullptr)
		{
			tex->LoadFromFile(this, fileNameUtf8, gen_mips);
			unfinishedTextures.insert(tex);
			SIMUL_INTERNAL_COUT <<"unfinishedTexture: "<<tex<<" "<<fileNameUtf8<<std::endl;
		}
		tex->SetName(fileNameUtf8);
	}
	return tex;
}

void RenderPlatform::InvalidatingTexture(Texture *t)
{
	auto i=unfinishedTextures.find(t);
	if(i!=unfinishedTextures.end())
		unfinishedTextures.erase(i);
}

void RenderPlatform::DrawCubemap(GraphicsDeviceContext &deviceContext,Texture *cubemap,int x,int y,int pixelSize,float exposure,float gamma,float displayLod)
{
	//unsigned int num_v=0;
	#if SIMUL_INTERNAL_CHECKS
	if(cubemap&&cubemap->IsCubemap()==false)
	{
		SIMUL_INTERNAL_CERR_ONCE<<"Texture "<<cubemap->GetName().c_str() << " is not a cubemap.\n";
	}
	#endif
	Viewport oldv=GetViewport(deviceContext,0);
	
	float aspect=float(oldv.w)/float(oldv.h);
	int pixelWidth=int(float(pixelSize)*(aspect>1.0f?aspect:1.0f));
	int pixelHeight=int(float(pixelSize)/(aspect<1.0f?aspect:1.0f));
	// Setup the viewport for rendering.
	Viewport viewport;
	viewport.w		=pixelWidth;
	viewport.h		=pixelHeight;
	viewport.x		=x-pixelWidth/2;
	viewport.y		=y-pixelHeight/2;
	SetViewports(deviceContext,1,&viewport);
	
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	math::Matrix4x4 proj=crossplatform::Camera::MakeDepthReversedProjectionMatrix(1.f,(float)viewport.h/(float)viewport.w,0.1f,100.f);
	// Create the viewport.
	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();
	float tan_x=1.0f/proj(0, 0);
	//float tan_y=1.0f/proj(1, 1);
	float size_req=tan_x*.5f;
	static float sizem=3.f;
	float d=2.0f*sizem/size_req;
	platform::math::Vector3 offs0(0,0,-d);
	view._41=0;
	view._42=0;
	view._43=0;
	platform::math::Vector3 offs;
	Multiply3(offs,view,offs0);
	world._14 =offs.x;
	world._24 =offs.y;
	world._34 =offs.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	debugConstants.debugWorldViewProj=wvp;
	debugConstants.displayLod=displayLod;
	debugEffect->SetTexture(deviceContext,debugEffect_cubeTexture,cubemap);
	static float rr=6.f;
	debugConstants.latitudes		=16;
	debugConstants.longitudes		=32;
	debugConstants.radius			=rr;
	debugConstants.multiplier		=vec4(exposure,exposure,exposure,0.0f);
	debugConstants.debugGamma		=gamma;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,draw_cubemap_sphere,0);

	SetTopology(deviceContext,Topology::TRIANGLESTRIP);
	Draw(deviceContext, (debugConstants.longitudes+1)*(debugConstants.latitudes+1)*2, 0);

	debugEffect->SetTexture(deviceContext, debugEffect_cubeTexture, nullptr);
	debugEffect->Unapply(deviceContext);
	SetViewports(deviceContext,1,&oldv);
}

void RenderPlatform::DrawCubemap(GraphicsDeviceContext &deviceContext,Texture *cubemap,float offsetx,float offsety,float size,float exposure,float gamma,float displayLod)
{
	//unsigned int num_v=0;
	#if SIMUL_INTERNAL_CHECKS
	if(cubemap&&cubemap->IsCubemap()==false)
	{
		SIMUL_INTERNAL_CERR_ONCE<<"Texture "<<cubemap->GetName().c_str() << " is not a cubemap.\n";
	}
	#endif
	Viewport oldv=GetViewport(deviceContext,0);
	
	// Setup the viewport for rendering.
	Viewport viewport;
	viewport.w		=(int)(oldv.w*size);
	viewport.h		=(int)(oldv.h*size);
	viewport.x		=(int)(0.5f*(1.f+offsetx)*oldv.w-viewport.w/2);
	viewport.y		=(int)(0.5f*(1.f-offsety)*oldv.h-viewport.h/2);
	SetViewports(deviceContext,1,&viewport);
	
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	math::Matrix4x4 proj=crossplatform::Camera::MakeDepthReversedProjectionMatrix(1.f,(float)viewport.h/(float)viewport.w,0.1f,100.f);
	// Create the viewport.
	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();
	float tan_x=1.0f/proj(0, 0);
	//float tan_y=1.0f/proj(1, 1);
	float size_req=tan_x*.5f;
	static float sizem=3.f;
	float d=2.0f*sizem/size_req;
	platform::math::Vector3 offs0(0,0,-d);
	view._41=0;
	view._42=0;
	view._43=0;
	platform::math::Vector3 offs;
	Multiply3(offs,view,offs0);
	world._14 =offs.x;
	world._24 =offs.y;
	world._34 =offs.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	debugConstants.debugWorldViewProj=wvp;
	debugConstants.displayLod=displayLod;
	debugEffect->SetTexture(deviceContext,debugEffect_cubeTexture,cubemap);
	static float rr=6.f;
	debugConstants.latitudes		=16;
	debugConstants.longitudes		=32;
	debugConstants.radius			=rr;
	debugConstants.multiplier		=vec4(exposure,exposure,exposure,0.0f);
	debugConstants.debugGamma		=gamma;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,draw_cubemap_sphere,0);

	SetTopology(deviceContext,Topology::TRIANGLESTRIP);
	Draw(deviceContext, (debugConstants.longitudes+1)*(debugConstants.latitudes+1)*2, 0);

	debugEffect->SetTexture(deviceContext, debugEffect_cubeTexture, nullptr);
	debugEffect->Unapply(deviceContext);
	SetViewports(deviceContext,1,&oldv);
}

void RenderPlatform::DrawAxes(GraphicsDeviceContext &deviceContext,const mat4 &m,float size)
{
	mat4 wvp;
	crossplatform::MakeWorldViewProjMatrix((float*)&wvp,m,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	debugConstants.debugWorldViewProj=wvp;
	debugConstants.radius			=size;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,"axes",0);
	SetTopology(deviceContext,Topology::LINELIST);
	Draw(deviceContext, 7, 0);
	debugEffect->Unapply(deviceContext);
}

void RenderPlatform::PrintAt3dPos(GraphicsDeviceContext &deviceContext,const float *p,const char *text,const float* colr,const float* bkg,int offsetx,int offsety,bool centred)
{
	//unsigned int num_v=1;
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	mat4 wvp;
	if(centred)
		crossplatform::MakeCentredViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	else
		crossplatform::MakeViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);

	vec4 clip_pos;
	clip_pos=wvp*vec4(p[0],p[1],p[2],1.0f);
	if(clip_pos.w<0)
		return;
	clip_pos.x/=clip_pos.w;
	clip_pos.y/=clip_pos.w;
	static bool ml=true;
	vec2 pos;
	pos.x=(1.0f+clip_pos.x)/2.0f;
	if(mirrorY)
		pos.y=(1.0f+clip_pos.y)/2.0f;
	else
		pos.y=(1.0f-clip_pos.y)/2.0f;
	if(ml)
	{
		pos.x*=viewport.w;
		pos.y*=viewport.h;
	}

	Print(deviceContext,(int)pos.x+offsetx,(int)pos.y+offsety,text,colr,bkg);
}

void RenderPlatform::PrintAt3dPos(MultiviewGraphicsDeviceContext& deviceContext, const float* p, const char* text, const float* colr, const float* bkg)
{
	crossplatform::Viewport viewport = GetViewport(deviceContext, 0);

	auto CreateViewProjectionMatrix = [&](size_t index) -> mat4
	{
		/*math::Matrix4x4 matrix = deviceContext.viewStructs[index].view;
		matrix.Transpose();

		//Translation
		math::Vector3 translation(matrix.m03, matrix.m13, matrix.m23);

		//Scale
		float scale_x = math::Vector3(matrix.m00, matrix.m10, matrix.m20).Magnitude();
		float scale_y = math::Vector3(matrix.m01, matrix.m11, matrix.m21).Magnitude();
		float scale_z = math::Vector3(matrix.m02, matrix.m12, matrix.m22).Magnitude();
		math::Vector3 scale(scale_x, scale_y, scale_z);

		//Orientation
		math::Matrix4x4 rotation(
			matrix.m00 / scale_x, matrix.m01 / scale_y, matrix.m02 / scale_z, 0.0f,
			matrix.m10 / scale_x, matrix.m11 / scale_y, matrix.m12 / scale_z, 0.0f,
			matrix.m20 / scale_x, matrix.m21 / scale_y, matrix.m22 / scale_z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
		math::Quaternion orientation;
		math::MatrixToQuaternion(orientation, rotation);*/

		mat4 vp;
		crossplatform::MakeViewProjMatrix((float*)&vp, deviceContext.viewStructs[index].view, deviceContext.viewStructs[index].proj);
		return vp;
	};

	std::vector<mat4> vpMatrices;
	for (size_t i = 0; i < deviceContext.viewStructs.size(); i++)
		vpMatrices.push_back(CreateViewProjectionMatrix(i));

	auto CreateTextPosition = [&](const mat4& vp, vec2& outPos) -> bool
	{
		vec4 clip_pos;
		clip_pos = vp * vec4(p[0], p[1], p[2], 1.0f);
		if (clip_pos.w < 0)
			return false;
		clip_pos.x /= clip_pos.w;
		clip_pos.y /= clip_pos.w;
		static bool ml = true;
		vec2 pos;
		pos.x = (1.0f + clip_pos.x) / 2.0f;
		if (mirrorY)
			pos.y = (1.0f + clip_pos.y) / 2.0f;
		else
			pos.y = (1.0f - clip_pos.y) / 2.0f;
		if (ml)
		{
			pos.x *= viewport.w;
			pos.y *= viewport.h;
		}
		outPos = pos;
		return true;
	};

	std::array<std::vector<float>, 2> positions;
	for (const auto& vp : vpMatrices)
	{
		vec2 outPos;
		if (CreateTextPosition(vp, outPos))
		{
			positions[0].push_back(outPos.x);
			positions[1].push_back(outPos.y);
		}
		else
			return;
	}

	Print(deviceContext, positions[0].data(), positions[1].data(), text, colr, bkg);
}

int2 RenderPlatform::DrawTexture(GraphicsDeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, vec4 mult, bool blend, float gamma, bool debug, vec2 texc, vec2 texc_scale, float mip, int slice)
{
	int level=slice;
	static int frames=25;
	static int count=frames;
	static unsigned long long framenumber=0;
	float displayLod=mip;
	static int lod=0;
	if (tex!=nullptr&&dy == 0&& tex->width>0)
	{
		dy = (dx * tex->length) / tex->width;
	}
	if(debug&&mip<0)
	{
		if(framenumber!=GetFrameNumber())
		{
			count--;
			if(!count)
			{
				lod++;
				count=frames;
			}
			framenumber=GetFrameNumber();
		}
		if(tex)
		{
			int m=tex->GetMipCount();
			displayLod=float((lod%(m?m:1)));
		}
	}
	if(tex && !tex->IsValid())
		tex=nullptr;
	debugConstants.debugGamma=gamma;
	debugConstants.multiplier=mult;
	debugConstants.displayLod=displayLod;
	debugConstants.displayLevel=0;
	debugConstants.debugTime = float(this->frameNumber);
	if(texc_scale.x==0||texc_scale.y==0)
	{
		texc_scale.x=1.0f;
		texc_scale.y=1.0f;
	}
	debugConstants.viewport={texc.x,texc.y,texc_scale.x,texc_scale.y};
	crossplatform::EffectTechnique *tech=textured;
	if(tex&&tex->GetDimension()==3)
	{
		if (debug)
		{
			tech = debugEffect->GetTechniqueByName("trace_volume");
			
			static char c = 0;
			static char cc = 20;
			c--;
			if (!c)
			{
				level++;
				c = cc;
			}
			debugConstants.displayLevel = (float)(level%std::max(1, tex->depth)) / tex->depth;
		}
		else
			tech=showVolume;

		debugEffect->SetTexture(deviceContext,volumeTexture,tex);
	}
	else if(tex&&tex->IsCubemap())
	{
		if(tex->arraySize>1)
		{
			tech=debugEffect->GetTechniqueByName("show_cubemap_array");
			debugEffect->SetTexture(deviceContext,"cubeTextureArray",tex,{TextureAspectFlags::COLOUR, (int32_t)displayLod, 1, 0, -1});
			if(debug)
			{
				static char c=0;
				static char cc=20;
				c--;
				if(!c)
				{
					level++;
					c=cc;
				}
				debugConstants.displayLevel=(float)(level%std::max(1,tex->arraySize));
			}
		}
		else
		{
			tech=debugEffect->GetTechniqueByName("show_cubemap");
			debugEffect->SetTexture(deviceContext,"cubeTexture",tex,{TextureAspectFlags::COLOUR, (int32_t)displayLod, 1, 0, -1});
			debugConstants.displayLevel=0;
		}
	}
	else if(tex&&tex->arraySize>1)
	{
		tech = debugEffect->GetTechniqueByName("show_texture_array");
		debugEffect->SetTexture(deviceContext, "imageTextureArray", tex,{TextureAspectFlags::COLOUR, (int32_t)displayLod, 1, 0, -1});
		{
			static char c = 0;
			static char cc = 20;
			c--;
			if (!c)
			{
				level++;
				c = cc;
			}

			debugConstants.displayLevel = (float)(level%std::max(1, tex->arraySize));
		}
	}
	else if(tex)
	{
		debugEffect->SetTexture(deviceContext,imageTexture,tex,{TextureAspectFlags::COLOUR, (int32_t)displayLod, 1, 0, -1});
	}
	else
	{
		tech=untextured;
	}
	DrawQuad(deviceContext,x1,y1,dx,dy,debugEffect,tech,blend?"blend":"noblend");
	debugEffect->UnbindTextures(deviceContext);
	if(debug)
	{
		vec4 white(1.0, 1.0, 1.0, 1.0);
		vec4 semiblack(0, 0, 0, 0.5);
		char mip_txt[]="MIP: 0";
		char lvl_txt[]="LVL: 0";
		if(tex&&tex->GetMipCount()>1&&displayLod>0&&displayLod<10)
		{
			mip_txt[5]='0'+(char)displayLod;
			Print(deviceContext,x1,y1+20,mip_txt,white,semiblack);
		}
		if(tex&&tex->arraySize>1)
		{
			int l=level%tex->arraySize;
			if(l<10)
				lvl_txt[5]='0'+(l);
			Print(deviceContext,x1+60,y1+20,lvl_txt,white,semiblack);
		}
		if (tex&&tex->GetDimension() == 3)
		{
			int l=level%tex->depth;
			Print(deviceContext, x1, y1 + 20, ("Z: " + std::to_string(l)).c_str(), white, semiblack);
		}
	}
    return int2(dx, dy);
}

void RenderPlatform::DrawQuad(GraphicsDeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect
	,crossplatform::EffectTechnique *technique,const char *pass)
{
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	if(mirrorY)
		y1=(int)viewport.h-y1-dy;
	vec4 r(2.f*(float)x1/(float)viewport.w-1.f
		,1.f-2.f*(float)(y1+dy)/(float)viewport.h
		,2.f*(float)dx/(float)viewport.w
		,2.f*(float)dy/(float)viewport.h);
	debugConstants.LinkToEffect(effect,"DebugConstants");
	debugConstants.rect=r;
	effect->SetConstantBuffer(deviceContext,&debugConstants);
	effect->Apply(deviceContext,technique,pass);
	DrawQuad(deviceContext);
	effect->Unapply(deviceContext);
}

int2 RenderPlatform::DrawTexture(GraphicsDeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, float mult, bool blend, float gamma, bool debug, vec2 texc, vec2 texc_scale, float mip, int slice)
{
	return DrawTexture(deviceContext,x1,y1,dx,dy,tex,vec4(mult,mult,mult,0.0f),blend,gamma,debug,texc,texc_scale, mip, slice);
}

int2 RenderPlatform::DrawDepth(GraphicsDeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, const crossplatform::Viewport *v
	,const float *proj)
{
	crossplatform::EffectTechnique *tech	=debugEffect->GetTechniqueByName("show_depth");
	if(tex&&tex->GetSampleCount()>0)
	{
		tech=debugEffect->GetTechniqueByName("show_depth_ms");
		debugEffect->SetTexture(deviceContext,"imageTextureMS",tex);
	}
	else if(tex&&tex->IsCubemap())
	{
		tech=debugEffect->GetTechniqueByName("show_depth_cube");
		debugEffect->SetTexture(deviceContext,"cubeTexture",tex);
	}
	else
	{
		debugEffect->SetTexture(deviceContext,"imageTexture",tex);
	}
	if(!proj)
		proj=deviceContext.viewStruct.proj;
	platform::crossplatform::Frustum frustum=platform::crossplatform::GetFrustumFromProjectionMatrix(proj);
	debugConstants.debugTanHalfFov=(frustum.tanHalfFov);

	vec4 depthToLinFadeDistParams=deviceContext.viewStruct.GetDepthToLinearDistanceParameters(isinf(frustum.farZ)?500000.0f:frustum.farZ);
	debugConstants.debugDepthToLinFadeDistParams=depthToLinFadeDistParams;
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	if(mirrorY2)
	{
		y1=(int)viewport.h-y1-dy;
	}
	{
		if(tex&&v)
			debugConstants.viewport=vec4((float)v->x/(float)tex->width,(float)v->y/(float)tex->length,(float)v->w/(float)tex->width,(float)v->h/(float)tex->length);
		else
			debugConstants.viewport=vec4(0.f,0.f,1.f,1.f);
		debugConstants.rect=vec4(2.f*(float)x1/(float)viewport.w-1.f
								,1.f-2.f*(float)(y1+dy)/(float)viewport.h
								,2.f*(float)dx/(float)viewport.w
								,2.f*(float)dy/(float)viewport.h);
		debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
		debugEffect->Apply(deviceContext,tech,frustum.reverseDepth?"reverse_depth":"forward_depth");
		DrawQuad(deviceContext);
		debugEffect->UnbindTextures(deviceContext);
		debugEffect->Unapply(deviceContext);
	}
    return int2(dx, dy);
}

void RenderPlatform::Draw2dLine(GraphicsDeviceContext &deviceContext,vec2 pos1,vec2 pos2,vec4 colour)
{
	PosColourVertex pts[2];
	pts[0].pos=pos1;
	pts[0].pos.z=0;
	pts[0].colour=colour;
	pts[1].pos=pos2;
	pts[1].pos.z=0;
	pts[1].colour=colour;
	Draw2dLines(deviceContext,pts,2,false);
}

int RenderPlatform::Print(GraphicsDeviceContext& deviceContext, int x, int y, const char* text, const float* colr, const float* bkg)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, "text")
	static float clr[]={1.f,1.f,0.f,1.f};
	static float black[]={0.f,0.f,0.f,0.0f};
	if(!colr)
		colr=clr;
	if(!bkg)
		bkg=black;
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	int lines=0;
	if(*text!=0)
	{
		lines+=textRenderer->Render(deviceContext,(float)x,(float)y,(float)viewport.w,(float)viewport.h,text,colr,bkg,mirrorYText);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext)
	return lines;
}

int RenderPlatform::Print(MultiviewGraphicsDeviceContext& deviceContext, float* xs, float* ys, const char* text, const float* colr, const float* bkg)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, "text")
	static float clr[] = { 1.f,1.f,0.f,1.f };
	static float black[] = { 0.f,0.f,0.f,0.0f };
	if (!colr)
		colr = clr;
	if (!bkg)
		bkg = black;
	crossplatform::Viewport viewport = GetViewport(deviceContext, 0);
	int lines = 0;
	if (*text != 0)
	{
		lines += textRenderer->Render(deviceContext, xs, ys, (float)viewport.w, (float)viewport.h, text, colr, bkg, mirrorYText);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext)
	return lines;
}

void RenderPlatform::LinePrint(GraphicsDeviceContext &deviceContext,const char *text,const float* colr,const float* bkg)
{
	int lines=Print(deviceContext, deviceContext.framePrintX,deviceContext.framePrintY,text,colr,bkg);
	deviceContext.framePrintY+=lines*textRenderer->GetDefaultTextHeight();
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext& deviceContext, bool )
{
	if(deviceContext.GetFrameNumber()!=last_begin_frame_number&&deviceContext.AsGraphicsDeviceContext())
		ContextFrameBegin(*(deviceContext.AsGraphicsDeviceContext()));
	return true;
}

crossplatform::Viewport RenderPlatform::PlatformGetViewport(crossplatform::DeviceContext &,int)
{
	crossplatform::Viewport v;
	memset(&v,0,sizeof(v));
	return v;
}

void RenderPlatform::SetViewports(GraphicsDeviceContext &deviceContext,int num,const crossplatform::Viewport *vps)
{
	if(num>0&&vps!=nullptr)
		memcpy(deviceContext.contextState.viewports,vps,num*sizeof(Viewport));
	auto *tv=deviceContext.GetCurrentTargetsAndViewport();
	if(tv&&vps)
		tv->viewport=*vps;
}

void RenderPlatform::SetScissor(GraphicsDeviceContext &deviceContext,int4 sc)
{
	deviceContext.contextState.scissor=sc;
}

int4 RenderPlatform::GetScissor(GraphicsDeviceContext &deviceContext) const
{
	return deviceContext.contextState.scissor;
}

crossplatform::Viewport	RenderPlatform::GetViewport(GraphicsDeviceContext &deviceContext,int index)
{
	crossplatform::Viewport v;
	if(deviceContext.GetFrameBufferStack().size())
	{
		v= deviceContext.GetFrameBufferStack().top()->viewport;
	}
	else
	{
		if(deviceContext.defaultTargetsAndViewport.viewport.w*deviceContext.defaultTargetsAndViewport.viewport.h==0)
		{
			//SIMUL_BREAK_ONCE("The default viewport is empty. Please call deviceContext.setDefaultRenderTargets() at least once on initialization or at the start of the trueSKY frame.");
		}
		v= deviceContext.defaultTargetsAndViewport.viewport;
	}
	return v;
}

void RenderPlatform::SetLayout(GraphicsDeviceContext &deviceContext,Layout *l)
{
	if(l)
		l->Apply(deviceContext);
}

void RenderPlatform::SetConstantBuffer(DeviceContext& deviceContext,ConstantBufferBase *s)
{
	PlatformConstantBuffer *pcb = (PlatformConstantBuffer*)s->GetPlatformConstantBuffer();
	pcb->Apply(deviceContext, s->GetSize(), s->GetAddr());

	deviceContext.contextState.applyBuffers[s->GetIndex()] = s;
	deviceContext.contextState.constantBuffersValid = false;
	pcb->SetChanged();
}

void RenderPlatform::SetStructuredBuffer(DeviceContext& deviceContext, BaseStructuredBuffer* s, const ShaderResource& shaderResource)
{
	if((shaderResource.shaderResourceType & ShaderResourceType::RW) == ShaderResourceType::RW)
		s->platformStructuredBuffer->ApplyAsUnorderedAccessView(deviceContext, shaderResource);
	else
		s->platformStructuredBuffer->Apply(deviceContext,shaderResource);
}

crossplatform::GpuProfiler *RenderPlatform::GetGpuProfiler()
{
	return gpuProfiler;
}


SamplerState *RenderPlatform::GetOrCreateSamplerStateByName	(const char *name_utf8,platform::crossplatform::SamplerStateDesc *desc)
{
	SamplerState *ss=nullptr;
	std::string str(name_utf8);
	if(sharedSamplerStates.find(str)!=sharedSamplerStates.end())
	{
		SamplerState *s=sharedSamplerStates[str];
		if(!desc||s->default_slot==desc->slot||s->default_slot==-1||desc->slot==-1)
		{
			if(desc&&desc->slot!=-1)
				s->default_slot=desc->slot;
			ss=s;
		}
		else
		{
			SIMUL_COUT<<"Simul fx: the sampler state "<<name_utf8<<" was declared with slots "<<s->default_slot<<" and "<<desc->slot
				<<"\nTherefore it will have no default slot."<<std::endl;
			s->default_slot=-1;
			ss=s;
		}
	}
	else
	{
		ss=CreateSamplerState(desc);
		sharedSamplerStates[str]=ss;
	}
	return ss;
}

void RenderPlatform::Destroy(Effect *&e)
{
	if (e)
	{
		destroyEffects.insert(e);
		e = nullptr;
	}
}

crossplatform::Effect *RenderPlatform::CreateEffect(const char *filename_utf8)
{
	std::string fn(filename_utf8);
	crossplatform::Effect *e=CreateEffect();
	effects[fn] = e;
	e->SetName(filename_utf8);
	bool success = e->Load(this,filename_utf8);
	if (!success)
	{
		SIMUL_BREAK(platform::core::QuickFormat("Failed to load effect file: %s. Effect will be placeholder.\n", filename_utf8));
		return e;
	}
	return e;
}

Effect* RenderPlatform::GetEffect(const char* filename_utf8)
{
	auto i = effects.find(filename_utf8);
	if (i == effects.end())
		return nullptr;
	return i->second;
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,const LayoutDesc *layoutDesc,bool interleaved)
{
	auto l= new Layout();
	l->SetDesc(layoutDesc,num_elements,interleaved);
	return l;
}

RenderState *RenderPlatform::CreateRenderState(const RenderStateDesc &desc)
{
	RenderState *rs=new RenderState;
	rs->type=desc.type;
	rs->desc=desc;
	return rs;
}

crossplatform::Shader *RenderPlatform::EnsureShader(const char *filenameUtf8, crossplatform::ShaderType t)
{
	platform::core::FileLoader* fileLoader = platform::core::FileLoader::GetFileLoader();
	
	std::string shaderSourcePath = fileLoader->FindFileInPathStack(filenameUtf8, GetShaderBinaryPathsUtf8());

	// Load the shader source:
	unsigned int fileSize = 0;
	void* fileData = nullptr;
	// load spirv file as binary data.
	fileLoader->AcquireFileContents(fileData, fileSize, shaderSourcePath.c_str(), false);
	if (!fileData)
	{
		// Some engines force filenames to lower case because reasons:
		std::transform(shaderSourcePath.begin(), shaderSourcePath.end(), shaderSourcePath.begin(), ::tolower);
		fileLoader->AcquireFileContents(fileData, fileSize, shaderSourcePath.c_str(), false);
		if (!fileData)
		{
			SIMUL_CERR << "Failed to load the shader:" << filenameUtf8<<std::endl;
			return nullptr;
		}
	}
	Shader *s = EnsureShader(filenameUtf8,fileData, 0, fileSize, t);

	// Free the loaded memory
	fileLoader->ReleaseFileContents(fileData);
	return s;
}

crossplatform::Shader *RenderPlatform::EnsureShader(const char *filenameUtf8,const void *sfxb_ptr, size_t inline_offset, size_t inline_length, ShaderType t)
{
	std::string name(filenameUtf8);
	//if(shaders.find(name) != shaders.end())
	//	return shaders[name];
	Shader *s = CreateShader();
	s->load(this,filenameUtf8, (unsigned char*)sfxb_ptr+inline_offset, inline_length, t);
	//shaders[name] = s;
	return s;
}

void RenderPlatform::ApplyPass(DeviceContext& deviceContext, EffectPass* pass)
{
	crossplatform::ContextState& cs = deviceContext.contextState;
	if (cs.apply_count != 0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	cs.apply_count++;
	cs.invalidate();
	//cs.currentTechnique = effectTechnique;
	cs.currentEffectPass = pass;
	if (!pass)
	{
		SIMUL_BREAK("No pass found");
	}
	else
		cs.currentEffect = pass->GetEffect();
}

void RenderPlatform::UnapplyPass(DeviceContext& deviceContext)
{
	crossplatform::ContextState& cs = deviceContext.contextState;
	cs.currentEffectPass = NULL;
	cs.currentEffect = NULL;
	if (cs.apply_count <= 0)
		SIMUL_BREAK("RenderPlatform::UnapplyPass without a corresponding Apply!")
	else if (cs.apply_count > 1)
		SIMUL_BREAK("RenderPlatform::ApplyPass has been called too many times!")
	cs.apply_count--;
}

DisplaySurface* RenderPlatform::CreateDisplaySurface()
{
    return nullptr;
}

GpuProfiler* RenderPlatform::CreateGpuProfiler()
{
	return new GpuProfiler();
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext, int slot, int num_buffers,const crossplatform::Buffer *const*buffers, const crossplatform::Layout *layout, const int *vertexSteps)
{
	if (!buffers)
		return;
	for(int i=0;i<num_buffers;i++)
	{
		deviceContext.contextState.applyVertexBuffers[slot+i]=(buffers[i]);
	}
}

void RenderPlatform::ClearVertexBuffers(crossplatform::DeviceContext& deviceContext)
{

}

void RenderPlatform::SetIndexBuffer(GraphicsDeviceContext &deviceContext,const Buffer *buffer)
{
	if (!buffer)
		return;
	deviceContext.contextState.indexBuffer=buffer;
}

void RenderPlatform::SetTopology(GraphicsDeviceContext& deviceContext, crossplatform::Topology t)
{
	deviceContext.contextState.topology = t;
}

void RenderPlatform::FinishLoadingTextures(DeviceContext& deviceContext)
{
	for(auto t:unfinishedTextures)
	{
		t->FinishLoading(deviceContext);
		if(t->ShouldGenerateMips())
			unMippedTextures.insert(t);
	}
	unfinishedTextures.clear();
}

void RenderPlatform::FinishGeneratingTextureMips(DeviceContext& deviceContext)
{
	if(!deviceContext.AsGraphicsDeviceContext())
		return;
	for(auto t:unMippedTextures)
	{
		GenerateMips(*deviceContext.AsGraphicsDeviceContext(),t,true);
	}
	unMippedTextures.clear();
}

void RenderPlatform::SetTexture(DeviceContext& deviceContext, const ShaderResource& res, crossplatform::Texture* tex, const SubresourceRange& subresource)
{
	// If not valid, we've already put out an error message when we assigned the resource, so fail silently. Don't risk overwriting a slot.
	if (!res.valid)
		return;
	ContextState* cs = GetContextState(deviceContext);
	if (cs->apply_count == 0)
	{
		FinishLoadingTextures(deviceContext);
	}
	unsigned long slot = res.slot;
	unsigned long dim = res.dimensions;
#ifdef _DEBUG
	if (!tex)
	{
		//SIMUL_BREAK_ONCE("Null texture applied"); This is ok.
	}
	else if (!tex->IsValid())
	{
		//SIMUL_BREAK_ONCE("Invalid texture applied");
		return;
	}
#endif
	TextureAssignment& ta = cs->textureAssignmentMap[slot];
	ta.resourceType = res.shaderResourceType;
	ta.texture = tex;
	if (!res.valid)
		ta.texture = nullptr;
	else if (tex)
	{
		if (!tex->IsValid())
		{
			ta.texture = nullptr;
		}
	}
	ta.dimensions = dim;
	ta.uav = false;
	ta.subresource = subresource;
	cs->textureAssignmentMapValid = false;
}

void RenderPlatform::SetAccelerationStructure(DeviceContext& deviceContext, const ShaderResource& res, TopLevelAccelerationStructure* a)
{
	// If not valid, we've already put out an error message when we assigned the resource, so fail silently. Don't risk overwriting a slot.
	if (!res.valid)
		return;
	ContextState* cs = GetContextState(deviceContext);
	TextureAssignment& ta = cs->textureAssignmentMap[res.slot];
	ta.resourceType = res.shaderResourceType;
	SIMUL_ASSERT(ta.resourceType==ShaderResourceType::ACCELERATION_STRUCTURE);
	ta.accelerationStructure = a ;
	ta.dimensions = 0;
	ta.uav = false;
	cs->textureAssignmentMapValid = false;
}

void RenderPlatform::SetUnorderedAccessView(DeviceContext& deviceContext, const ShaderResource& res, crossplatform::Texture* tex, const SubresourceLayers& subresource)
{
	// If not valid, we've already put out an error message when we assigned the resource, so fail silently. Don't risk overwriting a slot.
	if (!res.valid)
		return;
	crossplatform::ContextState* cs = GetContextState(deviceContext);
	unsigned long slot = res.slot;
	if (slot >= 1000)
		slot -= 1000;
	unsigned long dim = res.dimensions;
	auto& ta = cs->rwTextureAssignmentMap[slot];
	ta.resourceType = res.shaderResourceType;
	ta.texture = tex ;
	if(!res.valid)
		ta.texture = nullptr;
	if(tex&&!tex->IsValid())
		ta.texture=nullptr;
	ta.dimensions = dim;
	ta.uav = true;
	ta.subresource = { subresource.aspectMask, subresource.mipLevel, (uint32_t)1, subresource.baseArrayLayer, subresource.arrayLayerCount };;
	cs->rwTextureAssignmentMapValid = false;
}

vec4 platform::crossplatform::ViewportToTexCoordsXYWH(const Viewport *v,const Texture *t)
{

	vec4 texcXYWH;
	if(v&&t)
	{
		texcXYWH.x=(float)v->x/(float)t->width;
		texcXYWH.y=(float)v->y/(float)t->length;
		texcXYWH.z=(float)v->w/(float)t->width;
		texcXYWH.w=(float)v->h/(float)t->length;
	}
	else
	{
		texcXYWH=vec4(0,0,1.f,1.f);
	}
	return texcXYWH;
}

vec4 platform::crossplatform::ViewportToTexCoordsXYWH(const int4 *v,const Texture *t)
{

	vec4 texcXYWH;
	if(v&&t)
	{
		texcXYWH.x=(float)v->x/(float)t->width;
		texcXYWH.y=(float)v->y/(float)t->length;
		texcXYWH.z=(float)v->z/(float)t->width;
		texcXYWH.w=(float)v->w/(float)t->length;
	}
	else
	{
		texcXYWH=vec4(0,0,1.f,1.f);
	}
	return texcXYWH;
}

void platform::crossplatform::DrawGrid(GraphicsDeviceContext &deviceContext,vec3 centrePos,float square_size,float brightness,int numLines)
{
	// 101 lines across, 101 along.
	numLines++;
	crossplatform::PosColourVertex *lines=new crossplatform::PosColourVertex[2*numLines*2];
	// one metre apart
	crossplatform::PosColourVertex *vertex=lines;
	int halfOffset=numLines/2;
	float l10=brightness;
	float l5=brightness*0.5f;
	float l25=brightness*2.5f;
	for(int i=0;i<numLines;i++)
	{
		int j=i-numLines/2;
		vec3 pos1		=centrePos+vec3(square_size*j	,-square_size*halfOffset	,0);
		vec3 pos2		=centrePos+vec3(square_size*j	, square_size*halfOffset	,0);
		vertex->pos		=pos1;
		vertex->colour	=vec4(l5,0.5f*l5,0.f,0.2f);
		vertex++;
		vertex->pos		=pos2;
		vertex->colour	=vec4(l5,0.1f*l5,0.f,0.2f);
		vertex++;
	}
	for(int i=0;i<numLines;i++)
	{
		int j=i-numLines/2;
		vec3 pos1		=centrePos+vec3(-square_size*halfOffset	,square_size*j	,0);
		vec3 pos2		=centrePos+vec3( square_size*halfOffset	,square_size*j	,0);
		vertex->pos		=pos1;
		vertex->colour	=vec4(0.f,l10,0.1f*l5,0.2f);
		vertex++;
		vertex->pos		=pos2;
		vertex->colour	=vec4(0.f,l25,0.1f*l5,0.2f);
		vertex++;
	}
	deviceContext.renderPlatform->DrawLines(deviceContext,lines,2*numLines*2,false,true);
	delete[] lines;
}

void RenderPlatform::SetStandardRenderState	(DeviceContext &deviceContext,StandardRenderState s)
{
	SetRenderState(deviceContext,standardRenderStates[s]);
}