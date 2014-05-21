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
#include "Simul/Math/Pi.h"
#include "Simul/Camera/Camera.h"						// for simul::camera::Frustum

#include "Simul/Math/Noise3D.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/DX11Exception.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"

extern simul::dx11::RenderPlatform renderPlatformDx11;

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
	,m_pTechniqueCrossSection(NULL)
	,m_pd3dDevice(NULL)
	,effect(NULL)
	,cloudPerViewConstantBuffer(NULL)
	,layerConstantsBuffer(NULL)
	,noise_texture(NULL)
	,lightning_texture(NULL)
	,illumination_texture(NULL)
	,m_pComputeShader(NULL)
	,computeConstantBuffer(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
	,overcInscTexture_SRV(NULL)
	,skylightTexture_SRV(NULL)
	,illuminationTexture_SRV(NULL)
	,lightTableTexture_SRV(NULL)
	,noiseTextureResource(NULL)
	,lightningIlluminationTextureResource(NULL)
	,blendAndWriteAlpha(NULL)
	,blendAndDontWriteAlpha(NULL)
	,enable_lightning(false)
	,lightning_active(false)
	,m_pWrapSamplerState(NULL)
	,m_pClampSamplerState(NULL)
{
	view.Identity();
	proj.Identity();
	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
	texel_index[0]=texel_index[1]=texel_index[2]=texel_index[3]=0;
}

void SimulCloudRendererDX1x::SetLossTexture(void *t)
{
	skyLossTexture_SRV=(ID3D11ShaderResourceView*)t;
}

void SimulCloudRendererDX1x::SetInscatterTextures(void *i,void *s,void *o)
{
	skyInscatterTexture_SRV=(ID3D11ShaderResourceView*)i;
	overcInscTexture_SRV=(ID3D11ShaderResourceView*)o;
	skylightTexture_SRV=(ID3D11ShaderResourceView*)s;
}

void SimulCloudRendererDX1x::SetIlluminationTexture(void *i)
{
	illuminationTexture_SRV=(ID3D11ShaderResourceView*)i;
}

void SimulCloudRendererDX1x::SetLightTableTexture(void *l)
{
	lightTableTexture_SRV=(ID3D11ShaderResourceView*)l;
}

struct MixCloudsConstants
{
	float interpolation;
	float pad1,pad2,pad3;
};

void SimulCloudRendererDX1x::RecompileShaders()
{
	CreateCloudEffect();
	if(!m_pd3dDevice)
		return;
	
	SAFE_RELEASE(m_pComputeShader);
	SAFE_RELEASE(computeConstantBuffer);
	MAKE_CONSTANT_BUFFER(computeConstantBuffer,MixCloudsConstants)
	m_pComputeShader=LoadComputeShader(m_pd3dDevice,"MixClouds_c.hlsl");
	MAKE_CONSTANT_BUFFER(cloudPerViewConstantBuffer,CloudPerViewConstants);
	MAKE_CONSTANT_BUFFER(layerConstantsBuffer,LayerConstants);
	
	SAFE_RELEASE(blendAndWriteAlpha);
	SAFE_RELEASE(blendAndDontWriteAlpha);
	// two possible blend states for clouds - with alpha written, and without.
	D3D11_BLEND_DESC omDesc;
	ZeroMemory( &omDesc, sizeof( D3D11_BLEND_DESC ) );
	omDesc.RenderTarget[0].BlendEnable		= true;
	omDesc.RenderTarget[0].BlendOp			= D3D11_BLEND_OP_ADD;
	omDesc.RenderTarget[0].BlendOpAlpha		= D3D11_BLEND_OP_ADD;
	omDesc.RenderTarget[0].SrcBlend			= D3D11_BLEND_ONE;
	omDesc.RenderTarget[0].DestBlend		= D3D11_BLEND_SRC_ALPHA;
	omDesc.RenderTarget[0].SrcBlendAlpha	= D3D11_BLEND_ZERO;
	omDesc.RenderTarget[0].DestBlendAlpha	= D3D11_BLEND_SRC_ALPHA;
	omDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	omDesc.IndependentBlendEnable			=true;
	omDesc.AlphaToCoverageEnable			=false;
	m_pd3dDevice->CreateBlendState( &omDesc, &blendAndWriteAlpha );
	omDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED
										| D3D11_COLOR_WRITE_ENABLE_GREEN
										| D3D11_COLOR_WRITE_ENABLE_BLUE;
	m_pd3dDevice->CreateBlendState( &omDesc, &blendAndDontWriteAlpha );
	gpuCloudGenerator.RecompileShaders();
}

void SimulCloudRendererDX1x::RestoreDeviceObjects(void* dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
	gpuCloudGenerator.RestoreDeviceObjects(m_pd3dDevice);
	// Allow the GPU cloud generator to directly create and modify the target textures.
	TextureStruct *ts[]={&cloud_textures[0],&cloud_textures[1],&cloud_textures[2]};
	gpuCloudGenerator.SetDirectTargets(ts);
	CreateLightningTexture();
	RecompileShaders();
	D3D11_SHADER_RESOURCE_VIEW_DESC texdesc;

	texdesc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
	texdesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE3D;
	texdesc.Texture3D.MostDetailedMip=0;
	texdesc.Texture3D.MipLevels=1;

	noiseTexture				->SetResource(noiseTextureResource);
	
	const D3D11_INPUT_ELEMENT_DESC decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
		
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		1,	0,	D3D11_INPUT_PER_INSTANCE_DATA, 1 },//noiseOffset	
        { "TEXCOORD",	1, DXGI_FORMAT_R32G32_FLOAT,		1,	8,	D3D11_INPUT_PER_INSTANCE_DATA, 1 },	//  elevationRange	
        { "TEXCOORD",	2, DXGI_FORMAT_R32_FLOAT,			1,	16,	D3D11_INPUT_PER_INSTANCE_DATA, 1 },	// noiseScale	
        { "TEXCOORD",	3, DXGI_FORMAT_R32_FLOAT,			1,	20,	D3D11_INPUT_PER_INSTANCE_DATA, 1 },	//  layerFade		
        { "TEXCOORD",	4, DXGI_FORMAT_R32_FLOAT,			1,	24,	D3D11_INPUT_PER_INSTANCE_DATA, 1 },	//  layerDistance	
    };
	const D3D11_INPUT_ELEMENT_DESC std_decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
	D3DX11_PASS_DESC PassDesc;
	ID3DX11EffectPass *pass=m_hTechniqueCloud->GetPassByIndex(0);
	HRESULT hr=pass->GetDesc(&PassDesc);

	// Get a count of the elements in the layout.
	int numElements = sizeof(decl) / sizeof(decl[0]);

	
	if(lightningIlluminationTextureResource)
		lightningIlluminationTexture->SetResource(lightningIlluminationTextureResource);
	ClearIterators();
	cloudConstants.RestoreDeviceObjects(m_pd3dDevice);
	layerBuffer.RestoreDeviceObjects(m_pd3dDevice,SIMUL_MAX_CLOUD_RAYTRACE_STEPS);

	shadow_fb.RestoreDeviceObjects(m_pd3dDevice);
	moisture_fb.RestoreDeviceObjects(m_pd3dDevice);

	SAFE_RELEASE(m_pWrapSamplerState);
	SAFE_RELEASE(m_pClampSamplerState);
	D3D11_SAMPLER_DESC samplerDesc;
	
    ZeroMemory( &samplerDesc, sizeof( D3D11_SAMPLER_DESC ) );
    samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC   ;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxAnisotropy = 16;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pWrapSamplerState);
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pClampSamplerState);
}

void SimulCloudRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	gpuCloudGenerator.InvalidateDeviceObjects();
	Unmap();
	shadow_fb.InvalidateDeviceObjects();
	godrays_texture.release();
	moisture_fb.InvalidateDeviceObjects();
	//if(illumination_texture)
	//	Unmap3D(mapped_context,illumination_texture);
	SAFE_RELEASE(m_pWrapSamplerState);
	SAFE_RELEASE(m_pClampSamplerState);
	SAFE_RELEASE(m_pComputeShader);
	SAFE_RELEASE(computeConstantBuffer);
	SAFE_RELEASE(cloudPerViewConstantBuffer);
	SAFE_RELEASE(layerConstantsBuffer);
	SAFE_RELEASE(effect);
	for(int i=0;i<3;i++)
	{
		cloud_textures[i].release();
	}
	cloud_texture.release();
	// Set the stored texture sizes to zero, so the textures will be re-created.
	cloud_tex_width_x=cloud_tex_length_y=cloud_tex_depth_z=0;
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(lightning_texture);
	SAFE_RELEASE(illumination_texture);
	skyLossTexture_SRV		=NULL;
	skyInscatterTexture_SRV	=NULL;
	overcInscTexture_SRV	=NULL;
	skylightTexture_SRV		=NULL;
	illuminationTexture_SRV	=NULL;
	SAFE_RELEASE(blendAndWriteAlpha);
	SAFE_RELEASE(blendAndDontWriteAlpha);

	SAFE_RELEASE(noiseTextureResource);
	noise_texture_3D.release();
	
	SAFE_RELEASE(lightningIlluminationTextureResource);
	cloudConstants.InvalidateDeviceObjects();
	layerBuffer.release();
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

void SimulCloudRendererDX1x::RenderNoise(void *context)
{
	ID3D11DeviceContext* pContext			=(ID3D11DeviceContext*)context;

	int noise_texture_size		=cloudKeyframer->GetEdgeNoiseTextureSize();
	int noise_texture_frequency	=cloudKeyframer->GetEdgeNoiseFrequency();
	int texture_octaves			=cloudKeyframer->GetEdgeNoiseOctaves();
	float texture_persistence	=cloudKeyframer->GetEdgeNoisePersistence();

	ID3DX11Effect*					effect=NULL;
	ID3DX11EffectTechnique *randomTechnique	=NULL;
	ID3DX11EffectTechnique *noiseTechnique	=NULL;
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_rendernoise.fx");
	randomTechnique					=effect->GetTechniqueByName("simul_random");
	noiseTechnique					=effect->GetTechniqueByName("simul_noise_2d");

	simul::dx11::Framebuffer	random_fb;
	random_fb.RestoreDeviceObjects(m_pd3dDevice);
	random_fb.SetWidthAndHeight(noise_texture_frequency,noise_texture_frequency);
	random_fb.SetFormat((int)DXGI_FORMAT_R32G32B32A32_FLOAT);
	ApplyPass(pContext,randomTechnique->GetPassByIndex(0));
	random_fb.Activate(context);
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	random_fb.Deactivate(context);

	simul::dx11::Framebuffer n_fb;
	n_fb.RestoreDeviceObjects(m_pd3dDevice);
	n_fb.SetWidthAndHeight(noise_texture_size,noise_texture_size);
	n_fb.SetFormat((int)DXGI_FORMAT_R8G8B8A8_SNORM);
	n_fb.Activate(context);
	{
		simul::dx11::setTexture(effect,"noise_texture"	,(ID3D11ShaderResourceView*)random_fb.GetColorTex());
		ID3D1xEffectShaderResourceVariable*	noiseTexture	=effect->GetVariableByName("noise_texture")->AsShaderResource();
		noiseTexture->SetResource((ID3D11ShaderResourceView*)random_fb.GetColorTex());
		simul::dx11::setParameter(effect,"octaves"		,texture_octaves);
		simul::dx11::setParameter(effect,"persistence"	,texture_persistence);
		ApplyPass(pContext,noiseTechnique->GetPassByIndex(0));
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	}
	n_fb.Deactivate(context);
	// Now copy to a texture.
	int mips=1;//PowerOfTwo(noise_texture_size/2);
	D3D11_TEXTURE2D_DESC textureDesc=
	{
		noise_texture_size,
		noise_texture_size,
		mips,
		1,
		DXGI_FORMAT_R8G8B8A8_SNORM,
		{1,0},
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0//D3D11_RESOURCE_MISC_GENERATE_MIPS only works with an RT that is also a Shader resource.
	};
	SAFE_RELEASE(noise_texture);
	hr=m_pd3dDevice->CreateTexture2D(&textureDesc,NULL,&noise_texture);
	V_CHECK(hr);
	D3D1x_MAPPED_TEXTURE2D mapped;
	if(!noise_texture)
		return;
	if(FAILED(hr=pContext->Map(noise_texture,0,D3D1x_MAP_WRITE_DISCARD,0,&mapped)))
		return;
	n_fb.CopyToMemory(pContext,mapped.pData);
	pContext->Unmap(noise_texture,0);
	V_CHECK(m_pd3dDevice->CreateShaderResourceView(noise_texture,NULL,&noiseTextureResource));
	SAFE_RELEASE(effect);
}

void SimulCloudRendererDX1x::CreateNoiseTexture(void* context)
{
	if(!m_pd3dDevice)
		return;
	RenderNoise(context);
	Create3DNoiseTexture(context);
}

void SimulCloudRendererDX1x::Create3DNoiseTexture(void *context)
{
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	//using noise_size and noise_src_ptr, make a 3d texture:

	int noise_texture_frequency				=cloudKeyframer->GetEdgeNoiseFrequency();
	int noise_texture_size							=cloudKeyframer->GetEdgeNoiseTextureSize();
	ID3DX11Effect *effect					=NULL;
	HRESULT hr								=CreateEffect(m_pd3dDevice,&effect,"simul_rendernoise.fx");
	ID3DX11EffectTechnique *randomComputeTechnique	=effect->GetTechniqueByName("random_3d_compute");
	ID3DX11EffectTechnique *noise3DComputeTechnique	=effect->GetTechniqueByName("noise_3d_compute");

	TextureStruct random_3d;
	random_3d.ensureTexture3DSizeAndFormat(m_pd3dDevice
		,noise_texture_frequency,noise_texture_frequency,noise_texture_frequency
		,DXGI_FORMAT_R32G32B32A32_FLOAT,true);
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",random_3d.unorderedAccessView);
	ApplyPass(pContext,randomComputeTechnique->GetPassByIndex(0));
	int d=(noise_texture_frequency+7)/8;
	pContext->Dispatch(d,d,d);

	while(noise_texture_size>32)
	{
		noise_texture_size/=2;
		realtime_3d_octaves++;
	}
	noise_texture_3D.ensureTexture3DSizeAndFormat(m_pd3dDevice
		,noise_texture_size,noise_texture_size,noise_texture_size
		,DXGI_FORMAT_R8G8B8A8_SNORM,true);

	simul::dx11::setTexture(effect,"random_texture_3d",random_3d.shaderResourceView);
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",noise_texture_3D.unorderedAccessView);
	int texture_octaves			=cloudKeyframer->GetEdgeNoiseOctaves();
	float texture_persistence	=cloudKeyframer->GetEdgeNoisePersistence();
	simul::dx11::setParameter(effect,"octaves"		,texture_octaves);
	simul::dx11::setParameter(effect,"persistence"	,texture_persistence);
	//noise_texture_3D.setTexels(pContext,(unsigned*)data,0,noise_texture_frequency*noise_texture_frequency*noise_texture_frequency);
	ApplyPass(pContext,noise3DComputeTechnique->GetPassByIndex(0));
	int t=(noise_texture_size+7)/8;
	pContext->Dispatch(t,t,t);
	simul::dx11::setTexture(effect,"random_texture_3d",(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",NULL);
	ApplyPass(pContext,noise3DComputeTechnique->GetPassByIndex(0));
	SAFE_RELEASE(effect);
}

bool SimulCloudRendererDX1x::CreateLightningTexture()
{
	HRESULT hr=S_OK;
	unsigned size=64;
	SAFE_RELEASE(lightning_texture);
	D3D11_TEXTURE1D_DESC textureDesc=
	{
		size,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0
	};
	if(FAILED(hr=m_pd3dDevice->CreateTexture1D(&textureDesc,NULL,&lightning_texture)))
		return (hr==S_OK);
	D3D1x_MAPPED_TEXTURE2D resource;
	ID3D11DeviceContext* pContext;
	m_pd3dDevice->GetImmediateContext(&pContext);
	hr=pContext->Map(lightning_texture,0,D3D1x_MAP_WRITE_DISCARD,0,&resource);
	if(FAILED(hr))
		return (hr==S_OK);
	unsigned char *lightning_tex_data=(unsigned char *)(resource.pData);
	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float level=.5f*linear*linear+5.f*(linear>.97f);
		float r=level;
		float g=level;
		float b=level;
		if(r>1.f)
			r=1.f;
		if(g>1.f)
			g=1.f;
		if(b>1.f)
			b=1.f;
		lightning_tex_data[4*i+0]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+1]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+2]=(unsigned char)(255.f*g);
		lightning_tex_data[4*i+3]=(unsigned char)(255.f*r);
	}
	pContext->Unmap(lightning_texture,0);
	SAFE_RELEASE(pContext)
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
/*	D3D1x_MAPPED_TEXTURE3D mapped;
	if(FAILED(hr=Map3D(illumination_texture,&mapped)))
		return;
	memset(mapped.pData,0,4*width_x*length_y*depth_z);
	Unmap3D(illumination_texture);
	
	SAFE_RELEASE(lightningIlluminationTextureResource);
    FAILED(m_pd3dDevice->CreateShaderResourceView(illumination_texture,NULL,&lightningIlluminationTextureResource ));
	FAILED(hr=Map3D(illumination_texture,&mapped_illumination));*/
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
void SimulCloudRendererDX1x::Unmap()
{
	for(int i=0;i<3;i++)
		cloud_textures[i].unmap();
}

void SimulCloudRendererDX1x::Map(ID3D11DeviceContext *context,int texture_index)
{
	HRESULT hr=S_OK;
	Unmap();
	cloud_textures[(texture_cycle+texture_index)%3].map(context);
}

bool SimulCloudRendererDX1x::CreateCloudEffect()
{
	if(!m_pd3dDevice)
		return S_OK;
	std::map<std::string,std::string> defines;
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	defines["DETAIL_NOISE"]='1';
	if(cloudProperties.GetWrap())
		defines["WRAP_CLOUDS"]="1";
	if(y_vertical)
		defines["Y_VERTICAL"]="1";
	else
		defines["Z_VERTICAL"]="1";
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	if(UseLightTables)
		defines["USE_LIGHT_TABLES"]="1";
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_clouds.fx",defines);
	if(cloudKeyframer->GetUse3DNoise())
		m_hTechniqueCloud			=effect->GetTechniqueByName("simul_clouds_3d_noise");
	else
		m_hTechniqueCloud			=effect->GetTechniqueByName("simul_clouds");
	m_hTechniqueRaytraceForward		=effect->GetTechniqueByName("simul_raytrace_forward");
	m_hTechniqueRaytraceNearPass	=effect->GetTechniqueByName("raytrace_near_pass");
	m_hTechniqueSimpleRaytrace		=effect->GetTechniqueByName("simul_simple_raytrace");
	m_hTechniqueRaytrace3DNoise		=effect->GetTechniqueByName("simul_raytrace_3d_noise");
	m_hTechniqueCloudsAndLightning	=effect->GetTechniqueByName("simul_clouds_and_lightning");

	m_pTechniqueCrossSection		=effect->GetTechniqueByName("cross_section");

	cloudDensity					=effect->GetVariableByName("cloudDensity")->AsShaderResource();
	cloudDensity1					=effect->GetVariableByName("cloudDensity1")->AsShaderResource();
	cloudDensity2					=effect->GetVariableByName("cloudDensity2")->AsShaderResource();
	noiseTexture					=effect->GetVariableByName("noiseTexture")->AsShaderResource();
	noiseTexture3D					=effect->GetVariableByName("noiseTexture3D")->AsShaderResource();
	lightningIlluminationTexture	=effect->GetVariableByName("lightningIlluminationTexture")->AsShaderResource();
	skyLossTexture					=effect->GetVariableByName("lossTexture")->AsShaderResource();
	skyInscatterTexture				=effect->GetVariableByName("inscTexture")->AsShaderResource();
	skylightTexture					=effect->GetVariableByName("skylTexture")->AsShaderResource();
	depthTexture					=effect->GetVariableByName("depthTexture")->AsShaderResource();
	lightTableTexture				=effect->GetVariableByName("lightTableTexture")->AsShaderResource();
	cloudConstants.LinkToEffect(effect,"CloudConstants");
	return true;
}

static float saturate(float c)
{
	return std::max(std::min(1.f,c),0.f);
}

void SimulCloudRendererDX1x::RenderCombinedCloudTexture(void *context)
{
#if 0
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)context;
    ProfileBlock profileBlock(pContext,"RenderCombinedCloudTexture");

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
	gpu_combine_time+=0.01f*profileBlock.GetTime();
#endif
}

// The cloud shadow texture:
// Centred on the viewer
// Aligned to the sun.
// Output is km in front of or behind the view pos where shadow starts
void SimulCloudRendererDX1x::RenderCloudShadowTexture(void *context)
{
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)context;
    SIMUL_COMBINED_PROFILE_START(context,"RenderCloudShadow")
	ID3DX11EffectTechnique *tech	=effect->GetTechniqueByName("cloud_shadow");
	cloudConstants.Apply(pContext);
	// per view:
	CloudPerViewConstants cloudPerViewConstants;
	SetCloudShadowPerViewConstants(cloudPerViewConstants);
	UPDATE_CONSTANT_BUFFER(pContext,cloudPerViewConstantBuffer,CloudPerViewConstants,cloudPerViewConstants);
	ID3DX11EffectConstantBuffer* cbCloudPerViewConstants=effect->GetConstantBufferByName("CloudPerViewConstants");
	if(cbCloudPerViewConstants)
		cbCloudPerViewConstants->SetConstantBuffer(cloudPerViewConstantBuffer);

	cloudDensity1->SetResource(cloud_textures[(texture_cycle)  %3].shaderResourceView);
	cloudDensity2->SetResource(cloud_textures[(texture_cycle+1)%3].shaderResourceView);
	
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	if(cloudProperties.GetWrap())
		simul::dx11::setSamplerState(effect,"cloudSamplerState",m_pWrapSamplerState);
	else
		simul::dx11::setSamplerState(effect,"cloudSamplerState",m_pClampSamplerState);

	ApplyPass(pContext,tech->GetPassByIndex(0));
	shadow_fb.Activate(pContext);
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	shadow_fb.Deactivate(pContext);
    SIMUL_COMBINED_PROFILE_END(context)
    SIMUL_COMBINED_PROFILE_START(context,"GodraysAccumulation")
	{
		tech	=effect->GetTechniqueByName("godrays_accumulation");
		simul::dx11::setTexture(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)shadow_fb.GetColorTex());
		simul::dx11::setUnorderedAccessView(effect,"targetTexture1",godrays_texture.unorderedAccessView);
		ApplyPass(pContext,tech->GetPassByIndex(0));
		pContext->Dispatch(godrays_texture.width,1,1);
		simul::dx11::setTexture(effect,"cloudShadowTexture",NULL);
		simul::dx11::setUnorderedAccessView(effect,"targetTexture1",NULL);
		ApplyPass(pContext,tech->GetPassByIndex(0));
	}
    SIMUL_COMBINED_PROFILE_END(context)
    SIMUL_COMBINED_PROFILE_START(context,"MoistureAccumulation")
	tech	=effect->GetTechniqueByName("moisture_accumulation");
	simul::dx11::setTexture(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)shadow_fb.GetColorTex());
	ApplyPass(pContext,tech->GetPassByIndex(0));
	moisture_fb.Activate(pContext);
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	moisture_fb.Deactivate(pContext);
	
	simul::dx11::setTexture(effect,"cloudShadowTexture",NULL);
	cloudDensity1->SetResource(NULL);
	cloudDensity2->SetResource(NULL);
	ApplyPass(pContext,tech->GetPassByIndex(0));
    SIMUL_COMBINED_PROFILE_END(context)
}

void SimulCloudRendererDX1x::PreRenderUpdate(void *context)
{
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)context;
    SIMUL_COMBINED_PROFILE_START(context,"SimulCloudRendererDX1x::PreRenderUpdate")
	EnsureTexturesAreUpToDate(pContext);
	SetCloudConstants(cloudConstants);
	cloudConstants.Apply(pContext);
	RenderCombinedCloudTexture(pContext);
	RenderCloudShadowTexture(pContext);
    SIMUL_COMBINED_PROFILE_END(context)
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
bool SimulCloudRendererDX1x::Render(void* context,float exposure,bool cubemap,bool near_pass,const void *depth_tex
,bool default_fog,bool write_alpha,int viewport_id,const simul::sky::float4& viewportTextureRegionXYWH,const simul::sky::float4& mixedResTransformXYWH)
{
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)context;
	ID3D11ShaderResourceView *depthTexture_SRV	=(ID3D11ShaderResourceView *)depth_tex;
    ProfileBlock profileBlock(pContext,cubemap?"SimulCloudRendererDX1x::Render (cubemap side)":"SimulCloudRendererDX1x::Render");

	float blendFactor[] = {0, 0, 0, 0};
	UINT sampleMask   = 0xffffffff;
	if(write_alpha)
		pContext->OMSetBlendState(blendAndWriteAlpha,blendFactor,sampleMask);
	else
		pContext->OMSetBlendState(blendAndDontWriteAlpha,blendFactor,sampleMask);
	simul::math::Vector3 view_dir(view._13,view._23,view._33);
	dx11::GetCameraPosVector(view,view_dir,false);
	HRESULT hr=S_OK;
	cloudDensity		->SetResource(cloud_texture.shaderResourceView);
	cloudDensity1		->SetResource(cloud_textures[(texture_cycle)  %3].shaderResourceView);
	cloudDensity2		->SetResource(cloud_textures[(texture_cycle+1)%3].shaderResourceView);
	noiseTexture		->SetResource(noiseTextureResource);
	noiseTexture3D		->SetResource(noise_texture_3D.shaderResourceView);
	skyLossTexture		->SetResource(skyLossTexture_SRV);
	skyInscatterTexture	->SetResource(overcInscTexture_SRV);
	skylightTexture		->SetResource(skylightTexture_SRV);
	depthTexture		->SetResource(depthTexture_SRV);
	lightTableTexture	->SetResource(lightTableTexture_SRV);

	simul::dx11::setTexture(effect,"noiseTexture"		,noiseTextureResource);
	simul::dx11::setTexture(effect,"illuminationTexture",illuminationTexture_SRV);
	
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	if(cloudProperties.GetWrap())
		simul::dx11::setSamplerState(effect,"cloudSamplerState",m_pWrapSamplerState);
	else
		simul::dx11::setSamplerState(effect,"cloudSamplerState",m_pClampSamplerState);

	CloudPerViewConstants cloudPerViewConstants;
	SetCloudPerViewConstants(cloudPerViewConstants,view,proj,exposure,viewport_id,viewportTextureRegionXYWH,mixedResTransformXYWH);
	UPDATE_CONSTANT_BUFFER(pContext,cloudPerViewConstantBuffer,CloudPerViewConstants,cloudPerViewConstants);
	ID3DX11EffectConstantBuffer* cbCloudPerViewConstants=effect->GetConstantBufferByName("CloudPerViewConstants");
	if(cbCloudPerViewConstants)
		cbCloudPerViewConstants->SetConstantBuffer(cloudPerViewConstantBuffer);
	
	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(viewport_id);

	// Moved from Update function above. See commment.
	//if (!cubemap)
	{
		//set up matrices
		simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
		simul::math::Vector3 wind_offset=cloudKeyframer->GetWindOffset();
		if(y_vertical)
			std::swap(wind_offset.y,wind_offset.z);
		X+=wind_offset;
		if(!y_vertical)
			view_dir.Define(-view._13,-view._23,-view._33);
		simul::math::Vector3 up(view._12,view._22,view._32);
		helper->SetChurn(cloudProperties.GetChurn());
		helper->Update((const float*)cam_pos,wind_offset,view_dir,up,1.0);
		float tan_half_fov_vertical=1.f/proj._22;
		float tan_half_fov_horizontal=1.f/proj._11;
		helper->SetNoFrustumLimit(true);
		helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
		helper->MakeGeometry(cloudKeyframer,GetCloudGridInterface(),enable_lightning);
	}

	static int select_slice=-1;
	int ii=0;
	unsigned el,az;
	helper->GetGrid(el,az);
	SetLayerConstants(helper,layerConstants);
	int numInstances=(int)helper->GetSlices().size();
	if(select_slice>=0)
		numInstances=1;

	UPDATE_CONSTANT_BUFFER(pContext,layerConstantsBuffer,LayerConstants,layerConstants)
	ID3DX11EffectConstantBuffer* cbLayerConstants=effect->GetConstantBufferByName("LayerConstants");
	if(cbLayerConstants)
		cbLayerConstants->SetConstantBuffer(layerConstantsBuffer);
	if(cloudKeyframer->GetUse3DNoise())
	{
		ApplyPass(pContext,m_hTechniqueRaytrace3DNoise->GetPassByIndex(0));
	}
	else if(cubemap)
	{
		ApplyPass(pContext,m_hTechniqueSimpleRaytrace->GetPassByIndex(0));
	}
	else if(cloudKeyframer->GetTraceForward())
	{
		if(near_pass)
			ApplyPass(pContext,m_hTechniqueRaytraceNearPass->GetPassByIndex(0));
		else
			ApplyPass(pContext,m_hTechniqueRaytraceForward->GetPassByIndex(0));
	}
	UtilityRenderer::DrawQuad(pContext);
	skyLossTexture->SetResource(NULL);
	skyInscatterTexture->SetResource(NULL);
	skylightTexture->SetResource(NULL);
	depthTexture->SetResource(NULL);
	ApplyPass(pContext,m_hTechniqueRaytraceForward->GetPassByIndex(0));
	pContext->OMSetBlendState(NULL, blendFactor, sampleMask);
	depthTexture->SetResource((ID3D11ShaderResourceView*)NULL);
	cloudDensity->SetResource((ID3D11ShaderResourceView*)NULL);
	cloudDensity1->SetResource((ID3D11ShaderResourceView*)NULL);
	cloudDensity2->SetResource((ID3D11ShaderResourceView*)NULL);
	noiseTexture->SetResource((ID3D11ShaderResourceView*)NULL);
	noiseTexture3D->SetResource((ID3D11ShaderResourceView*)NULL);
	skyLossTexture->SetResource((ID3D11ShaderResourceView*)NULL);
	skyInscatterTexture->SetResource((ID3D11ShaderResourceView*)NULL);
	skylightTexture->SetResource((ID3D11ShaderResourceView*)NULL);
	depthTexture->SetResource((ID3D11ShaderResourceView*)NULL);
	lightTableTexture	->SetResource((ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"illuminationTexture",(ID3D11ShaderResourceView*)NULL);
// To prevent BIZARRE DX11 warning, we re-apply the pass with the textures unbound:
	ApplyPass(pContext,m_hTechniqueRaytraceForward->GetPassByIndex(0));
	return (hr==S_OK);
}


void SimulCloudRendererDX1x::RenderDebugInfo(void *context,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	
	D3DXMATRIX ortho;
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
    D3DXMatrixOrthoLH(&ortho,(float)width,(float)height,-100.f,100.f);
	ortho._41=-1.f;
	ortho._22=-ortho._22;
	ortho._42=1.f;
	UtilityRenderer::SetMatrices(ident,ortho);
	simul::clouds::BaseCloudRenderer::RenderDebugInfo(pContext,width,height);
}

void SimulCloudRendererDX1x::DrawLines(void *context,VertexXyzRgba *vertices,int vertex_count,bool strip)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	
	simul::dx11::UtilityRenderer::DrawLines(pContext,vertices,vertex_count,strip);
}

void SimulCloudRendererDX1x::RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();

	static int u=4;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
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
		cloudDensity1->SetResource(cloud_textures[(i+texture_cycle)%3].shaderResourceView);
		dx11::setTexture(effect,"cloudDensity",cloud_textures[(i+texture_cycle)%3].shaderResourceView);

		cloudConstants.lightResponse		=light_response;
		cloudConstants.crossSectionOffset	=vec3(0.5f,0.5f,0.f);
		cloudConstants.yz					=0.f;
		cloudConstants.Apply(pContext);
		deviceContext.renderPlatform->DrawQuad(deviceContext.platform_context,x0+i*(w+1)+4,y0+4,w,h,effect,m_pTechniqueCrossSection);
		cloudConstants.yz					=1.f;
		cloudConstants.Apply(pContext);
		deviceContext.renderPlatform->DrawQuad(deviceContext.platform_context,x0+i*(w+1)+4,y0+h+8,w,w,effect,m_pTechniqueCrossSection);
	}
	cloudDensity1->SetResource(NULL);
	cloudDensity2->SetResource(NULL);
	ApplyPass(pContext,effect->GetTechniqueByName("show_shadow")->GetPassByIndex(0));
}

void SimulCloudRendererDX1x::RenderAuxiliaryTextures(simul::crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.platform_context;
	HRESULT hr=S_OK;
	static int u=4;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
	simul::dx11::setTexture(effect,"noiseTexture",noiseTextureResource);
	UtilityRenderer::DrawQuad2(pContext	,width-w,height-(w+8),w,w,effect,effect->GetTechniqueByName("show_noise"));
	deviceContext.renderPlatform->Print(pContext		,width-w,height-(w+8)	,"2D Noise");
	simul::dx11::setTexture(effect,"cloudShadowTexture",(ID3D11ShaderResourceView*)shadow_fb.GetColorTex());
	simul::dx11::setTexture(effect,"cloudGodraysTexture",(ID3D11ShaderResourceView*)godrays_texture.shaderResourceView);
	UtilityRenderer::DrawQuad2(pContext	,width-(w+8)-(w+8),height-(w+8),w,w,effect,effect->GetTechniqueByName("show_shadow"));
	deviceContext.renderPlatform->Print(pContext		,width-(w+8)-(w+8),height-(w+8)	,"shadow texture");

	renderPlatformDx11.DrawTexture(pContext	,width-2*w,height-(w+8)-w/2	,w*2,w/2,godrays_texture.shaderResourceView);
	deviceContext.renderPlatform->Print(pContext			,width-2*w,height-(w+8)-w/2	,"godrays framebuffer");

	renderPlatformDx11.DrawTexture(pContext	,width-2*w,height-(w+8)-w	,w*2,w/2	,(ID3D11ShaderResourceView*)moisture_fb.GetColorTex());
	deviceContext.renderPlatform->Print(pContext			,width-2*w,height-(w+8)-w	,"moisture framebuffer");

	simul::dx11::setTexture(effect,"noiseTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"cloudShadowTexture"		,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"cloudGodraysTexture"	,(ID3D11ShaderResourceView*)NULL);
	ApplyPass(pContext,effect->GetTechniqueByName("show_shadow")->GetPassByIndex(0));
}

bool SimulCloudRendererDX1x::RenderLightning(void *context,int viewport_id)
{
	return S_OK;
}


bool SimulCloudRendererDX1x::MakeCubemap()
{
	HRESULT hr=S_OK;
	return (hr==S_OK);
}

void SimulCloudRendererDX1x::SetEnableStorms(bool s)
{
	enable_lightning=s;
}

CloudShadowStruct SimulCloudRendererDX1x::GetCloudShadowTexture()
{
	CloudShadowStruct s	=BaseCloudRenderer::GetCloudShadowTexture();
	s.texture			=shadow_fb.GetColorTex();
	s.godraysTexture	=godrays_texture.shaderResourceView;
	s.moistureTexture	=moisture_fb.GetColorTex();
	return s;
}

void *SimulCloudRendererDX1x::GetRandomTexture3D()
{
	return noise_texture_3D.shaderResourceView;
}

void SimulCloudRendererDX1x::EnsureCorrectTextureSizes()
{
	simul::sky::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	bool uav=gpuCloudGenerator.GetEnabled();
	static DXGI_FORMAT cloud_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;
	for(int i=0;i<3;i++)
	{
		cloud_textures[i].ensureTexture3DSizeAndFormat(m_pd3dDevice,width_x,length_y,depth_z,cloud_tex_format,uav);
	}
	shadow_fb.SetWidthAndHeight(cloudKeyframer->GetShadowTextureSize(),cloudKeyframer->GetGodraysSteps());
	godrays_texture.ensureTexture2DSizeAndFormat(m_pd3dDevice,cloudKeyframer->GetShadowTextureSize()*2,cloudKeyframer->GetGodraysSteps(),DXGI_FORMAT_R32_FLOAT,true,false);
	moisture_fb.SetWidthAndHeight(cloudKeyframer->GetShadowTextureSize()*2,cloudKeyframer->GetGodraysSteps());
	if(!width_x||!length_y||!depth_z)
		return;
	if(width_x==cloud_tex_width_x&&length_y==cloud_tex_length_y&&depth_z==cloud_tex_depth_z&&cloud_textures[texture_cycle%3].texture!=NULL)
		return;
	shadow_fb.SetGenerateMips(false);
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	
	//cloud_texture.ensureTexture3DSizeAndFormat(m_pd3dDevice,width_x,length_y,depth_z,cloud_tex_format,true);
}

void SimulCloudRendererDX1x::EnsureTexturesAreUpToDate(void *context)
{
	EnsureCorrectTextureSizes();
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	EnsureTextureCycle();
	if(FailedNoiseChecksum())
		SAFE_RELEASE(noise_texture);
	if(!noise_texture)
		CreateNoiseTexture(pContext);
	// We don't need to fill the textures if the gpu Generator has already done so:
	if(!gpuCloudGenerator.GetEnabled())
		return;
	for(int i=0;i<3;i++)
	{
		int cycled_index=(texture_cycle+i)%3;
		clouds::GpuCloudsParameters g=cloudKeyframer->GetGpuCloudsParameters(i);
		gpuCloudGenerator.Update(cycled_index,g,NULL);
	}
}

void SimulCloudRendererDX1x::EnsureCorrectIlluminationTextureSizes()
{
}

void SimulCloudRendererDX1x::EnsureIlluminationTexturesAreUpToDate()
{
}

void SimulCloudRendererDX1x::EnsureTextureCycle()
{
	Unmap();
	int cyc=(cloudKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(seq_texture_iterator[0],seq_texture_iterator[1]);
		std::swap(seq_texture_iterator[1],seq_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}


