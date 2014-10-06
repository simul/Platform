// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulCloudRendererDX1x.cpp A renderer for 3d clouds.

#define NOMINMAX
#include "SimulCloudRendererDX1x.h"
#include <fstream>
#include <math.h>
#include <tchar.h>
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#endif
#include <string>
#include <algorithm>			// for std::min / max

#include "Simul/Base/Timer.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Math/Pi.h"
#include "Simul/Camera/Camera.h"						// for simul::camera::Frustum

#include "Simul/Math/Noise3D.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/DX11Exception.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Base/StringFunctions.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

const char *GetErrorText(HRESULT hr)
{
	const char *err=DXGetErrorStringA(hr);
	return err;
}

SimulCloudRendererDX1x::SimulCloudRendererDX1x(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem) :
	simul::clouds::BaseCloudRenderer(ck,mem)
{
}

void SimulCloudRendererDX1x::Recompile()
{
	CreateCloudEffect();
	if(!renderPlatform)
		return;
	BaseCloudRenderer::Recompile();
	gpuCloudGenerator.RecompileShaders();
	recompile_shaders=false;
}

void SimulCloudRendererDX1x::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseCloudRenderer::RestoreDeviceObjects(r);
	gpuCloudGenerator.RestoreDeviceObjects(renderPlatform);
	// Allow the GPU cloud generator to directly create and modify the target textures.
	gpuCloudGenerator.SetDirectTargets(cloud_textures);
	RecompileShaders();
	ClearIterators();
}

void SimulCloudRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	BaseCloudRenderer::InvalidateDeviceObjects();
	gpuCloudGenerator.InvalidateDeviceObjects();
	ClearIterators();
}

SimulCloudRendererDX1x::~SimulCloudRendererDX1x()
{
	InvalidateDeviceObjects();
}

static int PowerOfTwo(int unum)
{
	float num=(float)unum;
	if(fabs(num)<1e-8f)
		num=1.f;
	float Exp=log(num);
	Exp/=log(2.f);
	return (int)Exp;
}


bool SimulCloudRendererDX1x::CreateCloudEffect()
{
	if(!renderPlatform)
		return S_OK;
	std::map<std::string,std::string> defines;
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	defines["DETAIL_NOISE"]			='1';
	defines["REVERSE_DEPTH"]		=ReverseDepth?"1":"0";
	defines["USE_LIGHT_TABLES"]		=UseLightTables?"1":"0";
	std::vector<crossplatform::EffectDefineOptions> opts;
	opts.push_back(crossplatform::CreateDefineOptions("DETAIL_NOISE","1"));
	opts.push_back(crossplatform::CreateDefineOptions("REVERSE_DEPTH","0","1"));
	opts.push_back(crossplatform::CreateDefineOptions("USE_LIGHT_TABLES","0","1"));
	renderPlatform->EnsureEffectIsBuilt("simul_clouds",opts);
	SAFE_DELETE(effect);
	effect							=renderPlatform->CreateEffect("simul_clouds",defines);
	

	cloudConstants.LinkToEffect(effect,"CloudConstants");
	cloudPerViewConstants.LinkToEffect(effect,"CloudPerViewConstants");
	layerConstants.LinkToEffect(effect,"LayerConstants");
	return true;
}

static float saturate(float c)
{
	return std::max(std::min(1.f,c),0.f);
}

void SimulCloudRendererDX1x::PreRenderUpdate(crossplatform::DeviceContext &deviceContext)
{
	if(recompile_shaders)
	{
		Recompile();
	}
    SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"SimulCloudRendererDX1x::PreRenderUpdate")
	EnsureTexturesAreUpToDate(deviceContext);
	SetCloudConstants(cloudConstants);
	cloudConstants.Apply(deviceContext);
	RenderCombinedCloudTexture(deviceContext);
    SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	//set up matrices
// Commented this out and moved to Render as was causing cloud noise problem due to the camera
// matrix it was using being for light probes rather than the main view.
// A global update shouldn't use per view data.
// This needs re-factoring once the view handle work we discussed has been implemented.
// We expect to be able to create views with flags e.g for whether they will render with noise and therefore need to 
// do a per frame view specific update.
// We'll then have a global update and per view updates.
}
#pragma optimize("",off)
void SimulCloudRendererDX1x::RenderTestXXX(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	HRESULT hr=S_OK;
	crossplatform::EffectTechnique*			m_pTechniqueCrossSection		=effect->GetTechniqueByName("cross_section");
	static int test=1;
	if(test>=1)
	{
			effect->SetTexture(deviceContext,"cloudDensity1",cloud_textures[(2+texture_cycle)%3]);
			effect->SetTexture(deviceContext,"cloudDensity2",cloud_textures[(2+texture_cycle)%3]);

		effect->Apply(deviceContext,m_pTechniqueCrossSection,0);
		effect->UnbindTextures(deviceContext);
		effect->Unapply(deviceContext);
	}
}

crossplatform::Texture *SimulCloudRendererDX1x::GetRandomTexture3D()
{
	return noise_texture_3D;
}

void SimulCloudRendererDX1x::EnsureCorrectTextureSizes()
{
	int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	bool uav=gpuCloudGenerator.GetEnabled();
	for(int i=0;i<3;i++)
	{
		cloud_textures[i]->ensureTexture3DSizeAndFormat(renderPlatform,width_x,length_y,depth_z,crossplatform::RGBA_8_UNORM,uav);
	}
	int shadow_tex_size=cloudKeyframer->GetShadowTextureSize();
	shadow_fb->ensureTexture2DSizeAndFormat		(renderPlatform,shadow_tex_size,cloudKeyframer->GetGodraysSteps(),crossplatform::RGBA_8_UNORM,false,true);
	godrays_texture->ensureTexture2DSizeAndFormat(renderPlatform,shadow_tex_size*2,cloudKeyframer->GetGodraysSteps(),crossplatform::R_16_FLOAT,true,false);
	moisture_fb->ensureTexture2DSizeAndFormat	(renderPlatform,shadow_tex_size*2,cloudKeyframer->GetGodraysSteps(),crossplatform::R_16_FLOAT,false,true);
	rain_map->ensureTexture2DSizeAndFormat		(renderPlatform,width_x/2,length_y/2,crossplatform::RGBA_8_UNORM,false,true);
	if(!width_x||!length_y||!depth_z)
		return;
	if(width_x==cloud_tex_width_x&&length_y==cloud_tex_length_y&&depth_z==cloud_tex_depth_z&&cloud_textures[texture_cycle%3]->AsD3D11UnorderedAccessView()!=NULL)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	
	//cloud_texture.ensureTexture3DSizeAndFormat(renderPlatform,width_x,length_y,depth_z,cloud_tex_format,true);
}

void SimulCloudRendererDX1x::EnsureTexturesAreUpToDate(crossplatform::DeviceContext &deviceContext)
{
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	if(FailedNoiseChecksum())
		SAFE_DELETE(noise_texture);
	if(!noise_texture)
		CreateNoiseTexture(deviceContext);
	// We don't need to fill the textures if the gpu Generator has already done so:
	if(gpuCloudGenerator.GetEnabled())
	for(int i=0;i<3;i++)
	{
		int cycled_index=(texture_cycle+i)%3;
		clouds::GpuCloudsParameters g=cloudKeyframer->GetGpuCloudsParameters(i);
		gpuCloudGenerator.Update(cycled_index,g,NULL);
	}
}

