// Copyright (c) 2007-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulCloudRendererDX1x.cpp A renderer for 3d clouds.
#define NOMINMAX
#include "SimulCloudRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include <fstream>
#include <math.h>
#include <tchar.h>
#include <dxerr.h>
#include <string>

#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/ThunderCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Clouds/LightningRenderInterface.h"
using namespace simul::dx11;
const char *GetErrorText(HRESULT hr)
{
	const char *err=DXGetErrorStringA(hr);
	return err;
}

typedef std::basic_string<TCHAR> tstring;
static tstring filepath=TEXT("");
DXGI_FORMAT illumination_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;
const bool big_endian=false;
static unsigned default_mip_levels=0;
static unsigned bits[4]={	(unsigned)0x0F00,
							(unsigned)0x000F,
							(unsigned)0x00F0,
							(unsigned)0xF000};

#ifndef FAIL_RETURN
#define FAIL_RETURN(x)    { hr = x; if( FAILED(hr) ) { return; } }
#endif


typedef std::basic_string<TCHAR> tstring;

struct CloudVertex_t
{
    vec3 position;
};
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

SimulCloudRendererDX1x::SimulCloudRendererDX1x(simul::clouds::CloudKeyframer *ck) :
	simul::clouds::BaseCloudRenderer(ck)
	,m_hTechniqueLightning(NULL)
	,m_hTechniqueCrossSectionXZ(NULL)
	,m_hTechniqueCrossSectionXY(NULL)
	,m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,m_pLightningVtxDecl(NULL)
	,m_pCloudEffect(NULL)
	,vertexBuffer(NULL)
	,indexBuffer(NULL)
	,instanceBuffer(NULL)
	,m_pLightningEffect(NULL)
	,cloudConstantsBuffer(NULL)
	,noise_texture(NULL)
	,lightning_texture(NULL)
	,illumination_texture(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
	,skylightTexture_SRV(NULL)
	,noiseTextureResource(NULL)
	,lightningIlluminationTextureResource(NULL)
	,blendAndWriteAlpha(NULL)
	,blendAndDontWriteAlpha(NULL)
	,enable_lightning(false)
	,lightning_active(false)
	,mapped(-1)
{
	mapped_cloud_texture.pData=NULL;
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
	{
		cloud_textures[i]=NULL;
		cloudDensityResource[i]=NULL;
	}
	helper->SetYVertical(y_vertical);
	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
	texel_index[0]=texel_index[1]=texel_index[2]=texel_index[3]=0;
}

void SimulCloudRendererDX1x::SetLossTexture(void *t)
{
	skyLossTexture_SRV=(ID3D11ShaderResourceView*)t;
}

void SimulCloudRendererDX1x::SetInscatterTextures(void *t,void *s)
{
	skyInscatterTexture_SRV=(ID3D11ShaderResourceView*)t;
	skylightTexture_SRV=(ID3D11ShaderResourceView*)s;
}

void SimulCloudRendererDX1x::SetNoiseTextureProperties(int s,int f,int o,float p)
{
	noise_texture_size=s;
	noise_texture_frequency=f;
	texture_octaves=o;
	texture_persistence=p;
	SAFE_RELEASE(noiseTextureResource);
}

void SimulCloudRendererDX1x::RecompileShaders()
{
	CreateCloudEffect();
	HRESULT hr;
	V_CHECK(CreateEffect(m_pd3dDevice,&m_pLightningEffect,L"simul_lightning.fx"));
	if(m_pLightningEffect)
	{
		m_hTechniqueLightning	=m_pLightningEffect->GetTechniqueByName("simul_lightning");
		l_worldViewProj			=m_pLightningEffect->GetVariableByName("worldViewProj")->AsMatrix();
	}
	MAKE_CONSTANT_BUFFER(cloudConstantsBuffer,CloudConstants);
}

void SimulCloudRendererDX1x::RestoreDeviceObjects(void* dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
	ID3D11DeviceContext* m_pImmediateContext;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	HRESULT hr;
	CreateNoiseTexture(m_pImmediateContext);
		SAFE_RELEASE(m_pImmediateContext)
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
        { "TEXCOORD",	1, DXGI_FORMAT_R32_FLOAT,			1,	8,	D3D11_INPUT_PER_INSTANCE_DATA, 1 },	// noiseScale	
        { "TEXCOORD",	2, DXGI_FORMAT_R32_FLOAT,			1,	12,	D3D11_INPUT_PER_INSTANCE_DATA, 1 },	//  layerFade		
        { "TEXCOORD",	3, DXGI_FORMAT_R32_FLOAT,			1,	16,	D3D11_INPUT_PER_INSTANCE_DATA, 1 },	//  layerDistance	
    };
	const D3D11_INPUT_ELEMENT_DESC std_decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
	D3DX11_PASS_DESC PassDesc;
	ID3DX11EffectPass *pass=m_hTechniqueCloud->GetPassByIndex(0);
	hr=pass->GetDesc(&PassDesc);
//return true;
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);

	// Get a count of the elements in the layout.
	int numElements = sizeof(decl) / sizeof(decl[0]);

	hr=m_pd3dDevice->CreateInputLayout( decl,numElements,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	CreateMeshBuffers();
	
//return true;
	if(m_hTechniqueLightning)
	{
		pass=m_hTechniqueLightning->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);
		hr=m_pd3dDevice->CreateInputLayout( std_decl, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pLightningVtxDecl);
	}
//	cloudKeyframer->SetRenderCallback(NULL);
//	cloudKeyframer->SetRenderCallback(this);
	if(lightningIlluminationTextureResource)
		lightningIlluminationTexture->SetResource(lightningIlluminationTextureResource);
	ClearIterators();
	
	// two possible blend states for clouds - with alpha written, and without.
	D3D11_BLEND_DESC omDesc;
	ZeroMemory( &omDesc, sizeof( D3D11_BLEND_DESC ) );
	omDesc.RenderTarget[0].BlendEnable		= true;
	omDesc.RenderTarget[0].BlendOp			= D3D11_BLEND_OP_ADD;
	omDesc.RenderTarget[0].BlendOpAlpha		= D3D11_BLEND_OP_ADD;
	omDesc.RenderTarget[0].SrcBlend			= D3D11_BLEND_SRC_ALPHA;
	omDesc.RenderTarget[0].DestBlend		= D3D11_BLEND_INV_SRC_ALPHA;
	omDesc.RenderTarget[0].SrcBlendAlpha	= D3D11_BLEND_ZERO;
	omDesc.RenderTarget[0].DestBlendAlpha	= D3D11_BLEND_INV_SRC_ALPHA;
	omDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	omDesc.IndependentBlendEnable=true;
	omDesc.AlphaToCoverageEnable=false;
	m_pd3dDevice->CreateBlendState( &omDesc, &blendAndWriteAlpha );
	omDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED
										| D3D11_COLOR_WRITE_ENABLE_GREEN
										| D3D11_COLOR_WRITE_ENABLE_BLUE;
	m_pd3dDevice->CreateBlendState( &omDesc, &blendAndDontWriteAlpha );
}
	
void SimulCloudRendererDX1x::CreateMeshBuffers()
{
	helper->GenerateSphereVertices();
	int num_vertices=32;//(int)helper->GetVertices().size();	 
	int num_indices=33;//(int)helper->GetQuadStripIndices().size();
	// Create the main vertex buffer:
	D3D11_BUFFER_DESC vertexBufferDesc=
	{
        num_vertices*sizeof(CloudVertex_t),
        D3D11_USAGE_DYNAMIC,
        D3D11_BIND_VERTEX_BUFFER,
        D3D11_CPU_ACCESS_WRITE,
        0
	};
	CloudVertex_t *vertices=new CloudVertex_t[num_vertices];
	unsigned short *indices=new unsigned short[num_indices];
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData,sizeof(D3D11_SUBRESOURCE_DATA));

	CloudVertex_t *dest=vertices;
	for(int i=0;i<num_vertices;i++,dest++)
	{
		float angle=2.f*pi*(float)i/(float)num_vertices;
		dest->position.x = sin(angle);	//helper->GetVertices()[i].x;
		dest->position.y = cos(angle);	//helper->GetVertices()[i].y;
		dest->position.z = 0.f;			//helper->GetVertices()[i].z;
	}
	for(int i=0;i<num_indices;i++)
	{
	//	unsigned short s=helper->GetQuadStripIndices()[i];
		indices[i]=i%num_vertices;
	}
	
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(CloudVertex_t);
	HRESULT hr=m_pd3dDevice->CreateBuffer(&vertexBufferDesc,&InitData,&vertexBuffer);
	
	// index buffer
	D3D11_BUFFER_DESC indexBufferDesc=
	{
        num_indices*sizeof(unsigned short),
        D3D11_USAGE_DYNAMIC,
        D3D11_BIND_INDEX_BUFFER,
        D3D11_CPU_ACCESS_WRITE,
        0
	};
    ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
    InitData.pSysMem = indices;
    InitData.SysMemPitch = sizeof(unsigned short);
	hr=m_pd3dDevice->CreateBuffer(&indexBufferDesc,&InitData,&indexBuffer);
    
    delete [] vertices;
    delete [] indices;

	// Instance buffer:
	SAFE_RELEASE(instanceBuffer);
	D3D11_BUFFER_DESC instanceBufferDesc;
	D3D11_SUBRESOURCE_DATA instanceData;
		// Set up the description of the instance buffer.
	instanceBufferDesc.Usage				=D3D11_USAGE_DYNAMIC;
	instanceBufferDesc.ByteWidth			=sizeof(InstanceType)*200;
	instanceBufferDesc.BindFlags			=D3D11_BIND_VERTEX_BUFFER;
	instanceBufferDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_WRITE;
	instanceBufferDesc.MiscFlags			=0;
	instanceBufferDesc.StructureByteStride	=0;
	// Give the subresource structure a pointer to the instance data.
	instanceData.pSysMem = instances;
	instanceData.SysMemPitch = 0;
	instanceData.SysMemSlicePitch = 0;

	// Create the instance buffer.
	m_pd3dDevice->CreateBuffer(&instanceBufferDesc, &instanceData, &instanceBuffer);
	{
		ID3D11DeviceContext* m_pImmediateContext;
		m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
		D3D11_MAPPED_SUBRESOURCE mapped_res;
		m_pImmediateContext->Map(instanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		for(int i=0;i<200;i++)
			((InstanceType*)(mapped_res.pData))[i]=instances[i];
		m_pImmediateContext->Unmap(instanceBuffer, 0);
		SAFE_RELEASE(m_pImmediateContext)
	}
}

void SimulCloudRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	Unmap();
	if(illumination_texture)
		Unmap3D(mapped_context,illumination_texture);
	SAFE_RELEASE(cloudConstantsBuffer);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);
	SAFE_RELEASE(m_pCloudEffect);
	SAFE_RELEASE(m_pLightningEffect);
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		SAFE_RELEASE(cloudDensityResource[i]);
	}
	// Set the stored texture sizes to zero, so the textures will be re-created.
	cloud_tex_width_x=cloud_tex_length_y=cloud_tex_depth_z=0;
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(lightning_texture);
	SAFE_RELEASE(illumination_texture);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	SAFE_RELEASE(instanceBuffer);
	skyLossTexture_SRV		=NULL;
	skyInscatterTexture_SRV	=NULL;
	skylightTexture_SRV		=NULL;
	SAFE_RELEASE(blendAndWriteAlpha);
	SAFE_RELEASE(blendAndDontWriteAlpha);

	SAFE_RELEASE(noiseTextureResource);
	
	SAFE_RELEASE(lightningIlluminationTextureResource);
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
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
void SimulCloudRendererDX1x::RenderNoise(void *context)
{
	ID3D11DeviceContext* m_pImmediateContext=(ID3D11DeviceContext*)context;
	ID3D1xEffect*					effect=NULL;
	ID3D1xEffectTechnique*			randomTechnique;
	ID3D1xEffectTechnique*			noiseTechnique;
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,L"simul_rendernoise.fx");
	randomTechnique					=effect->GetTechniqueByName("simul_random");
	noiseTechnique					=effect->GetTechniqueByName("simul_noise_2d");

	FramebufferDX1x	random_fb;
	random_fb.RestoreDeviceObjects(m_pd3dDevice);
	random_fb.SetWidthAndHeight(noise_texture_frequency,noise_texture_frequency);
	random_fb.SetFormat((int)DXGI_FORMAT_R32G32B32A32_FLOAT);
	ApplyPass(m_pImmediateContext,randomTechnique->GetPassByIndex(0));
	random_fb.Activate(context);
		random_fb.DrawQuad(context);
	random_fb.Deactivate(context);

	FramebufferDX1x n_fb;
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
		ApplyPass(m_pImmediateContext,noiseTechnique->GetPassByIndex(0));
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
	if(FAILED(hr=m_pImmediateContext->Map(noise_texture,0,D3D1x_MAP_WRITE_DISCARD,0,&mapped)))
		return;
	n_fb.CopyToMemory(mapped.pData);
	m_pImmediateContext->Unmap(noise_texture,0);
	V_CHECK(m_pd3dDevice->CreateShaderResourceView(noise_texture,NULL,&noiseTextureResource));

	SAFE_RELEASE(effect);
}

bool SimulCloudRendererDX1x::CreateNoiseTexture(void* context,bool override_file)
{
	if(!m_pd3dDevice)
		return false;
	RenderNoise(context);
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext*)context;
	return true;
	HRESULT hr=S_OK;
	SAFE_RELEASE(noise_texture);
	
	D3DX1x_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory( &loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO) );
	loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	loadInfo.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
loadInfo.MipLevels=0;
	if(!override_file)
	{
		/*hr=D3DX11CreateShaderResourceViewFromFile(
										m_pd3dDevice,
										TEXT("./Media/Textures/noise.dds"),
										&loadInfo,
										NULL,
										&noiseTextureResource,
										&hr);
		if(hr==S_OK)
		{
			return true;
		}*/
		const TCHAR *err=DXGetErrorString(hr);
		std::cerr<<err<<std::endl;
	}
	int mips=PowerOfTwo(noise_texture_size/2);
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
	hr=m_pd3dDevice->CreateTexture2D(&textureDesc,NULL,&noise_texture);
	V_CHECK(hr);
	D3D1x_MAPPED_TEXTURE2D mapped;
	if(!noise_texture)
		return false;
	if(FAILED(hr=m_pImmediateContext->Map(noise_texture,0,D3D1x_MAP_WRITE_DISCARD,0,&mapped)))
		return false;
	simul::clouds::TextureGenerator::SetBits(0x000000FF,0x0000FF00,0x00FF0000,0xFF000000,(unsigned)4,big_endian);
	simul::clouds::TextureGenerator::Make2DNoiseTexture(( char *)(mapped.pData),noise_texture_size,noise_texture_frequency,texture_octaves,texture_persistence);
	m_pImmediateContext->Unmap(noise_texture,0);
	V_CHECK(m_pd3dDevice->CreateShaderResourceView(noise_texture,NULL,&noiseTextureResource));
	//m_pImmediateContext->GenerateMips(noiseTextureResource);
	return true;
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
	ID3D11DeviceContext* m_pImmediateContext;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	hr=m_pImmediateContext->Map(lightning_texture,0,D3D1x_MAP_WRITE_DISCARD,0,&resource);
	if(FAILED(hr))
		return (hr==S_OK);
	unsigned char *lightning_tex_data=(unsigned char *)(resource.pData);
	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float level=.5f*linear*linear+5.f*(linear>.97f);
		float r=lightning_colour.x*level;
		float g=lightning_colour.y*level;
		float b=lightning_colour.z*level;
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
	m_pImmediateContext->Unmap(lightning_texture,0);
	SAFE_RELEASE(m_pImmediateContext)
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
	if(mapped!=-1)
	{
		if(mapped>=0)
		{
			Unmap3D(mapped_context,cloud_textures[mapped]);
		}
		mapped=-1;
		mapped_cloud_texture.pData=NULL;
	}
}

void SimulCloudRendererDX1x::Map(ID3D11DeviceContext *context,int texture_index)
{
	HRESULT hr=S_OK;
	if(mapped!=texture_index)
	{
		Unmap();
		hr=Map3D(context,cloud_textures[texture_index],&mapped_cloud_texture);
		mapped=texture_index;
		mapped_context=context;
	}
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
	HRESULT hr=CreateEffect(m_pd3dDevice,&m_pCloudEffect,L"simul_clouds.fx",defines);
	m_hTechniqueCloud				=m_pCloudEffect->GetTechniqueByName("simul_clouds");
	m_hTechniqueCloudsAndLightning	=m_pCloudEffect->GetTechniqueByName("simul_clouds_and_lightning");

	m_hTechniqueCrossSectionXY		=m_pCloudEffect->GetTechniqueByName("cross_section_xy");
	m_hTechniqueCrossSectionXZ		=m_pCloudEffect->GetTechniqueByName("cross_section_xz");

	worldViewProj					=m_pCloudEffect->GetVariableByName("worldViewProj")->AsMatrix();
	wrld							=m_pCloudEffect->GetVariableByName("wrld")->AsMatrix();
	eyePosition						=m_pCloudEffect->GetVariableByName("eyePosition")->AsVector();
	lightResponse					=m_pCloudEffect->GetVariableByName("lightResponse")->AsVector();
	lightDir						=m_pCloudEffect->GetVariableByName("lightDir")->AsVector();
	ambientColour					=m_pCloudEffect->GetVariableByName("ambientColour")->AsVector();
	sunlightColour1					=m_pCloudEffect->GetVariableByName("sunlightColour1")->AsVector();
	sunlightColour2					=m_pCloudEffect->GetVariableByName("sunlightColour2")->AsVector();
	earthshadowMultiplier			=m_pCloudEffect->GetVariableByName("earthshadowMultiplier")->AsScalar();
	fractalScale					=m_pCloudEffect->GetVariableByName("fractalScale")->AsVector();
	cloud_interp					=m_pCloudEffect->GetVariableByName("cloud_interp")->AsScalar();
	mieRayleighRatio				=m_pCloudEffect->GetVariableByName("mieRayleighRatio")->AsVector();
	hazeEccentricity				=m_pCloudEffect->GetVariableByName("hazeEccentricity")->AsScalar();
	fadeInterp						=m_pCloudEffect->GetVariableByName("fadeInterp")->AsScalar();
	cloudEccentricity				=m_pCloudEffect->GetVariableByName("cloudEccentricity")->AsScalar();
	alphaSharpness					=m_pCloudEffect->GetVariableByName("alphaSharpness")->AsScalar();
	maxFadeDistanceMetres			=m_pCloudEffect->GetVariableByName("maxFadeDistanceMetres")->AsScalar();

	//if(enable_lightning)
	{
		lightningMultipliers		=m_pCloudEffect->GetVariableByName("lightningMultipliers")->AsVector();
		lightningColour				=m_pCloudEffect->GetVariableByName("lightningColour")->AsVector();
		illuminationOrigin			=m_pCloudEffect->GetVariableByName("illuminationOrigin")->AsVector();
		illuminationScales			=m_pCloudEffect->GetVariableByName("illuminationScales")->AsVector();
	}

	cloudDensity1					=m_pCloudEffect->GetVariableByName("cloudDensity1")->AsShaderResource();
	cloudDensity2					=m_pCloudEffect->GetVariableByName("cloudDensity2")->AsShaderResource();
	noiseTexture					=m_pCloudEffect->GetVariableByName("noiseTexture")->AsShaderResource();
	lightningIlluminationTexture	=m_pCloudEffect->GetVariableByName("lightningIlluminationTexture")->AsShaderResource();
	skyLossTexture					=m_pCloudEffect->GetVariableByName("skyLossTexture")->AsShaderResource();
	skyInscatterTexture				=m_pCloudEffect->GetVariableByName("skyInscatterTexture")->AsShaderResource();
	skylightTexture					=m_pCloudEffect->GetVariableByName("skylightTexture")->AsShaderResource();
	
	layerFade						=m_pCloudEffect->GetVariableByName("layerFade")->AsScalar();
	layerDistance					=m_pCloudEffect->GetVariableByName("layerDistance")->AsScalar();
	cornerPos						=m_pCloudEffect->GetVariableByName("cornerPos")->AsVector();
	//inverseScales					=m_pCloudEffect->GetVariableByName("inverseScales")->AsVector();
	noiseMatrix						=m_pCloudEffect->GetVariableByName("noiseMatrix")->AsMatrix();
	noiseScale						=m_pCloudEffect->GetVariableByName("noiseScale")->AsScalar();
	noiseOffset						=m_pCloudEffect->GetVariableByName("noiseOffset")->AsVector();
	return true;
}

static D3DXVECTOR4 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR4 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}

static float saturate(float c)
{
	return std::max(std::min(1.f,c),0.f);
}
static int test=29999;
bool SimulCloudRendererDX1x::Render(void* context,bool cubemap,void *depth_tex,bool default_fog,bool write_alpha)
{
	if (test < 1)
	{
		return true;
	}
	ID3D11DeviceContext* m_pImmediateContext=(ID3D11DeviceContext*)context;
	EnsureTexturesAreUpToDate(m_pImmediateContext);
 
	if (test < 2)
	{
		return true;
	}
    ProfileBlock profileBlock(m_pImmediateContext,"SimulCloudRendererDX1x::Render");

	float blendFactor[] = {0, 0, 0, 0};
	UINT sampleMask   = 0xffffffff;
	if(write_alpha)
		m_pImmediateContext->OMSetBlendState(blendAndWriteAlpha, blendFactor, sampleMask);
	else
		m_pImmediateContext->OMSetBlendState(blendAndDontWriteAlpha, blendFactor, sampleMask);

	if (test < 3)
	{
		return true;
	}

	HRESULT hr=S_OK;
	PIXBeginNamedEvent(1,"Render Clouds Layers");
	cloudDensity1->SetResource(cloudDensityResource[0]);
	cloudDensity2->SetResource(cloudDensityResource[1]);
	noiseTexture->SetResource(noiseTextureResource);
	skyLossTexture->SetResource(skyLossTexture_SRV);
	skyInscatterTexture->SetResource(skyInscatterTexture_SRV);
	skylightTexture->SetResource(skylightTexture_SRV);

	if (test < 4)
	{
		return true;
	}

		simul::math::Vector3 X1=cloudKeyframer->GetCloudInterface()->GetOrigin();
		simul::math::Vector3 InverseDX=cloudKeyframer->GetCloudInterface()->GetInverseScales();
	if(ReverseDepth)
	{
		D3DXMATRIX invertz;
		D3DXMatrixIdentity(&invertz);
		invertz.m[2][2] = -1.0f;
		invertz.m[3][2]	= 1.0f;
		D3DXMatrixMultiply(&proj,&proj,&invertz);
	}
	else
	// Mess with the proj matrix to extend the far clipping plane:
		simul::dx11::FixProjectionMatrix(proj,helper->GetMaxCloudDistance()*1.1f,IsYVertical());
	
	if (test < 5)
	{
		return true;
	}

	//set up matrices
	D3DXMATRIX wvp,world;
	D3DXMatrixIdentity(&world);
	D3DXMatrixTranslation(&world,cam_pos.x,cam_pos.y,cam_pos.z);
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
	simul::math::Vector3 wind_offset=GetCloudInterface()->GetWindOffset();

	if (test < 6)
	{
		return true;
	}

	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);
	X+=wind_offset;

	//worldViewProj->SetMatrix(&wvp._11);
	//wrld->SetMatrixTranspose(&world._11);

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	if(!y_vertical)
		view_dir.Define(-view._13,-view._23,-view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	if (test < 7)
	{
		return true;
	}

	simul::sky::float4 view_km=(const float*)cam_pos;
	if(!cubemap)
	{
		helper->Update((const float*)cam_pos,wind_offset,view_dir,up);
	}

	if (test < 8)
	{
		return true;
	}

	view_km*=0.001f;
	static float direct_light_mult=0.25f;
	static float indirect_light_mult=0.03f;
	simul::sky::float4 light_response(	direct_light_mult*GetCloudInterface()->GetLightResponse(),
										indirect_light_mult*GetCloudInterface()->GetSecondaryLightResponse(),
										0,
										0);
	float base_alt_km=0.001f*(GetCloudInterface()->GetCloudBaseZ());
	float top_alt_km=base_alt_km+GetCloudInterface()->GetCloudHeight()*0.001f;
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToSun();
	simul::sky::float4 light_dir=skyInterface->GetDirectionToLight(base_alt_km);
	simul::sky::float4 sky_light_colour=skyInterface->GetAmbientLight(base_alt_km)*GetCloudInterface()->GetAmbientLightResponse();
	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetNoFrustumLimit(true);//cubemap);
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	helper->MakeGeometry(GetCloudInterface(),GetCloudGridInterface(),enable_lightning);

	if (test < 9)
	{
		return true;
	}

	ID3D1xEffectConstantBuffer* cbUser=m_pCloudEffect->GetConstantBufferByName("cbUser");
	if(cbUser)
	{
		ID3DX11EffectVectorVariable *lr=cbUser->GetMemberByName("lightResponse")->AsVector();
	}
	simul::sky::float4 sunlight1,sunlight2;
	CloudConstants cloudConstants;
	memset(&cloudConstants,0,sizeof(cloudConstants));
	
	if (test < 10)
	{
		return true;
	}

	cloudConstants.worldViewProj=wvp;
	cloudConstants.worldViewProj.transpose();
	cloudConstants.wrld=world;
	cloudConstants.wrld.transpose();
	if(skyInterface)
	{
		simul::sky::EarthShadow e=skyInterface->GetEarthShadow(base_alt_km,sun_dir);
		if(e.enable)
		{
			sunlight1=skyInterface->GetLocalIrradiance(base_alt_km)*saturate(base_alt_km-e.illumination_altitude);
			sunlight2=skyInterface->GetLocalIrradiance(top_alt_km)*saturate(top_alt_km-e.illumination_altitude);
			cloudConstants.earthshadowMultiplier=saturate(base_alt_km-e.illumination_altitude);
		}
		else
		{
			sunlight1=skyInterface->GetLocalIrradiance(base_alt_km);
			sunlight2=skyInterface->GetLocalIrradiance(top_alt_km);
			cloudConstants.earthshadowMultiplier=1.f;
		}
	}

	if (test < 11)
	{
		return true;
	}

	cloudConstants.cloud_interp		=cloudKeyframer->GetInterpolation();
	//eyePosition			->SetFloatVector	(cam_pos);
	cloudConstants.lightResponse	=light_response;

	cloudConstants.lightDir			=light_dir;
	cloudConstants.ambientColour	=sky_light_colour;
	cloudConstants.sunlightColour1	=sunlight1;
	cloudConstants.sunlightColour2=sunlight2;
	simul::sky::float4 fractal_scales=helper->GetFractalScales(GetCloudInterface());
	cloudConstants.fractalScale			=	(fractal_scales);
	simul::sky::float4 mie_rayleigh_ratio=skyInterface->GetMieRayleighRatio();
	cloudConstants.mieRayleighRatio		=	(mie_rayleigh_ratio);
	cloudConstants.cloudEccentricity	=(GetCloudInterface()->GetMieAsymmetry());
	cloudConstants.hazeEccentricity		=(skyInterface->GetMieEccentricity());
	cloudConstants.alphaSharpness		=(GetCloudInterface()->GetAlphaSharpness());
	cloudConstants.maxFadeDistanceMetres=(max_fade_distance_metres);
	
	if (test < 12)
	{
		return true;
	}

	cloudConstants.cornerPos		=X1;
	cloudConstants.inverseScales	=InverseDX;
	const simul::geometry::SimulOrientation &noise_orient=helper->GetNoiseOrientation();
	cloudConstants.noiseMatrix		=noise_orient.GetInverseMatrix();
	
	float time=skyInterface->GetTime();
	const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,0);

	if(enable_lightning)
	{
		static float bb=.2f;
		simul::sky::float4 lightning_multipliers;
		lightning_colour=lightningRenderInterface->GetLightningColour();
		for(int i=0;i<4;i++)
		{
			if(i<lightningRenderInterface->GetNumLightSources())
				lightning_multipliers[i]=bb*lightningRenderInterface->GetLightSourceBrightness(time);
			else lightning_multipliers[i]=0;
		}
		static float effect_on_cloud=20.f;
		lightning_colour.w=effect_on_cloud;
		cloudConstants.lightningMultipliers=	(lightning_multipliers);
		cloudConstants.lightningColour=	(lightning_colour);

	if (test < 13)
	{
		return true;
	}

		simul::math::Vector3 light_X1,light_X2,light_DX;
		light_DX.Define(lightningRenderInterface->GetLightningZoneSize(),
						lightningRenderInterface->GetLightningZoneSize(),
						GetCloudInterface()->GetCloudBaseZ()+GetCloudInterface()->GetCloudHeight());

//		light_X1=lightningRenderInterface->GetLightningCentreX();
		light_X1-=0.5f*light_DX;
		light_X1.z=0;
		//light_X2=lightningRenderInterface->GetLightningCentreX();
		light_X2+=0.5f*light_DX;
		light_X2.z=light_DX.z;

		cloudConstants.illuminationOrigin=light_X1;
		cloudConstants.illuminationScales=light_DX;
	}
	UPDATE_CONSTANT_BUFFER(m_pImmediateContext,cloudConstantsBuffer,CloudConstants,cloudConstants);
	ID3D1xEffectConstantBuffer* cbCloudConstants=m_pCloudEffect->GetConstantBufferByName("CloudConstants");
	if(cbCloudConstants)
		cbCloudConstants->SetConstantBuffer(cloudConstantsBuffer);
	int startv=0;
	int v=0;

	if (test < 14)
	{
		return true;
	}

	static int select_slice=-1;
	int ii=0;
	for(std::vector<simul::clouds::CloudGeometryHelper::Slice*>::const_iterator i=helper->GetSlices().begin();
		i!=helper->GetSlices().end();i++,ii++)
	{
		if(select_slice>=0&&select_slice!=ii)
			continue;
		simul::clouds::CloudGeometryHelper::Slice *s=*i;
		if(s==NULL)
			continue;
		InstanceType &inst=instances[ii];
		float noise_offset[]={s->noise_tex_x/cloudKeyframer->GetCloudInterface()->GetFractalRepeatLength()
				,s->noise_tex_y/cloudKeyframer->GetCloudInterface()->GetFractalRepeatLength(),0,0};
		inst.layerFade		=s->fadeIn;
		inst.layerDistance	=s->distance;
		inst.noiseScale		=s->distance/cloudKeyframer->GetCloudInterface()->GetFractalRepeatLength();
		inst.noiseOffset.x	=noise_offset[0];
		inst.noiseOffset.y	=noise_offset[1];
	}

	if (test < 15)
	{
		return true;
	}

	int numInstances=(int)helper->GetSlices().size();
	//UPDATE_CONSTANT_BUFFERS(instanceBuffer,InstanceType,instances,numInstances)
	{
		D3D11_MAPPED_SUBRESOURCE mapped_res;
		m_pImmediateContext->Map(instanceBuffer,0,D3D11_MAP_WRITE_DISCARD,0, &mapped_res);
		memset(mapped_res.pData,0,numInstances*sizeof(InstanceType));
		memcpy(mapped_res.pData,instances,numInstances*sizeof(InstanceType));
		//for(int i=0;i<numInstances;i++)
		//	((InstanceType*)(mapped_res.pData))[i]=instances[i];
		m_pImmediateContext->Unmap(instanceBuffer, 0);
	}

	if (test < 16)
	{
		return true;
	}

	// Update the instance buffer:
	// Set the input layout
	UINT strides[]={sizeof(CloudVertex_t),sizeof(InstanceType)};
	UINT offsets[]={0,0};
	ID3D11Buffer *buffers[]={vertexBuffer,instanceBuffer};

	ID3D11InputLayout* previousInputLayout;
	m_pImmediateContext->IAGetInputLayout( &previousInputLayout );

	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	m_pImmediateContext->IASetVertexBuffers(	0,			// the first input slot for binding
												2,			// the number of buffers in the array
												buffers,	// the array of vertex buffers
												strides,	// array of stride values, one for each buffer
												offsets);	// array of offset values, one for each buffer

				
	UINT Strides[1];
	UINT Offsets[1];
	Strides[0] = 0;
	Offsets[0] = 0;
	
	UINT prevOffset;
	DXGI_FORMAT prevFormat;
	ID3D11Buffer* pPrevBuffer;
	m_pImmediateContext->IAGetIndexBuffer(&pPrevBuffer, &prevFormat, &prevOffset);

	if (test < 17)
	{
		return true;
	}

	m_pImmediateContext->IASetIndexBuffer(	indexBuffer,
											DXGI_FORMAT_R16_UINT,	// unsigned short
											0);						// array of offset values, one for each buffer

	if (test < 18)
	{
		return true;
	}

	static int num_primitives=33;//helper->GetQuadStripIndices().size();//s->index_end-s->index_start;
	for(int i=0;i<(int)helper->GetSlices().size();i++)
	{
		InstanceType &inst=instances[i];
		//layerFade		->SetFloat(inst.layerFade);
		//layerDistance	->SetFloat(inst.layerDistance);
		//noiseScale		->SetFloat(inst.noiseScale);
		//noiseOffset		->SetFloatVector(inst.noiseOffset);
		float distance_tex_coord=inst.layerDistance/max_fade_distance_metres;

		static int cc=1;
		//if((num_primitives-cc)>0)
		//	m_pImmediateContext->DrawIndexed(num_primitives-cc,first_primitive,0);
		//	m_pImmediateContext->DrawIndexed((int)num_primitives-cc,0,0);
	}
   //ProfileBlock renderProfileBlock("rendering");
	if(enable_lightning)
		ApplyPass(m_pImmediateContext,m_hTechniqueCloudsAndLightning->GetPassByIndex(0));
	else
		ApplyPass(m_pImmediateContext,m_hTechniqueCloud->GetPassByIndex(0));
	m_pImmediateContext->//DrawInstanced(num_primitives,numInstances, 0, 0);
						DrawIndexedInstanced(num_primitives,numInstances,0,0,0);

	m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
	m_pImmediateContext->IASetInputLayout( previousInputLayout );
	m_pImmediateContext->IASetIndexBuffer(pPrevBuffer, prevFormat, prevOffset);
	SAFE_RELEASE(previousInputLayout);
	SAFE_RELEASE(pPrevBuffer);
	
	if (test < 19)
	{
		return true;
	}

	

	PIXEndNamedEvent();
	skyLossTexture->SetResource(NULL);
	skyInscatterTexture->SetResource(NULL);
	skylightTexture->SetResource(NULL);
// To prevent BIZARRE DX11 warning, we re-apply the pass with the UtilityRenderer::RenderTextures unbound:
	
	if(enable_lightning)
		ApplyPass(m_pImmediateContext,m_hTechniqueCloudsAndLightning->GetPassByIndex(0));
	else
		ApplyPass(m_pImmediateContext,m_hTechniqueCloud->GetPassByIndex(0));
	m_pImmediateContext->OMSetBlendState(NULL, blendFactor, sampleMask);
	// This actually returns the PREVIOUS FRAME's time value:

	if (test < 20)
	{
		return true;
	}

	render_time*=0.99f;
	render_time+=0.01f*profileBlock.GetTime();//profileBlock.GetTime();

	return (hr==S_OK);
}


void SimulCloudRendererDX1x::RenderDebugInfo(void *context,int width,int height)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext*)context;
	D3DXMATRIX ortho;
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
    D3DXMatrixOrthoLH(&ortho,(float)width,(float)height,-100.f,100.f);
	ortho._41=-1.f;
	ortho._22=-ortho._22;
	ortho._42=1.f;
	UtilityRenderer::SetMatrices(ident,ortho);
	simul::clouds::BaseCloudRenderer::RenderDebugInfo(m_pImmediateContext,width,height);
}

void SimulCloudRendererDX1x::DrawLines(void *context,VertexXyzRgba *vertices,int vertex_count,bool strip)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext*)context;
	simul::dx11::UtilityRenderer::DrawLines(m_pImmediateContext,vertices,vertex_count,strip);
}

void SimulCloudRendererDX1x::RenderCrossSections(void *context,int width,int height)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext*)context;
	HRESULT hr=S_OK;
	static int u=3;
	int w=(width-8)/u;
	if(w>height/2)
		w=height/2;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
	D3DXVECTOR4 cross_section_offset(
			(GetCloudInterface()->GetWrap()?0.5f:0.f)+0.5f/(float)cloud_tex_width_x
			,GetCloudInterface()->GetWrap()?0.5f:0.f+0.5f/(float)cloud_tex_length_y
			,0.5f/(float)cloud_tex_depth_z
			,0);
	D3DXMATRIX ortho;
    D3DXMatrixOrthoLH(&ortho,(float)width,(float)height,-100.f,100.f);
	ortho._14=-1.f;
	ortho._22=-ortho._22;
	ortho._24=1.f;
	ID3DX11EffectMatrixVariable*	worldViewProj=m_pCloudEffect->GetVariableByName("worldViewProj")->AsMatrix();

	CloudConstants cloudConstants;
	memset(&cloudConstants,0,sizeof(cloudConstants));
	cloudConstants.worldViewProj=ortho;
	cloudConstants.worldViewProj.transpose();
	ID3D1xEffectConstantBuffer* cbCloudConstants=m_pCloudEffect->GetConstantBufferByName("CloudConstants");
	UtilityRenderer::SetScreenSize(width,height);
	if(skyInterface)
	for(int i=0;i<2;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		D3DXVECTOR4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		if(cbCloudConstants)
		{
			cloudConstants.lightResponse=light_response;
			UPDATE_CONSTANT_BUFFER(m_pImmediateContext,cloudConstantsBuffer,CloudConstants,cloudConstants);
			cbCloudConstants->SetConstantBuffer(cloudConstantsBuffer);
		}
		cloudDensity1->SetResource(cloudDensityResource[i%3]);
		UtilityRenderer::RenderTexture(m_pImmediateContext,i*(w+1)+4,4,w,h,m_hTechniqueCrossSectionXZ);
		cloudDensity1->SetResource(cloudDensityResource[i%3]);
		UtilityRenderer::RenderTexture(m_pImmediateContext,i*(w+1)+4,h+8,w,w,m_hTechniqueCrossSectionXY);
	}
	simul::dx11::setParameter(m_pCloudEffect,"noiseTexture",noiseTextureResource);
	UtilityRenderer::RenderTexture(m_pImmediateContext,width-(w+8),height-(w+8),w,w,m_pCloudEffect->GetTechniqueByName("simple"));
}

bool SimulCloudRendererDX1x::RenderLightning(void *context)
{
	if(!enable_lightning)
		return S_OK;
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext*)context;
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

	m_pImmediateContext->IASetInputLayout(m_pLightningVtxDecl);
	simul::math::Vector3 pos;

	static float lm=10.f;
	static float main_bright=1.f;
	int vert_start=0;
	int vert_num=0;
	ApplyPass(m_pImmediateContext,m_hTechniqueLightning->GetPassByIndex(0));
	float time=skyInterface->GetTime();
	const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,0);

	l_worldViewProj->SetMatrix(&wvp._11);
	for(int i=0;i<lightningRenderInterface->GetNumLightSources();i++)
	{
		if(!lightningRenderInterface->IsSourceStarted(time))
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
			float vertical_shift=helper->GetVerticalShiftDueToCurvature(dist,x1.z);
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
				m_pImmediateContext->Draw(vert_num-vert_start-2,0);//PrimitiveUP(D3DPT_TRIANGLESTRIP,vert_num-vert_start-2,&(lightning_vertices[vert_start]),sizeof(PosTexVert_t));
		}
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

const TCHAR *SimulCloudRendererDX1x::GetDebugText() const
{
	static TCHAR debug_text[256];
	simul::math::Vector3 wo=GetCloudInterface()->GetWindOffset();
	_stprintf_s(debug_text,256,_T("cloud_interp %4.4g"),cloudKeyframer->GetInterpolation());
	return debug_text;
}


void SimulCloudRendererDX1x::SetEnableStorms(bool s)
{
	enable_lightning=s;
}

void *SimulCloudRendererDX1x::GetCloudShadowTexture()
{
	return NULL;
}

void SimulCloudRendererDX1x::SetYVertical(bool y)
{
	y_vertical=y;
	helper->SetYVertical(y);
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
	if(!width_x||!length_y||!depth_z)
		return;
	if(width_x==cloud_tex_width_x&&length_y==cloud_tex_length_y&&depth_z==cloud_tex_depth_z&&cloud_textures[0]!=NULL)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	HRESULT hr=S_OK;
	static DXGI_FORMAT cloud_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D11_TEXTURE3D_DESC textureDesc=
	{
		width_x,length_y,depth_z,
		1,
		cloud_tex_format,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0
	};
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		if(FAILED(hr=m_pd3dDevice->CreateTexture3D(&textureDesc,0,&cloud_textures[i])))
			return;
	}
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloudDensityResource[i]);
	    FAIL_RETURN(m_pd3dDevice->CreateShaderResourceView(cloud_textures[i],NULL,&cloudDensityResource[i]));
	}
}

void SimulCloudRendererDX1x::EnsureTexturesAreUpToDate(void *context)
{
	EnsureCorrectTextureSizes();
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext*)context;
	Map(m_pImmediateContext,2);
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		simul::sky::BaseKeyframer::seq_texture_fill texture_fill=cloudKeyframer->GetSequentialTextureFill(seq_texture_iterator[i]);
		if(!texture_fill.num_texels)
			continue;
		if(i!=mapped&&texture_fill.texel_index>0&&texture_fill.num_texels>0)
		{
			seq_texture_iterator[i].texel_index=0;
			texture_fill=cloudKeyframer->GetSequentialTextureFill(seq_texture_iterator[i]);
		}
		Map(m_pImmediateContext,i);
		unsigned *ptr=(unsigned *)mapped_cloud_texture.pData;
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
		std::swap(cloud_textures[0],cloud_textures[1]);
		std::swap(cloud_textures[1],cloud_textures[2]);
		std::swap(cloudDensityResource[0],cloudDensityResource[1]);
		std::swap(cloudDensityResource[1],cloudDensityResource[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
	Map(mapped_context,2);
}


