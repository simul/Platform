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
	,m_pTechniqueCrossSection(NULL)
	,m_pd3dDevice(NULL)
	,cloudPerViewConstantBuffer(NULL)
	,layerConstantsBuffer(NULL)
	,lightning_texture(NULL)
	,illumination_texture(NULL)
	,m_pComputeShader(NULL)
	,computeConstantBuffer(NULL)
	,blendAndWriteAlpha(NULL)
	,blendAndDontWriteAlpha(NULL)
	,enable_lightning(false)
	,lightning_active(false)
	,m_pWrapSamplerState(NULL)
	,m_pClampSamplerState(NULL)
{
	texel_index[0]=texel_index[1]=texel_index[2]=texel_index[3]=0;
}

struct MixCloudsConstants
{
	float interpolation;
	float pad1,pad2,pad3;
};

void SimulCloudRendererDX1x::Recompile()
{
	CreateCloudEffect();
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(m_pComputeShader);
	SAFE_RELEASE(computeConstantBuffer);
	MAKE_CONSTANT_BUFFER(computeConstantBuffer,MixCloudsConstants)
	//m_pComputeShader=LoadComputeShader(m_pd3dDevice,"MixClouds_c.hlsl");
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
	cloudConstants.RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
	D3D11_SHADER_RESOURCE_VIEW_DESC texdesc;

	texdesc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
	texdesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE3D;
	texdesc.Texture3D.MostDetailedMip=0;
	texdesc.Texture3D.MipLevels=1;
	/*
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
	ID3DX11EffectPass *pass=effect->GetTechniqueByIndex(0)->asD3DX11EffectTechnique()->GetPassByIndex(0);
	HRESULT hr=pass->GetDesc(&PassDesc);

	// Get a count of the elements in the layout.
	int numElements = sizeof(decl) / sizeof(decl[0]);*/

	ClearIterators();
	layerBuffer.RestoreDeviceObjects(m_pd3dDevice,SIMUL_MAX_CLOUD_RAYTRACE_STEPS);

	SAFE_DELETE(shadow_fb);
	shadow_fb=renderPlatform->CreateTexture();
	SAFE_DELETE(moisture_fb);
	moisture_fb=renderPlatform->CreateTexture();
	SAFE_DELETE(rain_map);
	rain_map=renderPlatform->CreateTexture();
	
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
	BaseCloudRenderer::InvalidateDeviceObjects();
	gpuCloudGenerator.InvalidateDeviceObjects();
	Unmap();
	if(shadow_fb)
		shadow_fb->InvalidateDeviceObjects();
	godrays_texture.InvalidateDeviceObjects();
	if(moisture_fb)
		moisture_fb->InvalidateDeviceObjects();
	if(rain_map)
		rain_map->InvalidateDeviceObjects();
	//if(illumination_texture)
	//	Unmap3D(mapped_context,illumination_texture);
	SAFE_RELEASE(m_pWrapSamplerState);
	SAFE_RELEASE(m_pClampSamplerState);
	SAFE_RELEASE(m_pComputeShader);
	SAFE_RELEASE(computeConstantBuffer);
	SAFE_RELEASE(cloudPerViewConstantBuffer);
	SAFE_RELEASE(layerConstantsBuffer);
	SAFE_DELETE(effect);
	cloud_texture.InvalidateDeviceObjects();
	// Set the stored texture sizes to zero, so the textures will be re-created.
	cloud_tex_width_x=cloud_tex_length_y=cloud_tex_depth_z=0;
	SAFE_RELEASE(lightning_texture);
	SAFE_RELEASE(illumination_texture);
	SAFE_RELEASE(blendAndWriteAlpha);
	SAFE_RELEASE(blendAndDontWriteAlpha);

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
}

void SimulCloudRendererDX1x::Map(crossplatform::DeviceContext &deviceContext,int texture_index)
{
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
	

	m_pTechniqueCrossSection		=effect->GetTechniqueByName("cross_section");

	cloudConstants.LinkToEffect(effect,"CloudConstants");
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

// The cloud shadow texture:
// Centred on the viewer
// Aligned to the sun.
// Output is km in front of or behind the view pos where shadow starts
void SimulCloudRendererDX1x::RenderCloudShadowTexture(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
    SIMUL_COMBINED_PROFILE_START(pContext,"RenderCloudShadow")
	crossplatform::EffectTechnique *tech	=effect->GetTechniqueByName("cloud_shadow");
	cloudConstants.Apply(deviceContext);
	// per view:
	CloudPerViewConstants cloudPerViewConstants;
	math::Vector3 cam_pos=GetCameraPosVector(deviceContext.viewStruct.view);
	SetCloudShadowPerViewConstants(cloudPerViewConstants,cam_pos);
	UPDATE_CONSTANT_BUFFER(pContext,cloudPerViewConstantBuffer,CloudPerViewConstants,cloudPerViewConstants);
	ID3DX11EffectConstantBuffer* cbCloudPerViewConstants=effect->asD3DX11Effect()->GetConstantBufferByName("CloudPerViewConstants");
	if(cbCloudPerViewConstants)
		cbCloudPerViewConstants->SetConstantBuffer(cloudPerViewConstantBuffer);

	effect->SetTexture(deviceContext,"cloudDensity1",cloud_textures[(texture_cycle)  %3]);
	effect->SetTexture(deviceContext,"cloudDensity2",cloud_textures[(texture_cycle+1)%3]);
	
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	if(cloudProperties.GetWrap())
		simul::dx11::setSamplerState(effect->asD3DX11Effect(),"cloudSamplerState",m_pWrapSamplerState);
	else
		simul::dx11::setSamplerState(effect->asD3DX11Effect(),"cloudSamplerState",m_pClampSamplerState);

	effect->Apply(deviceContext,tech,0);
	if(shadow_fb->IsValid())
	{
		shadow_fb->activateRenderTarget(deviceContext);
			renderPlatform->DrawQuad(deviceContext);
		shadow_fb->deactivateRenderTarget();
	}
	effect->Unapply(deviceContext);
    SIMUL_COMBINED_PROFILE_END(pContext)
    SIMUL_COMBINED_PROFILE_START(pContext,"GodraysAccumulation")
	{
		tech	=effect->GetTechniqueByName("godrays_accumulation");
		effect->SetTexture(deviceContext,"cloudShadowTexture",shadow_fb);
		effect->SetUnorderedAccessView(deviceContext,"targetTexture1",&godrays_texture);
		effect->Apply(deviceContext,tech,0);
		pContext->Dispatch(godrays_texture.width,1,1);
		effect->SetTexture(deviceContext,"cloudShadowTexture",NULL);
		effect->SetUnorderedAccessView(deviceContext,"targetTexture1",NULL);
	}
    SIMUL_COMBINED_PROFILE_END(pContext)
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
    SIMUL_COMBINED_PROFILE_START(pContext,"MoistureAccumulation")
	if(K.precipitation>0)
	{
		effect->Unapply(deviceContext);
		tech	=effect->GetTechniqueByName("moisture_accumulation");
		effect->SetTexture(deviceContext,"cloudShadowTexture",shadow_fb);
		effect->Apply(deviceContext,tech,0);
		moisture_fb->activateRenderTarget(deviceContext);
			renderPlatform->DrawQuad(deviceContext);
		moisture_fb->deactivateRenderTarget();
		effect->Unapply(deviceContext);
		tech	=effect->GetTechniqueByName("rain_map");
		effect->Apply(deviceContext,tech,0);
		rain_map->activateRenderTarget(deviceContext);
			renderPlatform->DrawQuad(deviceContext);
		rain_map->deactivateRenderTarget();
	}
    SIMUL_COMBINED_PROFILE_END(pContext)
	simul::dx11::setTexture(effect->asD3DX11Effect(),"cloudShadowTexture",NULL);
	effect->UnbindTextures(deviceContext);
	effect->Unapply(deviceContext);
}

void SimulCloudRendererDX1x::PreRenderUpdate(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)deviceContext.platform_context;
	if(recompile_shaders)
	{
		Recompile();
	}
    SIMUL_COMBINED_PROFILE_START(pContext,"SimulCloudRendererDX1x::PreRenderUpdate")
	EnsureTexturesAreUpToDate(deviceContext);
	SetCloudConstants(cloudConstants);
	cloudConstants.Apply(deviceContext);
	RenderCombinedCloudTexture(deviceContext);
    SIMUL_COMBINED_PROFILE_END(pContext)
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
		
	//SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"0")
	ID3D11DeviceContext* pContext	=deviceContext.asD3D11DeviceContext();
	ID3D11ShaderResourceView *depthTexture_SRV=NULL;
	if(depth_tex)
		depthTexture_SRV=depth_tex->AsD3D11ShaderResourceView();
	
	math::Vector3 cam_pos	=GetCameraPosVector(deviceContext.viewStruct.view);
	float blendFactor[]		={0,0,0,0};
	UINT sampleMask			=0xffffffff;
	if(write_alpha)
		pContext->OMSetBlendState(blendAndWriteAlpha,blendFactor,sampleMask);
	else
		pContext->OMSetBlendState(blendAndDontWriteAlpha,blendFactor,sampleMask);
	simul::math::Vector3 view_dir;
	dx11::GetCameraPosVector(deviceContext.viewStruct.view,view_dir,false);
	HRESULT hr=S_OK;
	effect->SetTexture(deviceContext,"cloudDensity"			,cloud_texture);
	effect->SetTexture(deviceContext,"cloudDensity1"		,cloud_textures[(texture_cycle)  %3]);
	effect->SetTexture(deviceContext,"cloudDensity2"		,cloud_textures[(texture_cycle+1)%3]);
	effect->SetTexture(deviceContext,"noiseTexture"			,noise_texture);
	effect->SetTexture(deviceContext,"noiseTexture3D"		,noise_texture_3D);
	effect->SetTexture(deviceContext,"lossTexture"			,skyLossTexture);
	effect->SetTexture(deviceContext,"inscTexture"			,overcInscTexture);
	effect->SetTexture(deviceContext,"skylTexture"			,skylightTexture);
	effect->SetTexture(deviceContext,"depthTexture"			,depth_tex);
	effect->SetTexture(deviceContext,"lightTableTexture"	,lightTableTexture);
	effect->SetTexture(deviceContext,"rainMapTexture"		,rain_map);
	effect->SetTexture(deviceContext,"noiseTexture"			,noise_texture);
	effect->SetTexture(deviceContext,"illuminationTexture"	,illuminationTexture);

	effect->SetTexture(deviceContext,"rainbowLookupTexture",rainbowLookupTexture);
	effect->SetTexture(deviceContext,"coronaLookupTexture",coronaLookupTexture);
	
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	if(cloudProperties.GetWrap())
		simul::dx11::setSamplerState(effect->asD3DX11Effect(),"cloudSamplerState",m_pWrapSamplerState);
	else
		simul::dx11::setSamplerState(effect->asD3DX11Effect(),"cloudSamplerState",m_pClampSamplerState);
	
	//SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	//SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"2")
	CloudPerViewConstants cloudPerViewConstants;
	SetCloudPerViewConstants(cloudPerViewConstants,deviceContext.viewStruct,exposure,viewportTextureRegionXYWH,mixedResTransformXYWH);
	UPDATE_CONSTANT_BUFFER(pContext,cloudPerViewConstantBuffer,CloudPerViewConstants,cloudPerViewConstants);
	ID3DX11EffectConstantBuffer* cbCloudPerViewConstants=effect->asD3DX11Effect()->GetConstantBufferByName("CloudPerViewConstants");
	if(cbCloudPerViewConstants)
		cbCloudPerViewConstants->SetConstantBuffer(cloudPerViewConstantBuffer);
	ERRNO_CHECK
	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(deviceContext.viewStruct.view_id);
	ERRNO_CHECK
	// Moved from Update function above. See commment.
	//if (!cubemap)
	//SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	//SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"3")
	{
		//set up matrices
		simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
		simul::math::Vector3 wind_offset=cloudKeyframer->GetWindOffset();
		if(y_vertical)
			std::swap(wind_offset.y,wind_offset.z);
		X+=wind_offset;
		if(!y_vertical)
			view_dir.Define(-deviceContext.viewStruct.view._13,-deviceContext.viewStruct.view._23,-deviceContext.viewStruct.view._33);
		simul::math::Vector3 up(deviceContext.viewStruct.view._12,deviceContext.viewStruct.view._22,deviceContext.viewStruct.view._32);
		helper->SetChurn(cloudProperties.GetChurn());
		helper->Update((const float*)cam_pos,wind_offset,view_dir,up,1.0,cubemap);
	//SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	//SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"4")
		float tan_half_fov_vertical=1.f/deviceContext.viewStruct.proj._22;
		float tan_half_fov_horizontal=1.f/deviceContext.viewStruct.proj._11;
		helper->SetNoFrustumLimit(true);
		helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
		helper->MakeGeometry(cloudKeyframer,GetCloudGridInterface(),enable_lightning);
	}
	//SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	//SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"5")
	ERRNO_CHECK
	static int select_slice=-1;
	SetLayerConstants(helper,layerConstants);
	int numInstances=(int)helper->GetSlices().size();
	if(select_slice>=0)
		numInstances=1;

	UPDATE_CONSTANT_BUFFER(pContext,layerConstantsBuffer,LayerConstants,layerConstants)
	ID3DX11EffectConstantBuffer* cbLayerConstants=effect->asD3DX11Effect()->GetConstantBufferByName("LayerConstants");
	if(cbLayerConstants)
		cbLayerConstants->SetConstantBuffer(layerConstantsBuffer);
	crossplatform::EffectTechniqueGroup *group	=effect->GetTechniqueGroupByName("raytrace");
	crossplatform::EffectTechnique *tech		=NULL;
	if(group)
	{
		if(cubemap)
			tech=group->GetTechniqueByName("simple");
		else if(cloudKeyframer->GetUse3DNoise())
		{
			if(cloudConstants.rainEffect>0.0f)
			tech=group->GetTechniqueByName("noise3d");
			else
				tech=group->GetTechniqueByName("noise3d_no_rain");
		}
		else if(!cloudProperties.GetWrap())
			tech=group->GetTechniqueByName("nonwrapping");
		else if(cloudConstants.rainEffect>0.0f)
			tech=group->GetTechniqueByName("full");
		else
			tech=group->GetTechniqueByName("no_rain");
	}
	effect->Apply(deviceContext,tech,(nearFarPass==crossplatform::NEAR_PASS)?"near":(nearFarPass==crossplatform::FAR_PASS)?"far":"both");
	UtilityRenderer::DrawQuad(deviceContext);

	pContext->OMSetBlendState(NULL, blendFactor, sampleMask);
	effect->UnbindTextures(deviceContext);
	effect->SetTexture(deviceContext,"noiseTexture",NULL);
	effect->SetTexture(deviceContext,"noiseTexture3D",NULL);
	simul::dx11::setTexture(effect->asD3DX11Effect(),"illuminationTexture",(ID3D11ShaderResourceView*)NULL);
	effect->SetTexture(deviceContext,"rainMapTexture"		,NULL);
	effect->SetTexture(deviceContext,"rainbowLookupTexture",NULL);
	effect->SetTexture(deviceContext,"coronaLookupTexture",NULL);
// To prevent DX11 warning, we re-apply the pass with the textures unbound:
	effect->Unapply(deviceContext);
	ERRNO_CHECK
	//SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	return (hr==S_OK);
}

#include "Simul/Base/StringFunctions.h"

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
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
#if 1
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
	#endif
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
	deviceContext.renderPlatform->Print(deviceContext		,x0+width-w		,y0+height-w					,"2D Noise");
	effect->SetTexture(deviceContext					,"cloudShadowTexture",shadow_fb);
	simul::dx11::setTexture(effect->asD3DX11Effect()	,"cloudGodraysTexture",(ID3D11ShaderResourceView*)godrays_texture.shaderResourceView);
	UtilityRenderer::DrawQuad2(deviceContext				,x0+width-w-w	,y0+height-w		,w,w		,effect->asD3DX11Effect(),effect->asD3DX11Effect()->GetTechniqueByName("show_shadow"));
	deviceContext.renderPlatform->Print(deviceContext		,x0+width-w-w	,y0+height-w					,"shadow texture");

	deviceContext.renderPlatform->DrawTexture(deviceContext	,x0+width-2*w	,y0+height-w-w/2	,w*2,w/2	,&godrays_texture);
	deviceContext.renderPlatform->Print(deviceContext		,x0+width-2*w	,y0+height-w-w/2				,"godrays framebuffer");

	deviceContext.renderPlatform->DrawTexture(deviceContext	,x0+width-2*w	,y0+height-w-w		,w*2,w/2	,moisture_fb);
	deviceContext.renderPlatform->Print(deviceContext		,x0+width-2*w	,y0+height-w-w					,"moisture framebuffer");
	
	simul::dx11::setTexture(effect->asD3DX11Effect(),"noiseTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect->asD3DX11Effect(),"cloudShadowTexture"		,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect->asD3DX11Effect(),"cloudGodraysTexture"	,(ID3D11ShaderResourceView*)NULL);
	ApplyPass(pContext,effect->asD3DX11Effect()->GetTechniqueByName("show_shadow")->GetPassByIndex(0));
}
#pragma optimize("",off)
void SimulCloudRendererDX1x::RenderTestXXX(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.platform_context;
	HRESULT hr=S_OK;
	//effect->SetTexture(deviceContext,"noiseTexture",noise_texture);
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

void SimulCloudRendererDX1x::SetEnableStorms(bool s)
{
	enable_lightning=s;
}

CloudShadowStruct SimulCloudRendererDX1x::GetCloudShadowTexture(math::Vector3 cam_pos)
{
	CloudShadowStruct s	=BaseCloudRenderer::GetCloudShadowTexture(cam_pos);
	s.texture			=shadow_fb;
	s.godraysTexture	=&godrays_texture;
	s.moistureTexture	=moisture_fb;
	s.rainMapTexture	=rain_map;
	return s;
}

crossplatform::Texture *SimulCloudRendererDX1x::GetRandomTexture3D()
{
	return noise_texture_3D;
}

void SimulCloudRendererDX1x::EnsureCorrectTextureSizes()
{
	simul::sky::int3 i=cloudKeyframer->GetTextureSizes();
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
	godrays_texture.ensureTexture2DSizeAndFormat(renderPlatform,shadow_tex_size*2,cloudKeyframer->GetGodraysSteps(),crossplatform::R_16_FLOAT,true,false);
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
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
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


