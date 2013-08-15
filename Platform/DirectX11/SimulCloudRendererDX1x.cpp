// Copyright (c) 2007-2013 Simul Software Ltd
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
#include <dxerr.h>
#include <string>

#include "Simul/Base/Timer.h"
#include "Simul/Math/Pi.h"
#include "Simul/Camera/Camera.h"	// for simul::camera::Frustum

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/Utilities.h"

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

class ExampleHumidityCallback:public simul::clouds::HumidityCallbackInterface
{
public:
	ExampleHumidityCallback()
		:base_layer(0.28f)
		,transition(0.2f)
		,mul(0.86f)
		,default_(1.f)
	{}
	float base_layer;
	float transition;
	float mul;
	float default_;
	virtual float GetHumidityMultiplier(float ,float ,float z) const
	{
		if(z>base_layer)
		{
			if(z>base_layer+transition)
				return mul;
			float i=(z-base_layer)/transition;
			return default_*(1.f-i)+mul*i;
		}
		return default_;
	}
};
ExampleHumidityCallback hm;

SimulCloudRendererDX1x::SimulCloudRendererDX1x(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem) :
	simul::clouds::BaseCloudRenderer(ck,mem)
	,m_hTechniqueLightning(NULL)
	,m_hTechniqueCrossSectionXZ(NULL)
	,m_hTechniqueCrossSectionXY(NULL)
	,m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,m_pLightningVtxDecl(NULL)
	,m_pCloudEffect(NULL)
	,instanceBuffer(NULL)
	,m_pLightningEffect(NULL)
	,cloudPerViewConstantBuffer(NULL)
	,layerConstantsBuffer(NULL)
	,noise_texture(NULL)
	,noise_texture_3D(NULL)
	,lightning_texture(NULL)
	,illumination_texture(NULL)
	,m_pComputeShader(NULL)
	,computeConstantBuffer(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
	,overcInscTexture_SRV(NULL)
	,skylightTexture_SRV(NULL)
	,illuminationTexture_SRV(NULL)
	,noiseTextureResource(NULL)
	,noiseTexture3DResource(NULL)
	,lightningIlluminationTextureResource(NULL)
	,blendAndWriteAlpha(NULL)
	,blendAndDontWriteAlpha(NULL)
	,enable_lightning(false)
	,lightning_active(false)
	,m_pWrapSamplerState(NULL)
	,m_pClampSamplerState(NULL)
{
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
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
	illuminationTexture_SRV=(ID3D1xShaderResourceView*)i;
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
	HRESULT hr;
	SAFE_RELEASE(m_pComputeShader);
	SAFE_RELEASE(computeConstantBuffer);
	MAKE_CONSTANT_BUFFER(computeConstantBuffer,MixCloudsConstants)
	m_pComputeShader=LoadComputeShader(m_pd3dDevice,"MixClouds_c.hlsl");
	V_CHECK(CreateEffect(m_pd3dDevice,&m_pLightningEffect,"simul_lightning.fx"));
	if(m_pLightningEffect)
	{
		m_hTechniqueLightning	=m_pLightningEffect->GetTechniqueByName("simul_lightning");
		l_worldViewProj			=m_pLightningEffect->GetVariableByName("worldViewProj")->AsMatrix();
	}
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
	omDesc.RenderTarget[0].SrcBlend			= Raytrace?D3D11_BLEND_ONE:D3D11_BLEND_SRC_ALPHA;
	omDesc.RenderTarget[0].DestBlend		= D3D11_BLEND_INV_SRC_ALPHA;
	omDesc.RenderTarget[0].SrcBlendAlpha	= D3D11_BLEND_ZERO;
	omDesc.RenderTarget[0].DestBlendAlpha	= D3D11_BLEND_INV_SRC_ALPHA;
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
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);

	// Get a count of the elements in the layout.
	int numElements = sizeof(decl) / sizeof(decl[0]);

	hr=m_pd3dDevice->CreateInputLayout( decl,numElements,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	CreateMeshBuffers();
	
	if(m_hTechniqueLightning)
	{
		pass=m_hTechniqueLightning->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);
		hr=m_pd3dDevice->CreateInputLayout( std_decl, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pLightningVtxDecl);
	}
	if(lightningIlluminationTextureResource)
		lightningIlluminationTexture->SetResource(lightningIlluminationTextureResource);
	ClearIterators();
	cloudConstants.RestoreDeviceObjects(m_pd3dDevice);

	cloudShadow.SetGenerateMips(false/*true*/);
	cloudShadow.RestoreDeviceObjects(m_pd3dDevice);

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
	
void SimulCloudRendererDX1x::CreateMeshBuffers()
{
	unsigned el,az;
	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(0);
	helper->GetGrid(el,az);
	helper->GenerateSphereVertices();
	std::vector<vec3> v;
	std::vector<unsigned short> ind;
	simul::clouds::CloudGeometryHelper::GenerateSphereVertices(
				v
				,ind
				,el,az);
	sphere.init(m_pd3dDevice,v,ind);

	int num_vertices=32;
	int num_indices=33;
	// Create the main vertex buffer:
	vec3 *vertices=new vec3[num_vertices];
	unsigned short *indices=new unsigned short[num_indices];
	vec3 *dest=vertices;
	for(int i=0;i<num_vertices;i++,dest++)
	{
		float angle=2.f*pi*(float)i/(float)num_vertices;
		dest->x = sin(angle);	//helper->GetVertices()[i].x;
		dest->y = cos(angle);	//helper->GetVertices()[i].y;
		dest->z = 0.f;			//helper->GetVertices()[i].z;
	}
	for(int i=0;i<num_indices;i++)
	{
	//	unsigned short s=helper->GetQuadStripIndices()[i];
		indices[i]=i%num_vertices;
	}
	circle.init(m_pd3dDevice,num_vertices,num_indices,vertices,indices);
    delete [] vertices;
    delete [] indices;

	// Instance buffer:
	SAFE_RELEASE(instanceBuffer);
	D3D11_BUFFER_DESC instanceBufferDesc;
	D3D11_SUBRESOURCE_DATA instanceData;
		// Set up the description of the instance buffer.
	instanceBufferDesc.Usage				=D3D11_USAGE_DYNAMIC;
	instanceBufferDesc.ByteWidth			=sizeof(LayerData)*MAX_INSTANCES;
	instanceBufferDesc.BindFlags			=D3D11_BIND_VERTEX_BUFFER;
	instanceBufferDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_WRITE;
	instanceBufferDesc.MiscFlags			=0;
	instanceBufferDesc.StructureByteStride	=0;
	// Give the subresource structure a pointer to the instance data.
	instanceData.pSysMem = layerConstants.layers;
	instanceData.SysMemPitch = 0;
	instanceData.SysMemSlicePitch = 0;

	// Create the instance buffer.
	m_pd3dDevice->CreateBuffer(&instanceBufferDesc, &instanceData, &instanceBuffer);
	{
		ID3D11DeviceContext* pContext;
		m_pd3dDevice->GetImmediateContext(&pContext);
		D3D11_MAPPED_SUBRESOURCE mapped_res;
		pContext->Map(instanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		for(int i=0;i<MAX_INSTANCES;i++)
			((LayerData*)(mapped_res.pData))[i]=layerConstants.layers[i];
		pContext->Unmap(instanceBuffer, 0);
		SAFE_RELEASE(pContext)
	}
}

void SimulCloudRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	gpuCloudGenerator.InvalidateDeviceObjects();
	Unmap();
	cloudShadow.InvalidateDeviceObjects();
	//if(illumination_texture)
	//	Unmap3D(mapped_context,illumination_texture);
	SAFE_RELEASE(m_pWrapSamplerState);
	SAFE_RELEASE(m_pClampSamplerState);
	SAFE_RELEASE(m_pComputeShader);
	SAFE_RELEASE(computeConstantBuffer);
	SAFE_RELEASE(cloudPerViewConstantBuffer);
	SAFE_RELEASE(layerConstantsBuffer);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);
	SAFE_RELEASE(m_pCloudEffect);
	SAFE_RELEASE(m_pLightningEffect);
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
	circle.release();
	sphere.release();
	SAFE_RELEASE(instanceBuffer);
	skyLossTexture_SRV		=NULL;
	skyInscatterTexture_SRV	=NULL;
	overcInscTexture_SRV	=NULL;
	skylightTexture_SRV		=NULL;
	illuminationTexture_SRV	=NULL;
	SAFE_RELEASE(blendAndWriteAlpha);
	SAFE_RELEASE(blendAndDontWriteAlpha);

	SAFE_RELEASE(noiseTextureResource);
	SAFE_RELEASE(noise_texture_3D);
	SAFE_RELEASE(noiseTexture3DResource);
	
	SAFE_RELEASE(lightningIlluminationTextureResource);
	cloudConstants.InvalidateDeviceObjects();
	ClearIterators();
}

bool SimulCloudRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return (true);
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
	int noise_texture_size		=cloudKeyframer->GetEdgeNoiseTextureSize();
	int noise_texture_frequency	=cloudKeyframer->GetEdgeNoiseFrequency();
	int texture_octaves			=cloudKeyframer->GetEdgeNoiseOctaves();
	float texture_persistence	=cloudKeyframer->GetEdgeNoisePersistence();

	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	ID3D1xEffect*					effect=NULL;
	ID3D1xEffectTechnique*			randomTechnique;
	ID3D1xEffectTechnique*			noiseTechnique;
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_rendernoise.fx");
	randomTechnique					=effect->GetTechniqueByName("simul_random");
	noiseTechnique					=effect->GetTechniqueByName("simul_noise_2d");

	simul::dx11::Framebuffer	random_fb;
	random_fb.RestoreDeviceObjects(m_pd3dDevice);
	random_fb.SetWidthAndHeight(noise_texture_frequency,noise_texture_frequency);
	random_fb.SetFormat((int)DXGI_FORMAT_R32G32B32A32_FLOAT);
	ApplyPass(pContext,randomTechnique->GetPassByIndex(0));
	random_fb.Activate(context);
		random_fb.DrawQuad(context);
	random_fb.Deactivate(context);

	simul::dx11::Framebuffer n_fb;
	n_fb.RestoreDeviceObjects(m_pd3dDevice);
	n_fb.SetWidthAndHeight(noise_texture_size,noise_texture_size);
	n_fb.SetFormat((int)DXGI_FORMAT_R8G8B8A8_SNORM);
	n_fb.Activate(context);
	{
		simul::dx11::setParameter(effect,"noise_texture"	,(ID3D11ShaderResourceView*)random_fb.GetColorTex());
		ID3D1xEffectShaderResourceVariable*	noiseTexture	=effect->GetVariableByName("noise_texture")->AsShaderResource();
		noiseTexture->SetResource((ID3D11ShaderResourceView*)random_fb.GetColorTex());
		simul::dx11::setParameter(effect,"octaves"		,texture_octaves);
		simul::dx11::setParameter(effect,"persistence"	,texture_persistence);
		ApplyPass(pContext,noiseTechnique->GetPassByIndex(0));
		n_fb.DrawQuad(context);
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

bool SimulCloudRendererDX1x::CreateNoiseTexture(void* context)
{
	if(!m_pd3dDevice)
		return false;
	RenderNoise(context);
	Create3DNoiseTexture(context);
	return true;
}
#include "Simul/Math/Noise3D.h"
void SimulCloudRendererDX1x::Create3DNoiseTexture(void *context)
{
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext*)context;
	//using noise_size and noise_src_ptr, make a 3d texture:
	SAFE_RELEASE(noise_texture_3D);
	SAFE_RELEASE(noiseTexture3DResource);

	int noise_texture_frequency	=cloudKeyframer->GetEdgeNoiseFrequency();
	ID3D1xEffect*					effect=NULL;
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_rendernoise.fx");
	ID3D1xEffectTechnique*			randomTechnique=effect->GetTechniqueByName("simul_random");

	simul::dx11::Framebuffer	random_fb;
	random_fb.RestoreDeviceObjects(m_pd3dDevice);
	random_fb.SetWidthAndHeight(noise_texture_frequency,noise_texture_frequency*noise_texture_frequency);
	random_fb.SetFormat((int)DXGI_FORMAT_R8G8B8A8_SNORM);
	ApplyPass(pContext,randomTechnique->GetPassByIndex(0));
	random_fb.Activate(context);
		random_fb.DrawQuad(context);
	random_fb.Deactivate(context);

	char *data=new char[4*noise_texture_frequency*noise_texture_frequency*noise_texture_frequency];
	random_fb.CopyToMemory(pContext,data);
	noise_texture_3D=make3DTexture(m_pd3dDevice,noise_texture_frequency,noise_texture_frequency,noise_texture_frequency,DXGI_FORMAT_R8G8B8A8_SNORM,(const float*)data);

	m_pd3dDevice->CreateShaderResourceView(noise_texture_3D,NULL,&noiseTexture3DResource);
// Told to comment out by Rod as was causing an issue - needs sorting out.
//	pContext->GenerateMips(noiseTexture3DResource);
	delete [] data;
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
	defines["DETAIL_NOISE"]='1';
	if(GetCloudInterface()->GetWrap())
		defines["WRAP_CLOUDS"]="1";
	if(y_vertical)
		defines["Y_VERTICAL"]="1";
	else
		defines["Z_VERTICAL"]="1";
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	HRESULT hr=CreateEffect(m_pd3dDevice,&m_pCloudEffect,"simul_clouds.fx",defines);
	if(cloudKeyframer->GetUse3DNoise())
		m_hTechniqueCloud			=m_pCloudEffect->GetTechniqueByName("simul_clouds_3d_noise");
	else
		m_hTechniqueCloud			=m_pCloudEffect->GetTechniqueByName("simul_clouds");
	m_hTechniqueRaytrace			=m_pCloudEffect->GetTechniqueByName("simul_raytrace");
	m_hTechniqueSimpleRaytrace		=m_pCloudEffect->GetTechniqueByName("simul_simple_raytrace");
	m_hTechniqueRaytrace3DNoise		=m_pCloudEffect->GetTechniqueByName("simul_raytrace_3d_noise");
	m_hTechniqueCloudsAndLightning	=m_pCloudEffect->GetTechniqueByName("simul_clouds_and_lightning");

	m_hTechniqueCrossSectionXY		=m_pCloudEffect->GetTechniqueByName("cross_section_xy");
	m_hTechniqueCrossSectionXZ		=m_pCloudEffect->GetTechniqueByName("cross_section_xz");

	cloudDensity					=m_pCloudEffect->GetVariableByName("cloudDensity")->AsShaderResource();
	cloudDensity1					=m_pCloudEffect->GetVariableByName("cloudDensity1")->AsShaderResource();
	cloudDensity2					=m_pCloudEffect->GetVariableByName("cloudDensity2")->AsShaderResource();
	noiseTexture					=m_pCloudEffect->GetVariableByName("noiseTexture")->AsShaderResource();
	noiseTexture3D					=m_pCloudEffect->GetVariableByName("noiseTexture3D")->AsShaderResource();
	lightningIlluminationTexture	=m_pCloudEffect->GetVariableByName("lightningIlluminationTexture")->AsShaderResource();
	skyLossTexture					=m_pCloudEffect->GetVariableByName("lossTexture")->AsShaderResource();
	skyInscatterTexture				=m_pCloudEffect->GetVariableByName("inscTexture")->AsShaderResource();
	skylightTexture					=m_pCloudEffect->GetVariableByName("skylTexture")->AsShaderResource();
	depthTexture					=m_pCloudEffect->GetVariableByName("depthTexture")->AsShaderResource();
	cloudConstants.LinkToEffect(m_pCloudEffect,"CloudConstants");
	return true;
}
/*
static D3DXVECTOR4 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR4 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}*/

static float saturate(float c)
{
	return std::max(std::min(1.f,c),0.f);
}

void SimulCloudRendererDX1x::RenderCombinedCloudTexture(void *context)
{
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

	//pContext->Dispatch(cloud_tex_width_x/16,cloud_tex_length_y/16,cloud_tex_depth_z/1);

    pContext->CSSetShader( NULL, NULL, 0 );
	ID3D11UnorderedAccessView *u[]={NULL,NULL};
    pContext->CSSetUnorderedAccessViews( 0, 1, u, NULL );
	ID3D11ShaderResourceView *n[]={NULL,NULL};
	pContext->CSSetShaderResources( 0, 2, n);
	gpu_combine_time*=0.99f;
	gpu_combine_time+=0.01f*profileBlock.GetTime();
}

// The cloud shadow texture:
// Centred on the viewer
// Aligned to the sun.
// Output is km in front of or behind the view pos where shadow starts
void SimulCloudRendererDX1x::RenderCloudShadowTexture(void *context)
{
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)context;
    ProfileBlock profileBlock(pContext,"SimulCloudRendererDX1x::RenderCloudShadow");
	ID3DX11EffectTechnique *tech	=m_pCloudEffect->GetTechniqueByName("cloud_shadow");
	cloudConstants.Apply(pContext);
	// per view:
	CloudPerViewConstants cloudPerViewConstants;
	SetCloudShadowPerViewConstants(cloudPerViewConstants);
	//cloudPerViewConstants.shadowMatrix.transpose();
	UPDATE_CONSTANT_BUFFER(pContext,cloudPerViewConstantBuffer,CloudPerViewConstants,cloudPerViewConstants);
	ID3DX11EffectConstantBuffer* cbCloudPerViewConstants=m_pCloudEffect->GetConstantBufferByName("CloudPerViewConstants");
	if(cbCloudPerViewConstants)
		cbCloudPerViewConstants->SetConstantBuffer(cloudPerViewConstantBuffer);

	cloudDensity1->SetResource(cloud_textures[(texture_cycle)  %3].shaderResourceView);
	cloudDensity2->SetResource(cloud_textures[(texture_cycle+1)%3].shaderResourceView);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	cloudShadow.Activate(pContext);
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	cloudShadow.Deactivate(pContext);
//	pContext->GenerateMips((ID3D11ShaderResourceView*)cloudShadow.GetColorTex());
}

void SimulCloudRendererDX1x::Update(void *context)
{
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)context;
	EnsureTexturesAreUpToDate(pContext);
	SetCloudConstants(cloudConstants);
	cloudConstants.Apply(pContext);
	RenderCombinedCloudTexture(pContext);
	RenderCloudShadowTexture(pContext);
	//set up matrices
// Commented this out and moved to Render as was causing cloud noise problem due to the camera
// matrix it was using being for light probes rather than the main view.
// A global update shouldn't use per view data.
// This needs re-factoring once the view handle work we discussed has been implemented.
// We expect to be able to create views with flags e.g for whether they will render with noise and therefore need to 
// do a per frame view specific update.
// We'll then have a global update and per view updates.
/*	simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
	simul::math::Vector3 wind_offset=GetCloudInterface()->GetWindOffset();
	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);
	X+=wind_offset;
	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	if(!y_vertical)
		view_dir.Define(-view._13,-view._23,-view._33);
	simul::math::Vector3 up(view._12,view._22,view._32);
	helper->Update((const float*)cam_pos,wind_offset,view_dir,up);
	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetNoFrustumLimit(true);
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	helper->MakeGeometry(GetCloudInterface(),GetCloudGridInterface(),enable_lightning);*/
}
static int test=29999;
bool SimulCloudRendererDX1x::Render(void* context,float exposure,bool cubemap,const void *depth_tex,bool default_fog,bool write_alpha,int viewport_id,const simul::sky::float4& viewportTextureRegionXYWH)
{
	ID3D11DeviceContext* pContext	=(ID3D11DeviceContext*)context;
	ID3D1xShaderResourceView *depthTexture_SRV	=(ID3D1xShaderResourceView *)depth_tex;
    ProfileBlock profileBlock(pContext,"SimulCloudRendererDX1x::Render");

	float blendFactor[] = {0, 0, 0, 0};
	UINT sampleMask   = 0xffffffff;
	if(write_alpha)
		pContext->OMSetBlendState(blendAndWriteAlpha, blendFactor, sampleMask);
	else
		pContext->OMSetBlendState(blendAndDontWriteAlpha, blendFactor, sampleMask);

	HRESULT hr=S_OK;
	PIXBeginNamedEvent(1,"Render Clouds Layers");
	cloudDensity->SetResource(cloud_texture.shaderResourceView);
	cloudDensity1->SetResource(cloud_textures[(texture_cycle)  %3].shaderResourceView);
	cloudDensity2->SetResource(cloud_textures[(texture_cycle+1)%3].shaderResourceView);
	noiseTexture->SetResource(noiseTextureResource);
	noiseTexture3D->SetResource(noiseTexture3DResource);
	skyLossTexture->SetResource(skyLossTexture_SRV);
	skyInscatterTexture->SetResource(overcInscTexture_SRV);
	skylightTexture->SetResource(skylightTexture_SRV);
	depthTexture->SetResource(depthTexture_SRV);
	//simul::dx11::setParameter(m_pCloudEffect,"depthTexture",depthTexture_SRV);
	simul::dx11::setParameter(m_pCloudEffect,"illuminationTexture",illuminationTexture_SRV);

	if(GetCloudInterface()->GetWrap())
		simul::dx11::setSamplerState(m_pCloudEffect,"cloudSamplerState",m_pWrapSamplerState);
	else
		simul::dx11::setSamplerState(m_pCloudEffect,"cloudSamplerState",m_pClampSamplerState);

	CloudPerViewConstants cloudPerViewConstants;
	SetCloudPerViewConstants(cloudPerViewConstants,view,proj,exposure,viewport_id,viewportTextureRegionXYWH);
	UPDATE_CONSTANT_BUFFER(pContext,cloudPerViewConstantBuffer,CloudPerViewConstants,cloudPerViewConstants);
	ID3DX11EffectConstantBuffer* cbCloudPerViewConstants=m_pCloudEffect->GetConstantBufferByName("CloudPerViewConstants");
	if(cbCloudPerViewConstants)
		cbCloudPerViewConstants->SetConstantBuffer(cloudPerViewConstantBuffer);

	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(viewport_id);
	// Moved from Update function above. See commment.
	if (!cubemap)
	{
		//set up matrices
		simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
		simul::math::Vector3 wind_offset=GetCloudInterface()->GetWindOffset();
		if(y_vertical)
			std::swap(wind_offset.y,wind_offset.z);
		X+=wind_offset;
		simul::math::Vector3 view_dir	(view._13,view._23,view._33);
		if(!y_vertical)
			view_dir.Define(-view._13,-view._23,-view._33);
		simul::math::Vector3 up(view._12,view._22,view._32);
		helper->SetChurn(GetCloudInterface()->GetChurn());
		helper->Update((const float*)cam_pos,wind_offset,view_dir,up);
		float tan_half_fov_vertical=1.f/proj._22;
		float tan_half_fov_horizontal=1.f/proj._11;
		helper->SetNoFrustumLimit(true);
		helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
		helper->MakeGeometry(GetCloudInterface(),GetCloudGridInterface(),enable_lightning);
	}

	static int select_slice=-1;
	int ii=0;
	unsigned el,az;
	helper->GetGrid(el,az);
	SetLayerConstants(helper,layerConstants);
	int numInstances=(int)helper->GetSlices().size();
	if(select_slice>=0)
		numInstances=1;
	UPDATE_CONSTANT_BUFFERS(pContext,instanceBuffer,LayerData,layerConstants.layers,numInstances)

	UPDATE_CONSTANT_BUFFER(pContext,layerConstantsBuffer,LayerConstants,layerConstants)
	ID3DX11EffectConstantBuffer* cbLayerConstants=m_pCloudEffect->GetConstantBufferByName("LayerConstants");
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
	else
	{
		ApplyPass(pContext,m_hTechniqueRaytrace->GetPassByIndex(0));
	}
	UtilityRenderer::DrawQuad(pContext);
	PIXEndNamedEvent();
	skyLossTexture->SetResource(NULL);
	skyInscatterTexture->SetResource(NULL);
	skylightTexture->SetResource(NULL);
	depthTexture->SetResource(NULL);
// To prevent BIZARRE DX11 warning, we re-apply the pass with the UtilityRenderer::RenderTextures unbound:
	if(Raytrace)
	{
		ApplyPass(pContext,m_hTechniqueRaytrace->GetPassByIndex(0));
	}
	else
	{
		if(enable_lightning)
			ApplyPass(pContext,m_hTechniqueCloudsAndLightning->GetPassByIndex(0));
		else
			ApplyPass(pContext,m_hTechniqueCloud->GetPassByIndex(0));
	}
	pContext->OMSetBlendState(NULL, blendFactor, sampleMask);
	// This actually returns the PREVIOUS FRAME's time value:
	gpu_time*=0.99f;
	gpu_time+=0.01f*profileBlock.GetTime();
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

void SimulCloudRendererDX1x::RenderCrossSections(void *context,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
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
	D3DXVECTOR4 cross_section_offset(0,0,0,0);
	UtilityRenderer::SetScreenSize(width,height);
	if(skyInterface)
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		D3DXVECTOR4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		cloudConstants.lightResponse=light_response;
		cloudConstants.crossSectionOffset=vec3(0.5f,0.5f,0.f);
		cloudConstants.Apply(pContext);
		cloudDensity1->SetResource(cloud_textures[(i+texture_cycle)%3].shaderResourceView);
		UtilityRenderer::DrawQuad2(pContext	,i*(w+1)+4	,4		,w,h	,m_pCloudEffect	,m_hTechniqueCrossSectionXZ);
		UtilityRenderer::DrawQuad2(pContext	,i*(w+1)+4	,h+8	,w,w	,m_pCloudEffect	,m_hTechniqueCrossSectionXY);
	}
	/*cloudDensity1->SetResource(cloud_texture.srv);
	UtilityRenderer::DrawQuad2(pContext,2*(w+1)+4,4,w,h,m_pCloudEffect,m_hTechniqueCrossSectionXZ);
	UtilityRenderer::DrawQuad2(pContext,2*(w+1)+4,h+8,w,w,m_pCloudEffect,m_hTechniqueCrossSectionXY);*/
	simul::dx11::setParameter(m_pCloudEffect,"noiseTexture",noiseTextureResource);
	UtilityRenderer::DrawQuad2(pContext,width-(w+8),height-(w+8),w,w,m_pCloudEffect,m_pCloudEffect->GetTechniqueByName("show_noise"));
	simul::dx11::setParameter(m_pCloudEffect,"noiseTexture",(ID3D1xShaderResourceView*)cloudShadow.GetColorTex());
	UtilityRenderer::DrawQuad2(pContext,width-(w+8)-(w+8),height-(w+8),w,w,m_pCloudEffect,m_pCloudEffect->GetTechniqueByName("show_shadow"));
}

bool SimulCloudRendererDX1x::RenderLightning(void *context,int viewport_id)
{
	if(!enable_lightning)
		return S_OK;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	using namespace simul::clouds;

	if(!lightning_vertices)
		lightning_vertices=new PosTexVert_t[4500];

	HRESULT hr=S_OK;

	PIXBeginNamedEvent(0, "Render Lightning");
		
	//set up matrices
	D3DXMATRIX wvp;
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	cam_pos=GetCameraPosVector(view);

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	pContext->IASetInputLayout(m_pLightningVtxDecl);
	simul::math::Vector3 pos;

	static float lm=10.f;
	static float main_bright=1.f;
	int vert_start=0;
	int vert_num=0;
	ApplyPass(pContext,m_hTechniqueLightning->GetPassByIndex(0));
	float time=skyInterface->GetTime();
	const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,0);

	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(viewport_id);
	l_worldViewProj->SetMatrix(&wvp._11);
	for(int i=0;i<lightningRenderInterface->GetNumLightSources();i++)
	{
		/*if(!lightningRenderInterface->IsSourceStarted(time))
			continue;
		simul::math::Vector3 x1,x2;
		float bright1=0.f,bright2=0.f;
		simul::math::Vector3 camPos(cam_pos);
		for(int jj=0;jj<lightningRenderInterface->GetNumBranches(0);jj++)
		{
			simul::math::Vector3 last_transverse;
			vert_start=vert_num;
			const simul::clouds::LightningRenderInterface::Branch &branch=lightningRenderInterface->GetBranch(time,0,jj);
			x1=(const float*)branch.vertices[0];
			float dist=(x1-camPos).Magnitude();
			float vertical_shift=helper->GetVerticalShiftDueToCurvature(dist,x1.z,R);
			for(int k=0;k<branch.numVertices-1;k++)
			{
				x1=(const float*)branch.vertices[k];
				x2=(const float*)branch.vertices[k+1];
				

				static float ww=700.f;
				float width=ww*branch.width;
				float width1=bright1*width;
				simul::math::Vector3 dx=x2-x1;
				simul::math::Vector3 transverse;
				CrossProduct(transverse,view_dir,dx);
				transverse.Unit();
				transverse*=width1;
				simul::math::Vector3 t=transverse;
				if(k)
					t=.5f*(last_transverse+transverse);
				simul::math::Vector3 x1a=x1-t;
				simul::math::Vector3 x1b=x1+t;
				if(!k)
					bright1=0;
				if(k==branch.numVertices-1)
					bright2=0;
				PosTexVert_t &v1=lightning_vertices[vert_num++];
				if(y_vertical)
				{
					std::swap(x1a.y,x1a.z);
					std::swap(x1b.y,x1b.z);
				}
				v1.position.x=x1a.x;
				v1.position.y=x1a.y+vertical_shift;
				v1.position.z=x1a.z;
				v1.texCoords.x=0;
				//glMultiTexCoord4f(GL_TEXTURE1,lm*lightning_red,lm*lightning_green,lm*lightning_blue,bright);  //Fade off the tips of end branches.
				PosTexVert_t &v2=lightning_vertices[vert_num++];
				v2.position.x=x1b.x;
				v2.position.y=x1b.y+vertical_shift;
				v2.position.z=x1b.z;
				v2.texCoords.x=1.f;
				//glMultiTexCoord4f(GL_TEXTURE1,lm*lightning_red,lm*lightning_green,lm*lightning_blue,bright);  //Fade off the tips of end branches.
				last_transverse=transverse;
			}
			if(vert_num-vert_start>2)
				pContext->Draw(vert_num-vert_start-2,0);//PrimitiveUP(D3DPT_TRIANGLESTRIP,vert_num-vert_start-2,&(lightning_vertices[vert_start]),sizeof(PosTexVert_t));
		}*/
	}
//	hr=m_pLightningEffect->EndPass();
//	hr=m_pLightningEffect->End();
	PIXEndNamedEvent();
	return (hr==S_OK);
}

void SimulCloudRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
	cam_pos=GetCameraPosVector(view);
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
	CloudShadowStruct s=BaseCloudRenderer::GetCloudShadowTexture();
	s.texture=cloudShadow.GetColorTex();
	return s;
}

void SimulCloudRendererDX1x::SetYVertical(bool y)
{
	BaseCloudRenderer::SetYVertical(y);
	CreateCloudEffect();
}

bool SimulCloudRendererDX1x::IsYVertical() const
{
	return y_vertical;
}

void SimulCloudRendererDX1x::EnsureCorrectTextureSizes()
{
	simul::clouds::CloudKeyframer::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	bool uav=gpuCloudGenerator.GetEnabled()&&cloudKeyframer->GetGpuCloudGenerator()==&gpuCloudGenerator;
	static DXGI_FORMAT cloud_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;
	for(int i=0;i<3;i++)
	{
		cloud_textures[i].ensureTexture3DSizeAndFormat(m_pd3dDevice,width_x,length_y,depth_z,cloud_tex_format,uav);
	}
	if(!width_x||!length_y||!depth_z)
		return;
	if(width_x==cloud_tex_width_x&&length_y==cloud_tex_length_y&&depth_z==cloud_tex_depth_z&&cloud_textures[texture_cycle%3].texture!=NULL)
		return;
	cloudShadow.SetGenerateMips(false/*true*/);
	cloudShadow.SetWidthAndHeight(64,64);
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	HRESULT hr=S_OK;
	
	//cloud_texture.ensureTexture3DSizeAndFormat(m_pd3dDevice,width_x,length_y,depth_z,cloud_tex_format,true);
}

void SimulCloudRendererDX1x::EnsureTexturesAreUpToDate(void *context)
{
	EnsureCorrectTextureSizes();
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	EnsureTextureCycle();
	int a	=cloudKeyframer->GetEdgeNoiseTextureSize();
	int b	=cloudKeyframer->GetEdgeNoiseFrequency();
	int c	=cloudKeyframer->GetEdgeNoiseOctaves();
	float d	=cloudKeyframer->GetEdgeNoisePersistence();
	unsigned check=a+b+c+(*(unsigned*)(&d));
	if(check!=noise_checksum)
	{
		SAFE_RELEASE(noise_texture);
		noise_checksum=check;
	}
	if(!noise_texture)
		CreateNoiseTexture(pContext);
	// We don't need to fill the textures if the gpu Generator has already done so:
	if(cloudKeyframer->GetGpuCloudGenerator()==&gpuCloudGenerator&&gpuCloudGenerator.GetEnabled())
		return;
	for(int i=0;i<3;i++)
	{
		TextureStruct &texture=cloud_textures[(texture_cycle+i)%3];
		simul::sky::BaseKeyframer::seq_texture_fill texture_fill=cloudKeyframer->GetSequentialTextureFill(seq_texture_iterator[i]);
		if(!texture_fill.num_texels)
			continue;
		if(!texture.isMapped()&&texture_fill.texel_index>0&&texture_fill.num_texels>0)
		{
			seq_texture_iterator[i].texel_index=0;
			texture_fill=cloudKeyframer->GetSequentialTextureFill(seq_texture_iterator[i]);
		}
		Map(pContext,i);
		unsigned *ptr=(unsigned *)texture.mapped.pData;
		if(!ptr)
			continue;
		ptr+=texture_fill.texel_index;
		memcpy(ptr,texture_fill.uint32_array,texture_fill.num_texels*sizeof(unsigned));
		if(i!=2)
			Unmap();
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
		//std::swap(cloud_textures[0],cloud_textures[1]);
		//std::swap(cloud_textures[1],cloud_textures[2]);
		std::swap(seq_texture_iterator[0],seq_texture_iterator[1]);
		std::swap(seq_texture_iterator[1],seq_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}


