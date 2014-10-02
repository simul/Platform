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

DXGI_FORMAT illumination_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;

struct PosTexVert_t
{
    vec3 position;	
    vec2 texCoords;
};

PosTexVert_t *lightning_vertices=NULL;
#define MAX_VERTICES (12000)

SimulCloudRendererDX1x::SimulCloudRendererDX1x(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem) :
	simul::clouds::BaseCloudRenderer(ck,mem)
	,m_pd3dDevice(NULL)
	,illumination_texture(NULL)
	,blendAndWriteAlpha(NULL)
	,blendAndDontWriteAlpha(NULL)
	,lightning_active(false)
{
	texel_index[0]=texel_index[1]=texel_index[2]=texel_index[3]=0;
}

void SimulCloudRendererDX1x::Recompile()
{
	CreateCloudEffect();
	if(!m_pd3dDevice)
		return;
	
	SAFE_DELETE(blendAndWriteAlpha);
	SAFE_DELETE(blendAndDontWriteAlpha);
	crossplatform::RenderStateDesc desc;
	// two possible blend states for clouds - with alpha written, and without.

	ZeroMemory( &desc, sizeof( crossplatform::RenderStateDesc ) );
	desc.type=crossplatform::BLEND;
	desc.blend.numRTs=1;
	desc.blend.RenderTarget[0].BlendEnable		= true;
	desc.blend.RenderTarget[0].SrcBlend			= crossplatform::BLEND_ONE;
	desc.blend.RenderTarget[0].DestBlend		= crossplatform::BLEND_SRC_ALPHA;
	desc.blend.RenderTarget[0].SrcBlendAlpha	= crossplatform::BLEND_ZERO;
	desc.blend.RenderTarget[0].DestBlendAlpha	= crossplatform::BLEND_SRC_ALPHA;
	desc.blend.RenderTarget[0].RenderTargetWriteMask = 15;
	desc.blend.IndependentBlendEnable			=true;
	desc.blend.AlphaToCoverageEnable			=false;

	blendAndWriteAlpha=renderPlatform->CreateRenderState(desc);

	// write only r g and b:
	desc.blend.RenderTarget[0].RenderTargetWriteMask = 7;
	blendAndDontWriteAlpha=renderPlatform->CreateRenderState(desc);
	gpuCloudGenerator.RecompileShaders();
	recompile_shaders=false;
}

void SimulCloudRendererDX1x::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseCloudRenderer::RestoreDeviceObjects(r);
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	gpuCloudGenerator.RestoreDeviceObjects(renderPlatform);
	// Allow the GPU cloud generator to directly create and modify the target textures.
	gpuCloudGenerator.SetDirectTargets(cloud_textures);
	CreateLightningTexture();
	RecompileShaders();

	ClearIterators();

	SAFE_DELETE(shadow_fb);
	shadow_fb=renderPlatform->CreateTexture();
	SAFE_DELETE(moisture_fb);
	moisture_fb=renderPlatform->CreateTexture();
	SAFE_DELETE(rain_map);
	rain_map=renderPlatform->CreateTexture();
}

void SimulCloudRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	BaseCloudRenderer::InvalidateDeviceObjects();
	gpuCloudGenerator.InvalidateDeviceObjects();
	if(shadow_fb)
		shadow_fb->InvalidateDeviceObjects();
	if(godrays_texture)
		godrays_texture->InvalidateDeviceObjects();
	if(moisture_fb)
		moisture_fb->InvalidateDeviceObjects();
	if(rain_map)
		rain_map->InvalidateDeviceObjects();

	SAFE_DELETE(effect);
	if(cloud_texture)
		cloud_texture->InvalidateDeviceObjects();
	
	SAFE_DELETE(cloud_texture);
	// Set the stored texture sizes to zero, so the textures will be re-created.
	cloud_tex_width_x=cloud_tex_length_y=cloud_tex_depth_z=0;
	SAFE_RELEASE(illumination_texture);
	SAFE_DELETE(blendAndWriteAlpha);
	SAFE_DELETE(blendAndDontWriteAlpha);

	cloudConstants.InvalidateDeviceObjects();
	ClearIterators();
}

bool SimulCloudRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return true;
}

SimulCloudRendererDX1x::~SimulCloudRendererDX1x()
{
	Destroy();
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

bool SimulCloudRendererDX1x::CreateLightningTexture()
{
	HRESULT hr=S_OK;
	return (hr==S_OK);
}

void SimulCloudRendererDX1x::SetIlluminationGridSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	SAFE_RELEASE(illumination_texture);
	D3D11_TEXTURE3D_DESC textureDesc=
	{
		width_x,length_y,depth_z,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0
	};
	HRESULT hr=m_pd3dDevice->CreateTexture3D(&textureDesc,0,&illumination_texture);
}

void SimulCloudRendererDX1x::FillIlluminationSequentially(int source_index,int texel_index,int num_texels,const unsigned char *uchar8_array)
{
	if(!num_texels)
		return;
	unsigned char *lightning_cloud_tex_data=(unsigned char *)(mapped_illumination.pData);
	unsigned *ptr=(unsigned *)(lightning_cloud_tex_data);
	ptr+=texel_index;
	for(int i=0;i<num_texels;i++)
	{
		unsigned ui=*uchar8_array;
		unsigned offset=8*(source_index); // xyzw
		ui<<=offset;
		unsigned msk=255<<offset;
		msk=~msk;
		(*ptr)&=msk;
		(*ptr)|=ui;
		uchar8_array++;
		ptr++;
	}
}

bool SimulCloudRendererDX1x::CreateCloudEffect()
{
	if(!m_pd3dDevice)
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

void SimulCloudRendererDX1x::RenderCombinedCloudTexture(crossplatform::DeviceContext &deviceContext)
{
#if 0
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)context;

	MixCloudsConstants mixCloudsConstants;
	mixCloudsConstants.interpolation=cloudKeyframer->GetInterpolation();
	UPDATE_CONSTANT_BUFFER(pContext,computeConstantBuffer,MixCloudsConstants,mixCloudsConstants);

    // We now set up the shader and run it
    pContext->CSSetShader( m_pComputeShader, NULL, 0 );
	pContext->CSSetConstantBuffers(10,1,&computeConstantBuffer);
	pContext->CSSetShaderResources(0,1,&cloud_textures[(texture_cycle)  %3].shaderResourceView );
    pContext->CSSetShaderResources(1,1,&cloud_textures[(texture_cycle+1)%3].shaderResourceView );
	pContext->CSSetUnorderedAccessViews(0, 1,&cloud_texture.unorderedAccessView,  NULL );

	pContext->Dispatch(cloud_tex_width_x/16,cloud_tex_length_y/16,cloud_tex_depth_z/1);

    pContext->CSSetShader( NULL, NULL, 0 );
	ID3D11UnorderedAccessView *u[]={NULL,NULL};
    pContext->CSSetUnorderedAccessViews( 0, 1, u, NULL );
	ID3D11ShaderResourceView *n[]={NULL,NULL};
	pContext->CSSetShaderResources( 0, 2, n);
	gpu_combine_time*=0.99f;
#endif
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
static int test=29999;
bool SimulCloudRendererDX1x::Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap
									,crossplatform::NearFarPass nearFarPass,crossplatform::Texture *depth_tex
									,bool write_alpha
									,const simul::sky::float4& viewportTextureRegionXYWH
									,const simul::sky::float4& mixedResTransformXYWH)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"SimulCloudRendererDX1x::Render")
	ID3D11DeviceContext* pContext	=deviceContext.asD3D11DeviceContext();
	float blendFactor[]		={0,0,0,0};
	UINT sampleMask			=0xffffffff;
	if(write_alpha)
		renderPlatform->SetRenderState(deviceContext,blendAndWriteAlpha);
		//pContext->OMSetBlendState(blendAndWriteAlpha,blendFactor,sampleMask);
	else
		renderPlatform->SetRenderState(deviceContext,blendAndDontWriteAlpha);
		//pContext->OMSetBlendState(blendAndDontWriteAlpha,blendFactor,sampleMask);
	
	BaseCloudRenderer::Render(deviceContext,exposure,cubemap
				,nearFarPass,depth_tex
				,write_alpha
				,viewportTextureRegionXYWH
				,mixedResTransformXYWH);
	pContext->OMSetBlendState(NULL, blendFactor, sampleMask);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	return (true);
}

void SimulCloudRendererDX1x::RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	static int u=3;
	int w=(width-8)/u;
	if(w>height)
		w=height;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();

	crossplatform::EffectTechnique*			m_pTechniqueCrossSection		=effect->GetTechniqueByName("cross_section");
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	if(skyInterface)
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		h=(int)(kf->cloud_height_km*1000.f/cloudProperties.GetCloudWidth()*(float)w);
		sky::float4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		effect->SetTexture(deviceContext,"cloudDensity1",cloud_textures[(i+texture_cycle)%3]);
		effect->SetTexture(deviceContext,"cloudDensity2",cloud_textures[(i+texture_cycle)%3]);
		effect->SetTexture(deviceContext,"cloudDensity",cloud_textures[(i+texture_cycle)%3]);

		cloudConstants.lightResponse		=light_response;
		cloudConstants.crossSectionOffset	=vec3(0.5f,0.5f,0.f);
		cloudConstants.yz					=0.f;
		cloudConstants.Apply(deviceContext);
		deviceContext.renderPlatform->DrawQuad(deviceContext,x0+i*(w+1)+4,y0+4,w,h,effect,m_pTechniqueCrossSection);
		cloudConstants.yz					=1.f;
		cloudConstants.Apply(deviceContext);
		deviceContext.renderPlatform->DrawQuad(deviceContext,x0+i*(w+1)+4,y0+h+8,w,w,effect,m_pTechniqueCrossSection);
	}
	deviceContext.renderPlatform->Print(deviceContext,x0,w,simul::base::stringFormat("%4.4f",cloudConstants.cloud_interp).c_str());
	effect->Apply(deviceContext,m_pTechniqueCrossSection,0);
	effect->UnbindTextures(deviceContext);
	effect->Unapply(deviceContext);
}

void SimulCloudRendererDX1x::RenderAuxiliaryTextures(simul::crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.platform_context;
	HRESULT hr=S_OK;
	static int u=3;
	int w=(width-8)/u;
	if(w>height)
		w=height;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
	effect->SetTexture(deviceContext,"noiseTexture",noise_texture);
	UtilityRenderer::DrawQuad2(deviceContext				,x0+width-w		,y0+height-w		,w,w		,effect->asD3DX11Effect(),effect->asD3DX11Effect()->GetTechniqueByName("show_noise"));
	renderPlatform->Print(deviceContext						,x0+width-w		,y0+height-w					,"2D Noise");
	effect->SetTexture(deviceContext					,"cloudShadowTexture",shadow_fb);
	effect->SetTexture(deviceContext					,"cloudGodraysTexture",godrays_texture);
	renderPlatform->DrawQuad(deviceContext		,x0+width-w-w	,y0+height-w		,w,w,effect,effect->GetTechniqueByName("show_shadow"));
	renderPlatform->Print(deviceContext			,x0+width-w-w	,y0+height-w					,"shadow texture");

	renderPlatform->DrawTexture(deviceContext	,x0+width-2*w	,y0+height-w-w/2	,w*2,w/2	,godrays_texture);
	renderPlatform->Print(deviceContext			,x0+width-2*w	,y0+height-w-w/2				,"godrays framebuffer");

	renderPlatform->DrawTexture(deviceContext	,x0+width-2*w	,y0+height-w-w		,w*2,w/2	,moisture_fb);
	renderPlatform->Print(deviceContext			,x0+width-2*w	,y0+height-w-w					,"moisture framebuffer");

	effect->SetTexture(deviceContext,"noiseTexture"			,NULL);
	effect->SetTexture(deviceContext,"cloudShadowTexture"	,NULL);
	effect->SetTexture(deviceContext,"cloudGodraysTexture"	,NULL);
	ApplyPass(pContext,effect->asD3DX11Effect()->GetTechniqueByName("show_shadow")->GetPassByIndex(0));
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

