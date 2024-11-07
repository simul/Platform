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

#include <algorithm>

#if PLATFORM_STD_FILESYSTEM==0
#define SIMUL_FILESYSTEM 0
#elif PLATFORM_STD_FILESYSTEM==1
#define SIMUL_FILESYSTEM 1
#include <filesystem>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <fmt/core.h>

using namespace std::literals;
using namespace std::string_literals;
using namespace std::literals::string_literals;

#if PLATFORM_IMPLEMENT_STB_IMAGE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifdef _MSC_VER
#define __STDC_LIB_EXT1__
#endif

// Disambiguate stb usage, prevent double-definition errors at link time.
namespace platform
{
	#include "Platform/External/stb/stb_image.h"
	#include "Platform/External/stb/stb_image_write.h"
}


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
	for(uint8_t i=0;i<4;i++)
	{
		resourceGroupLayouts[i]={0};
	}
	auto &perPassLayout = resourceGroupLayouts[PER_PASS_RESOURCE_GROUP];
	perPassLayout.constantBufferSlots = ~uint64_t(0);
}

RenderPlatform::~RenderPlatform()
{
	numPlatforms--;

	recompileThreadActive=false;
	while (!(effectCompileThread.joinable() && recompileThreadFinished))
	{
	}
	effectCompileThread.join();

	allocator.Shutdown();
	InvalidateDeviceObjects();
	delete gpuProfiler;
}

static bool RewriteOutput(std::string str)
{
	std::cerr<<str.c_str();
	return true;
}

static std::string recompiling_effect_names;

void RenderPlatform::recompileAsync()
{
	while(recompileThreadActive)
	{
		// Deal with effectsToCompileFutures
		{
			recompiling_effect_names.clear();

			std::lock_guard recompileEffectFutureGuard(recompileEffectFutureMutex);
			for (auto it = effectsToCompileFutures.begin(); it != effectsToCompileFutures.end();)
			{
				const std::string &effect_name = it->first;
				std::future<EffectRecompile> &future = it->second;

				if (future.valid())
				{
					std::future_status status = future.wait_for(10ms);
					if (status == std::future_status::ready)
					{
						const EffectRecompile &effectRecompile = future.get();
						if (effectRecompile.callback)
							effectRecompile.callback();

						SIMUL_COUT << "Effect " << effectRecompile.effect_name << ".sfx has recompiled." << std::endl;
					}
					else if (status == std::future_status::timeout)
					{
						if (!recompiling_effect_names.empty())
							recompiling_effect_names += ", ";
						recompiling_effect_names += effect_name;
					}

					it++;
				}
				else
				{
					it = effectsToCompileFutures.erase(it);
				}
			}
		}

		if(!effectsToCompile.size())
		{
			std::this_thread::sleep_for(1000ms);
			std::this_thread::yield();
			continue;
		}
		
		//Clear effectsToCompile
		{
			std::lock_guard recompileEffectGuard(recompileEffectMutex);
			for (const auto &effectToCompile : effectsToCompile)
			{
				auto RecompileEffectAsync = [&](EffectRecompile effectRecompile) -> EffectRecompile 
					{
						if (effectRecompile.effect_name.length())
						{
							if(RecompileEffect(effectRecompile.effect_name))
							{
								GetOrCreateEffect(effectRecompile.effect_name.c_str(), true);
							}
						}
						return effectRecompile;
					};

				std::lock_guard recompileEffectFutureGuard(recompileEffectFutureMutex);
				effectsToCompileFutures[effectToCompile.effect_name] = std::async(std::launch::async, RecompileEffectAsync, effectToCompile);
			}

			effectsToCompile.clear();
		}

	}
	
	recompileThreadFinished = true;
}

float RenderPlatform::GetRecompileStatus(std::string &txt)
{
	txt = recompiling_effect_names;
	std::lock_guard recompileEffectFutureGuard(recompileEffectFutureMutex);
	return (float)effectsToCompileFutures.size();
}

void RenderPlatform::ScheduleRecompileEffects(const std::vector<std::string> &effect_names, std::function<void()> f)
{
	std::lock_guard recompileEffectGuard(recompileEffectMutex);
	std::lock_guard recompileEffectFutureGuard(recompileEffectFutureMutex);
	bool pushedBack = false;
	for (const std::string &effect_name : effect_names)
	{
		bool found = false;
		for (const auto &effectToCompile : effectsToCompile)
		{
			if (effectToCompile.effect_name == effect_name 
				|| effectsToCompileFutures.find(effectToCompile.effect_name) != effectsToCompileFutures.end())
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			effectsToCompile.push_back({effect_name, nullptr});
			pushedBack = true;
		}
	}

	if (pushedBack)
	{
		effectsToCompile.back().callback = f;
	}
}

bool RenderPlatform::RecompileEffect(std::string effect_filename)
{
#if defined(WIN32) && !defined(_GAMING_XBOX)
	std::string filename_fx=effect_filename;
	size_t dot_pos=filename_fx.find(".");
	size_t len=filename_fx.size();
	if(dot_pos>=len)
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
		return false;
	}
	else if(index<GetShaderPathsUtf8().size())
		filenameInUseUtf8=(GetShaderPathsUtf8()[index]+"/")+filename_fx;
	std::string shaderbin = GetShaderBinaryPathsUtf8().back();
	std::string exe_dir = platform::core::GetExeDirectory();
	std::string SIMUL= std::filesystem::weakly_canonical((exe_dir + "/../../..").c_str()).generic_string();
	std::string cmake_binary_dir = PLATFORM_STRING_OF_MACRO(CMAKE_BINARY_DIR);
	std::string platform_source_dir = PLATFORM_STRING_OF_MACRO(PLATFORM_SOURCE_DIR);
	auto bin_to_platform=std::filesystem::relative(std::filesystem::path(platform_source_dir),std::filesystem::path(cmake_binary_dir)).generic_string();
	std::string PLATFORM= std::filesystem::weakly_canonical((exe_dir + "/../../"s+bin_to_platform).c_str()).generic_string();
	std::string BUILD_DIR= std::filesystem::weakly_canonical((exe_dir + "/../..").c_str()).generic_string();
#ifdef _MSC_VER
	std::string sfxcmd=BUILD_DIR+"/bin/Release/Sfx.exe";
#else
	std::string sfxcmd=BUILD_DIR+"/bin/Sfx";
#endif
	std::string name=std::string(GetName());
	std::string json_file=PLATFORM+"/"s+GetSfxConfigFilename();
	std::string this_platform_dir=std::filesystem::path(json_file).parent_path().generic_string();
	std::string command=sfxcmd+fmt::format(" -I\"{PLATFORM}/../..;{PLATFORM}/..;{PLATFORM};{this_platform_dir};{PLATFORM}/CrossPlatform/Shaders\""
											" -O\"{shaderbin}\""
												" -P\"{json_file}\""
												" -m\"{shaderbin}/intermediate\" "
												,fmt::arg("PLATFORM", PLATFORM)
												,fmt::arg("this_platform_dir",this_platform_dir)
												,fmt::arg("shaderbin", shaderbin)
												,fmt::arg("json_file", json_file));
	command+= std::string(" -EPLATFORM=") + PLATFORM;
	if ((buildMode & crossplatform::ALWAYS_BUILD) != 0)
		command+=" -F";
	if (platform::core::SimulInternalChecks)
		command += " -V";
	command+=" "s+filenameInUseUtf8.c_str();
	platform::core::find_and_replace(command,"{SIMUL}",SIMUL);

	platform::core::OutputDelegate cc=std::bind(&RewriteOutput,std::placeholders::_1);
	bool result= platform::core::RunCommandLine(command.c_str(),  cc);
	
	return result;
#else
	return false;
#endif
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
		if (binary_dir.length())
		{
			std::string shader_binary_path = binary_dir + "/"s + render_platform + "/shaderbin"s;
			std::string platform_build_path = binary_dir + "/Platform"s;
			std::string this_platform_build_path = platform_build_path+"/"s + render_platform;
			PushShaderBinaryPath((binary_dir + "/shaderbin/"s+render_platform).c_str());
			PushTexturePath((source_dir + "/Resources/Textures").c_str());
			PushTexturePath((source_dir + "/Media/textures").c_str());
		}
		std::string cmake_binary_dir = PLATFORM_STRING_OF_MACRO(PLATFORM_BUILD_DIR);
		std::string cmake_source_dir = PLATFORM_STRING_OF_MACRO(CMAKE_SOURCE_DIR);
		std::string platform_source_dir = PLATFORM_STRING_OF_MACRO(PLATFORM_SOURCE_DIR);
		if (cmake_binary_dir.length())
		{
			std::string platform_build_path = ((cmake_binary_dir + "/Platform/") + GetPathName());
			PushTexturePath((cmake_source_dir + "/Resources/Textures").c_str());
		}
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
	
	debugEffect=GetOrCreateEffect("debug");
	solidEffect=GetOrCreateEffect("solid");
	copyEffect=GetOrCreateEffect("copy");
	mipEffect=GetOrCreateEffect("mip");
	
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
}

void RenderPlatform::InvalidateDeviceObjects()
{
	if(gpuProfiler)
		gpuProfiler->InvalidateDeviceObjects();
	if(textRenderer)
		textRenderer->InvalidateDeviceObjects();
	for(std::map<StandardRenderState,RenderState*>::iterator i=standardRenderStates.begin();i!=standardRenderStates.end();i++)
		SAFE_DELETE(i->second);
	standardRenderStates.clear();
	SAFE_DELETE(textRenderer);
	debugConstants.InvalidateDeviceObjects();

	debugEffect = nullptr;
	solidEffect = nullptr;
	copyEffect = nullptr;
	mipEffect = nullptr;
	
	textured=nullptr;
	untextured=nullptr;
	showVolume=nullptr;
	textureQueryResult.InvalidateDeviceObjects();
	
	for (auto i = materials.begin(); i != materials.end(); i++)
	{
		Material *mat = i->second;
		mat->InvalidateDeviceObjects();
		delete mat;
	}
	materials.clear();
	for (auto &debugVertexBuffer : debugVertexBuffers)
	{
		debugVertexBuffer->InvalidateDeviceObjects();
	}
	debugVertexBuffers.clear();
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
	for (auto &effect : effects)
	{
		effect.second = nullptr;
	}
	effects.clear();
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
	debugEffect=GetOrCreateEffect("debug");
	solidEffect=GetOrCreateEffect("solid");
	copyEffect=GetOrCreateEffect("copy");
	mipEffect=GetOrCreateEffect("mip");
	
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
}

void RenderPlatform::PushTexturePath(const char *path_utf8)
{
#if SIMUL_FILESYSTEM
	std::filesystem::path path(path_utf8);
	std::error_code ec;
	auto canonical=std::filesystem::weakly_canonical(path,ec);
	std::string str = canonical.generic_string();
	char c = 'x';
	if (str.length())
		c = str.back();
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

void RenderPlatform::ApplyResourceGroup(DeviceContext &deviceContext, uint8_t g)
{
#if SIMUL_INTERNAL_CHECKS
	if(g>=3)
	{
		SIMUL_BREAK_ONCE("Invalid resource group for RenderPlatform::ApplyResourceGroup");
	}
#endif
	deviceContext.contextState.resourceGroupApplyCounter[g]++;
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
	if (deviceContext.targetStack.empty() && deviceContext.defaultTargetsAndViewport.m_rt)
	{
		crossplatform::EffectTechnique *clearTechnique = debugEffect->GetTechniqueByName("clear");
		if (deviceContext.AsMultiviewGraphicsDeviceContext())
			clearTechnique = debugEffect->GetTechniqueByName("clear_multiview");

		debugConstants.debugColour = colour_rgba;
		SetConstantBuffer(deviceContext, &debugConstants);
		debugEffect->Apply(deviceContext, clearTechnique, 0);
		DrawQuad(deviceContext);
		debugEffect->Unapply(deviceContext);
	}
	else
	{
		for (size_t i = 0; i < 8; i++)
		{
			crossplatform::Texture* texture = deviceContext.targetStack.top()->textureTargets[i].texture;
			if (texture)
			{
				texture->ClearColour(deviceContext, colour_rgba);
			}
		}
	}
}

void RenderPlatform::ClearTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,const vec4& colour)
{
	// Silently return if not initialized
	if(!texture->IsValid())
		return;

	auto *graphicsDeviceContext=deviceContext.AsGraphicsDeviceContext();
	if (graphicsDeviceContext != nullptr)
	{
		if (texture->IsDepthStencil())
		{
			texture->ClearDepthStencil(*graphicsDeviceContext, colour.x, 0);
		}
		else 
		{
			texture->ClearColour(*graphicsDeviceContext, colour);
		}
	}
}

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
		TextureView textureView;
		textureView.elements = { t->GetShaderResourceTypeForRTVAndDSV(),
								 {TextureAspectFlags::COLOUR, uint8_t(m1), 1, uint8_t(array_idx), 1} };
		t->activateRenderTarget(deviceContext,textureView);
		SetTexture(deviceContext, _imageTexture, t, {TextureAspectFlags::COLOUR, 0, 1, uint8_t(array_idx), 1});
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
	SetConstantBuffer(deviceContext,&debugConstants);
	textureQueryResult.ApplyAsUnorderedAccessView(deviceContext,debugEffect->GetShaderResource("textureQueryResults"));
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

void RenderPlatform::SetResourceGroupLayout(uint8_t group_index, ResourceGroupLayout l)
{
#if defined(_DEBUG) || defined(SIMUL_INTERNAL_CHECKS)
	if(group_index>=PER_PASS_RESOURCE_GROUP)
	{
		SIMUL_BREAK_ONCE("Invalid resource group index");
		return;
	}
	else
	{
	}
#endif
	resourceGroupLayouts[group_index] = l;
	// recreate octave layout 0
	auto &perPassLayout = resourceGroupLayouts[PER_PASS_RESOURCE_GROUP];
	// initially, use all 64 slots.
	perPassLayout.constantBufferSlots = ~uint64_t(0);
	perPassLayout.readOnlyResourceSlots = ~uint64_t(0);
	perPassLayout.samplerSlots = ~uint64_t(0);
	for (uint8_t i = 0; i < PER_PASS_RESOURCE_GROUP; i++)
	{
		auto &layout = resourceGroupLayouts[i];
		perPassLayout.constantBufferSlots &= (~(layout.constantBufferSlots));
		perPassLayout.readOnlyResourceSlots &= (~(layout.readOnlyResourceSlots));
		perPassLayout.samplerSlots &= (~(layout.samplerSlots));
	}
}
const ResourceGroupLayout &RenderPlatform::GetResourceGroupLayout(uint8_t group_index) const
{
	return resourceGroupLayouts[group_index];
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
	SetConstantBuffer(deviceContext,&debugConstants);
	ApplyPass(deviceContext,pass);
	TextureView tv;
	tv.elements = {normalMap->GetShaderResourceTypeForRTVAndDSV(), TextureAspectFlags::COLOUR, 0, 1, 0, 1};
	normalMap->activateRenderTarget(deviceContext,tv);
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

std::shared_ptr<crossplatform::Effect> RenderPlatform::GetDebugEffect()
{
	return debugEffect;
}

std::shared_ptr<crossplatform::Effect> RenderPlatform::GetCopyEffect()
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

void RenderPlatform::DrawLine(GraphicsDeviceContext &deviceContext,vec3 startp, vec3 endp, vec4 colour,float width)
{
	debugConstants.line_start=startp;
	debugConstants.line_end=endp;
	debugConstants.debugColour=colour;
	mat4 wvp;
	crossplatform::MakeViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	debugConstants.debugWorldViewProj=wvp;//deviceContext.viewStruct.viewProj;
	debugConstants.debugWorldViewProj.transpose();
	SetConstantBuffer(deviceContext,&debugConstants);
	SetLayout(deviceContext,posColourLayout.get());
	debugEffect->Apply(deviceContext,"lines_3d","lines3d_novb");
	SetTopology(deviceContext,Topology::LINELIST);
	Draw(deviceContext, 2, 0);
	debugEffect->Unapply(deviceContext);
	
	if (posColourLayout)
		posColourLayout->Unapply(deviceContext);
}

void RenderPlatform::DrawLines(GraphicsDeviceContext &deviceContext,PosColourVertex * lines,int count,bool strip,bool test_depth,bool view_centred)
{
	static uint64_t previousFrameNumber = 0;
	uint64_t currentFrameNumber = deviceContext.renderPlatform->GetFrameNumber();

	static uint64_t vertexBufferIdx = 0; //Get new vertex buffer each DrawLines render.

	if (currentFrameNumber != previousFrameNumber)
	{
		previousFrameNumber = currentFrameNumber;
		vertexBufferIdx = 0;
	}
	if (!(vertexBufferIdx < debugVertexBuffers.size()))
		debugVertexBuffers.resize(vertexBufferIdx + 1);

	std::shared_ptr<Buffer> &debugVertexBuffer = debugVertexBuffers[vertexBufferIdx++];
	if (!debugVertexBuffer)
	{
		debugVertexBuffer.reset(CreateBuffer());
	}
	// Create and grow vertex/index buffers if needed
	if ( debugVertexBuffer->count < count)
	{
		if(!posColourLayout)
		{
			crossplatform::LayoutDesc local_layout[] =
			{
				{ "POSITION", 0, crossplatform::RGB_32_FLOAT,	0, (uint32_t)0, false, 0 },
				{ "TEXCOORD", 0, crossplatform::RGBA_32_FLOAT,	0, (uint32_t)12,  false, 0 },
			};
			posColourLayout.reset(CreateLayout(2,local_layout,true));
		}
		debugVertexBuffer->EnsureVertexBuffer(this, 2*count, posColourLayout.get(), nullptr,true);
	}
	// Upload vertex/index data into a single contiguous GPU buffer
	PosColourVertex* vtx_dst= (PosColourVertex*)debugVertexBuffer->Map(deviceContext);
	if (!vtx_dst)
	{
	// Force recreate to find error.
		debugVertexBuffer.reset();
		return;
	}
	memcpy(vtx_dst,lines,count*sizeof(PosColourVertex));
	debugVertexBuffer->Unmap(deviceContext);

	mat4 wvp;
	if (view_centred)
		crossplatform::MakeCentredViewProjMatrix((float *)&wvp, deviceContext.viewStruct.view, deviceContext.viewStruct.proj);
	else
		crossplatform::MakeViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	debugConstants.debugWorldViewProj=wvp;//deviceContext.viewStruct.viewProj;
	debugConstants.debugWorldViewProj.transpose();
	auto *b=debugVertexBuffer.get();
	SetVertexBuffers(deviceContext,0,1,&b,posColourLayout.get());
	SetConstantBuffer(deviceContext,&debugConstants);
	SetLayout(deviceContext,posColourLayout.get());
	debugEffect->Apply(deviceContext,"lines_3d","lines3d_nodepth");
	SetTopology(deviceContext, strip?Topology::LINESTRIP:Topology::LINELIST);
	Draw(deviceContext, count, 0);
	SetVertexBuffers(deviceContext,0,0,nullptr,nullptr);
	debugEffect->Unapply(deviceContext);
	posColourLayout->Unapply(deviceContext);
}

void RenderPlatform::DrawCircle(GraphicsDeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill,bool view_centred)
{
	vec3 pos = view_centred ? vec3d(0, 0, 0) : GetCameraPosVector(deviceContext.viewStruct.view);
	vec3 d=dir;
	d/=length(d);
	pos += 1.5f * d;
	float radius = 1.5f * rads;
	DrawCircle(deviceContext,pos,dir,radius,colr,fill,view_centred);
}

void RenderPlatform::DrawCircle(GraphicsDeviceContext &deviceContext,const double *dir,double rads,const float *colr,bool fill,bool view_centred)
{
	vec3d pos = view_centred ? vec3d(0,0,0) : GetCameraPosVector(deviceContext.viewStruct.view);
	vec3d d=dir;
	d/=length(d);
	pos += 1.5 * d;
	double radius = 1.5 * rads;
	DrawCircle(deviceContext,pos,dir,radius,colr,fill,view_centred);
}

void RenderPlatform::DrawCircle(GraphicsDeviceContext &deviceContext,const float *pos,const float *dir,float radius,const float *colr,bool fill,bool view_centred)
{
	PosColourVertex line_vertices[36];
	vec3 direction(dir);
	direction = normalize(direction);
	vec3 z(0,0,1.f);
	vec3 y(0,1.f,0);
	vec3 x=cross(z,direction);
	if(length(x)>.1f)
		x=normalize(x);
	else
		x=cross(direction,y);
	x*=radius;
	y = cross(direction , x);
	int l=0;
	for(int j=0;j<_countof(line_vertices);j++)
	{
		float angle					=(float(j)/float(_countof(line_vertices)-1))*2.0f*3.1415926536f;
		vec3 p						=vec3((x * cos(angle) + y * sin(angle)));
		line_vertices[l].pos		=vec3(pos)+p;
		line_vertices[l++].colour	=colr;
	}
	DrawLines(deviceContext, line_vertices, _countof(line_vertices), true, false, view_centred);
}

void RenderPlatform::DrawCircle(GraphicsDeviceContext &deviceContext, const double *pos, const double *dir, double radius, const float *colr, bool fill, bool view_centred)
{
	PosColourVertex line_vertices[36];
	vec3d direction(dir);
	direction = normalize(direction);
	vec3d z(0, 0, 1.0);
	vec3d y(0, 1.0, 0);
	vec3d x = cross(z, direction);
	if (length(x) > 0.1)
		x = normalize(x);
	else
		x = cross(direction, y);
	x *= radius;
	y = cross(direction, x);
	int l = 0;
	for (int j = 0; j < _countof(line_vertices); j++)
	{
		double angle = (double(j) / float(_countof(line_vertices) - 1)) * 2.0 * 3.1415926536;
		vec3d p = vec3d((x * cos(angle) + y * sin(angle)));
		line_vertices[l].pos = vec3(vec3d(pos) + p);
		line_vertices[l++].colour = colr;
	}
	DrawLines(deviceContext, line_vertices, _countof(line_vertices), true, false, view_centred);
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
			if(tex->LoadFromFile(this, fileNameUtf8, gen_mips))
			{
				unfinishedTextures.insert(tex);
				SIMUL_INTERNAL_COUT <<"unfinishedTexture: "<<tex<<" "<<fileNameUtf8<<std::endl;
			}
			else
			{
				SIMUL_INTERNAL_CERR<<"Failed to load texture: {0}"<<fileNameUtf8<<"\n";
			}
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

void RenderPlatform::DrawCubemap(GraphicsDeviceContext &deviceContext,Texture *cubemap,int x,int y,int pixelSize,float exposure,float gamma,float displayMip)
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
	debugConstants.displayMip=displayMip;
	SetTexture(deviceContext,debugEffect_cubeTexture,cubemap);
	static float rr=6.f;
	debugConstants.latitudes		=16;
	debugConstants.longitudes		=32;
	debugConstants.radius			=rr;
	debugConstants.multiplier		=vec4(exposure,exposure,exposure,0.0f);
	debugConstants.debugGamma		=gamma;
	SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,draw_cubemap_sphere,0);

	SetTopology(deviceContext,Topology::TRIANGLESTRIP);
	Draw(deviceContext, (debugConstants.longitudes+1)*(debugConstants.latitudes+1)*2, 0);

	SetTexture(deviceContext, debugEffect_cubeTexture, nullptr);
	debugEffect->Unapply(deviceContext);
	SetViewports(deviceContext,1,&oldv);
}

void RenderPlatform::DrawCubemap(GraphicsDeviceContext &deviceContext,Texture *cubemap,float offsetx,float offsety,float size,float exposure,float gamma,float displayMip)
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
	debugConstants.displayMip=displayMip;
	SetTexture(deviceContext,debugEffect_cubeTexture,cubemap);
	static float rr=6.f;
	debugConstants.latitudes		=16;
	debugConstants.longitudes		=32;
	debugConstants.radius			=rr;
	debugConstants.multiplier		=vec4(exposure,exposure,exposure,0.0f);
	debugConstants.debugGamma		=gamma;
	SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,draw_cubemap_sphere,0);

	SetTopology(deviceContext,Topology::TRIANGLESTRIP);
	Draw(deviceContext, (debugConstants.longitudes+1)*(debugConstants.latitudes+1)*2, 0);

	SetTexture(deviceContext, debugEffect_cubeTexture, nullptr);
	debugEffect->Unapply(deviceContext);
	SetViewports(deviceContext,1,&oldv);
}

void RenderPlatform::DrawAxes(GraphicsDeviceContext &deviceContext,const mat4 &m,float size)
{
	mat4 wvp;
	crossplatform::MakeWorldViewProjMatrix((float*)&wvp,m,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	debugConstants.debugWorldViewProj=wvp;
	debugConstants.radius			=size;
	SetConstantBuffer(deviceContext,&debugConstants);
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

int2 RenderPlatform::DrawTexture(GraphicsDeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, vec4 mult, bool blend, float gamma, bool debug, vec2 texc, vec2 texc_scale, float mip, int layer)
{
	if (tex && !tex->IsValid())
		tex = nullptr;

	if (tex!=nullptr&&dy == 0&& tex->width>0)
	{
		dy = (dx * tex->length) / tex->width;
	}

	static int frames = 25;
	static unsigned long long framenumber = 0;

	float displayMip = mip;
	static int _mip = 0;
	static int mipFramesCount = frames;
	if (mip < 0)
	{
		if(framenumber!=GetFrameNumber())
		{
			mipFramesCount--;
			if (!mipFramesCount)
			{
				_mip++;
				mipFramesCount = frames;
			}
			framenumber=GetFrameNumber();
		}
		if(tex)
		{
			const int& m = tex->GetMipCount();
			displayMip = float((_mip % (m ? m : 1)));
		}
	}

	int displayLayer = layer;
	static int _layer = 0;
	static int layerFramesCount = frames;
	auto UpdateDisplayLayer = [&](const int &maxLayers) -> uint {
		if (framenumber != GetFrameNumber())
		{
			layerFramesCount--;
			if (!layerFramesCount)
			{
				_layer++;
				layerFramesCount = frames;
			}
		}
		displayLayer = _layer % std::max(1, maxLayers);
		return (uint)displayLayer;
	};
	
	debugConstants.debugGamma=gamma;
	debugConstants.multiplier=mult;
	debugConstants.displayMip = displayMip;
	debugConstants.displayLayer = displayLayer;
	debugConstants.debugTime = float(this->frameNumber);

	if(texc_scale.x==0||texc_scale.y==0)
	{
		texc_scale.x=1.0f;
		texc_scale.y=1.0f;
	}
	debugConstants.viewport={texc.x,texc.y,texc_scale.x,texc_scale.y};

	crossplatform::EffectTechnique *tech= nullptr;

	if(tex&&tex->GetDimension()==3)
	{
		if (debug)
		{
			tech = debugEffect->GetTechniqueByName("trace_volume");
			debugConstants.displayLayer = UpdateDisplayLayer(tex->depth);
			}
		else
		{
			tech=showVolume;
		}

		SetTexture(deviceContext,volumeTexture,tex);
	}
	else if(tex&&tex->IsCubemap())
	{
		if(tex->arraySize>1)
		{
			tech=debugEffect->GetTechniqueByName("show_cubemap_array");
			debugEffect->SetTexture(deviceContext, "cubeTextureArray", tex, {TextureAspectFlags::COLOUR, (uint8_t)displayMip, 1, 0, (uint8_t)-1});
			if(debug)
			{
				debugConstants.displayLayer = UpdateDisplayLayer(tex->arraySize);
				}
			}
		else
		{
			tech=debugEffect->GetTechniqueByName("show_cubemap");
			debugEffect->SetTexture(deviceContext, "cubeTexture", tex, {TextureAspectFlags::COLOUR, (uint8_t)displayMip, 1, 0, (uint8_t)-1});
			debugConstants.displayLayer = 0;
		}
	}
	else if(tex&&tex->arraySize>1)
	{
		tech = debugEffect->GetTechniqueByName("show_texture_array");
		debugEffect->SetTexture(deviceContext, "imageTextureArray", tex, {TextureAspectFlags::COLOUR, (uint8_t)displayMip, 1, 0, (uint8_t)-1});
		debugConstants.displayLayer = UpdateDisplayLayer(tex->arraySize);
			}
	else if(tex)
	{
		tech = textured;
		SetTexture(deviceContext, imageTexture, tex, {TextureAspectFlags::COLOUR, (uint8_t)displayMip, 1, 0, (uint8_t)-1});
	}
	else
	{
		tech=untextured;
	}

	DrawQuad(deviceContext,x1,y1,dx,dy,debugEffect.get(),tech,blend?"blend":"noblend");
	debugEffect->UnbindTextures(deviceContext);
	
	if(debug)
	{
		vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
		vec4 semiblack(0.0f, 0.0f, 0.0f, 0.5f);
		char mip_txt[]="MIP: 0";
		char lyr_txt[] = "LYR: 0";
		if (tex && tex->GetMipCount() > 1 && displayMip > 0 && displayMip < 10)
		{
			mip_txt[5] = '0' + (char)displayMip;
			Print(deviceContext,x1,y1+20,mip_txt,white,semiblack);
		}
		if(tex&&tex->arraySize>1)
		{
			if (displayLayer < 10)
				lyr_txt[5] = '0' + (displayLayer);
			Print(deviceContext, x1 + 60, y1 + 20, lyr_txt, white, semiblack);
		}
		if (tex&&tex->GetDimension() == 3)
		{
			Print(deviceContext, x1, y1 + 20, ("Z: " + std::to_string(displayLayer)).c_str(), white, semiblack);
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
	debugConstants.rect=r;
	SetConstantBuffer(deviceContext,&debugConstants);
	effect->Apply(deviceContext,technique,pass);
	DrawQuad(deviceContext);
	effect->Unapply(deviceContext);
}

int2 RenderPlatform::DrawTexture(GraphicsDeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, float mult, bool blend, float gamma, bool debug, vec2 texc, vec2 texc_scale, float mip, int layer)
{
	return DrawTexture(deviceContext, x1, y1, dx, dy, tex, vec4(mult, mult, mult, 0.0f), blend, gamma, debug, texc, texc_scale, mip, layer);
}

int2 RenderPlatform::DrawDepth(GraphicsDeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, const crossplatform::Viewport *v, const float *proj)
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
		SetConstantBuffer(deviceContext,&debugConstants);
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
	pts[0].pos={pos1.x,pos1.y,0};
	pts[0].colour=colour;
	pts[1].pos = {pos2.x, pos2.y, 0};
	pts[1].colour=colour;
	Draw2dLines(deviceContext,pts,2,false);
}

int RenderPlatform::Print(GraphicsDeviceContext& deviceContext, int x, int y, const char* text, const float* colr, const float* bkg)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, "Print")
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
	SIMUL_COMBINED_PROFILE_START(deviceContext, "Print")
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
	{
		tv->viewport=*vps;
		deviceContext.contextState.viewportsChanged=true;
	}
}

void RenderPlatform::SetScissor(GraphicsDeviceContext &deviceContext,int4 sc)
{
	deviceContext.contextState.scissor = sc;
	deviceContext.contextState.scissorChanged = true;
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
		ss=desc?CreateSamplerState(desc):nullptr;
		sharedSamplerStates[str]=ss;
	}
	return ss;
}

std::shared_ptr<Effect> RenderPlatform::GetOrCreateEffect(const char *filename_utf8, bool createOnly)
{
	std::string fn(filename_utf8);

	if (!createOnly)
	{
		//Check if the effect in being recompiled
		std::lock_guard recompileEffectFutureGuard(recompileEffectFutureMutex);
		const auto &it = effectsToCompileFutures.find(fn);
		if (it != effectsToCompileFutures.end())
		{
			if (it->second.valid())
			{
				effectsToCompileFutures.erase(it);
				return effects[fn];
			}
			else
			{
				effectsToCompileFutures.erase(it);
			}
		}

		//Else, check if the effect is already loaded.
		auto i = effects.find(filename_utf8);
		if (i != effects.end())
			return i->second;
	}

	//Else, load as normal
	std::shared_ptr<Effect> e;
	e.reset(CreateEffect());
	e->SetName(filename_utf8);
	effects[fn] = e;
	bool success = e->Load(this,filename_utf8);
	if (!success)
	{
		SIMUL_BREAK(platform::core::QuickFormat("Failed to load effect file: %s. Effect will be placeholder.\n", filename_utf8));
		return e;
	}
	return e;
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
/*	if (ta.texture)
	{
		if ((uint32_t)ta.subresource.baseMipLevel + (uint32_t)ta.subresource.mipLevelCount >= ta.texture->mips)
		{
			ta.subresource.baseMipLevel = (uint8_t)(ta.texture->mips - (int32_t)ta.subresource.mipLevelCount);
			if ((uint32_t)ta.subresource.baseMipLevel + (uint32_t)ta.subresource.mipLevelCount >= (uint32_t)ta.texture->mips)
			{
				ta.subresource.baseMipLevel=0;
				ta.subresource.mipLevelCount = ta.texture->mips;
			}
		}
	}*/
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
#if SIMUL_INTERNAL_CHECKS
	SIMUL_ASSERT_WARN_ONCE(subresource.arrayLayerCount>0,"Must have at least one array layer in a subresourceLayers spec.");
#endif
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
	ta.subresource = { subresource.aspectMask, subresource.mipLevel, (uint32_t)1, subresource.baseArrayLayer, subresource.arrayLayerCount };
	cs->rwTextureAssignmentMapValid = false;
}

void RenderPlatform::SetSamplerState(DeviceContext &deviceContext, int slot, SamplerState *s)
{
	if (slot > 31 || slot < 0)
		return;
	crossplatform::ContextState &cs = deviceContext.contextState;
	cs.samplerStateOverrides[slot] = s;
	cs.samplerStateOverridesValid = false;
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