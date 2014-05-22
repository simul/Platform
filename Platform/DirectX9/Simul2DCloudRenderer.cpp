#define NOMINMAX
// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRenderer.cpp A renderer for 2D cloud layers.

#include "Simul2DCloudRenderer.h"

#ifdef XBOX
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static D3DFORMAT cloud_tex_format=D3DFMT_LIN_A4R4G4B4;
	const bool big_endian=true;
	static DWORD default_texture_usage=0;
	static unsigned default_mip_levels=1;
	static unsigned bits[4]={	(unsigned)0x0F00,
								(unsigned)0x000F,
								(unsigned)0xF000,
								(unsigned)0x00F0};
	static D3DPOOL default_d3d_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static D3DFORMAT cloud_tex_format=D3DFMT_A8R8G8B8;
	const bool big_endian=false;
	static DWORD default_texture_usage=D3DUSAGE_AUTOGENMIPMAP;
	static unsigned default_mip_levels=0;
	static unsigned bits[4]={	(unsigned)0x0F00,
								(unsigned)0x000F,
								(unsigned)0x00F0,
								(unsigned)0xF000};
	static unsigned bits8[4]={	(unsigned)0x00FF0000,
								(unsigned)0x000000FF,
								(unsigned)0x0000FF00,
								(unsigned)0xFF000000};
	static D3DPOOL default_d3d_pool=D3DPOOL_MANAGED;
#endif

#include "Macros.h"
#include "CreateDX9Effect.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"

struct float2
{
	float x,y;
	float2()
	{}
	float2(float X,float Y)
	{
		x=X;
		y=Y;
	}
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
	}
};
struct float3
{
	float x,y,z;
	float3()
	{}
	float3(float X,float Y,float Z)
	{
		x=X;
		y=Y;
		z=Z;
	}
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
		z=f[2];
	}
};
using namespace simul;
using namespace dx9;

struct PosTexVert_t
{
    float3 position;	
    float2 texCoords;
};

#define MAX_VERTICES (500)

static void SetBits8()
{
	simul::clouds::TextureGenerator::SetBits(bits8[0],bits8[1],bits8[2],bits8[3],(unsigned)4,big_endian);
}

Simul2DCloudRenderer::Simul2DCloudRenderer(simul::clouds::CloudKeyframer *ck,
										   simul::base::MemoryInterface *mem)
	:Base2DCloudRenderer(ck,mem)
	,m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,m_pCloudEffect(NULL)
	,noise_texture(NULL)
	,image_texture(NULL)
	,own_image_texture(true)
	,texture_scale(0.25f)
	,enabled(true)
{
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	simul::base::SmartPtr<simul::base::Referenced> test;
	test=new simul::base::Referenced;
	test=NULL;
	cloudKeyframer->SetMake2DTextures(true);
//	cloudKeyframer->InitKeyframesFromClouds();

	helper=new simul::clouds::Cloud2DGeometryHelper(memoryInterface);
	helper->Initialize(8);
	helper->SetGrid(12,24);
	
	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
}

bool Simul2DCloudRenderer::Create( LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr=S_OK;
	return (hr==S_OK);
}
struct Vertex2D_t
{
    float3 position;
    float2 texCoords;
    float2 texCoordNoise;
	float2 imageCoords;
};
Vertex2D_t *vertices=NULL;

void Simul2DCloudRenderer::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pCloudEffect,"simul_clouds_2d.fx"));

	m_hTechniqueCloud	=m_pCloudEffect->GetTechniqueByName("simul_clouds_2d");

	worldViewProj		=m_pCloudEffect->GetParameterByName(NULL,"worldViewProj");
	eyePosition			=m_pCloudEffect->GetParameterByName(NULL,"eyePosition");
	lightResponse		=m_pCloudEffect->GetParameterByName(NULL,"lightResponse");
	lightDir			=m_pCloudEffect->GetParameterByName(NULL,"lightDir");
	skylightColour		=m_pCloudEffect->GetParameterByName(NULL,"ambientColour");
	sunlightColour		=m_pCloudEffect->GetParameterByName(NULL,"sunlightColour");
	fractalScale		=m_pCloudEffect->GetParameterByName(NULL,"fractalScale");
	interp				=m_pCloudEffect->GetParameterByName(NULL,"interp");

	layerDensity		=m_pCloudEffect->GetParameterByName(NULL,"layerDensity");
	imageEffect			=m_pCloudEffect->GetParameterByName(NULL,"imageEffect");

	mieRayleighRatio	=m_pCloudEffect->GetParameterByName(NULL,"mieRayleighRatio");
	hazeEccentricity	=m_pCloudEffect->GetParameterByName(NULL,"hazeEccentricity");
	cloudEccentricity	=m_pCloudEffect->GetParameterByName(NULL,"cloudEccentricity");
	exposure			=m_pCloudEffect->GetParameterByName(NULL,"exposure");

	cloudDensity1		=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity1");
	cloudDensity2		=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity2");
	noiseTexture		=m_pCloudEffect->GetParameterByName(NULL,"noiseTexture");
	imageTexture		=m_pCloudEffect->GetParameterByName(NULL,"imageTexture");
}

void Simul2DCloudRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	HRESULT hr;
	D3DVERTEXELEMENT9 decl[]=
	{
		{0, 0	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0, 12	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0, 20	,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0, 36	,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		{0,	52	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,3},
		{0,	60	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,4},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	V_CHECK(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl))
	CreateNoiseTexture(m_pd3dDevice);
	hr=CreateImageTexture();
	RecompileShaders();
	// NOW can set the rendercallback, as we have a device to implement the callback fns with:
//	cloudKeyframer->SetRenderCallback(this);
}

void Simul2DCloudRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pCloudEffect)
        hr=m_pCloudEffect->OnLostDevice();
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pCloudEffect);
	for(int i=0;i<3;i++)
		SAFE_RELEASE(cloud_textures[i]);
	SAFE_RELEASE(noise_texture);
	if(own_image_texture)
	{
		SAFE_RELEASE(image_texture);
	}
	else
		image_texture=NULL;
}

Simul2DCloudRenderer::~Simul2DCloudRenderer()
{
	InvalidateDeviceObjects();
}

void Simul2DCloudRenderer::CreateNoiseTexture(void *)
{
	SAFE_RELEASE(noise_texture);
	// Can we load it from disk?
	HRESULT hr=S_OK;
//	if(!override_file)
//		if((hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/noise.dds"),&noise_texture))==S_OK)
//			return false;
	// Otherwise create it:
	int size=512;
	// NOTE: We specify ONE mipmap for this texture, NOT ZERO. If we use zero, that means
	// automatically generate mipmaps.
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,size,default_mip_levels,default_texture_usage,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&noise_texture)))
		return ;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=noise_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return ;
	SetBits8();
	simul::clouds::TextureGenerator::Make2DNoiseTexture(( char *)(lockedRect.pBits),size,16,8,0.8f);
	hr=noise_texture->UnlockRect(0);
	//noise_texture->GenerateMipSubLevels();
}

bool Simul2DCloudRenderer::CreateImageTexture()
{
	HRESULT hr=S_OK;
	if(!own_image_texture)
		return (hr==S_OK);
	SAFE_RELEASE(image_texture);
	if(FAILED(hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/Cirrocumulus.png"),&image_texture)))
		return (hr==S_OK);
	return (hr==S_OK);
}

void SetTexture()
{
}

void Simul2DCloudRenderer::PreRenderUpdate(crossplatform::DeviceContext &)
{
}

bool Simul2DCloudRenderer::Render(crossplatform::DeviceContext &deviceContext,float expos,bool cubemap
								  ,bool /*near_pass*/,const void *depth_alpha_tex,bool default_fog,bool,const simul::sky::float4& )
{
	return false;
}

#ifdef XBOX
void Simul2DCloudRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
#endif
void Simul2DCloudRenderer::SetWind(float speed,float heading_degrees)
{
	cloudKeyframer->SetWindSpeed(speed);
	cloudKeyframer->SetWindHeadingDegrees(heading_degrees);
}

void Simul2DCloudRenderer::SetExternalTexture(LPDIRECT3DTEXTURE9 tex)
{
	if(own_image_texture)
	{
		SAFE_RELEASE(image_texture);
	}
	else
		image_texture=NULL;
	image_texture=tex;
	own_image_texture=false;
}

void Simul2DCloudRenderer::EnsureCorrectTextureSizes()
{
	simul::sky::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	if(!width_x||!length_y||!depth_z)
		return;
	if(width_x==cloud_tex_width_x&&length_y==cloud_tex_length_y&&depth_z==cloud_tex_depth_z&&cloud_textures[0]!=NULL)
		return;
	assert(depth_z==1);
	depth_z;
	HRESULT hr=S_OK;
	V_CHECK(CanUseTexFormat(m_pd3dDevice,cloud_tex_format));
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,width_x,length_y,1,0,cloud_tex_format,default_d3d_pool,&cloud_textures[i])))
			return;
	}
}

void Simul2DCloudRenderer::EnsureTexturesAreUpToDate(void*)
{
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		simul::sky::seq_texture_fill texture_fill=cloudKeyframer->GetSequentialTextureFill(seq_texture_iterator[i]);
		if(!texture_fill.num_texels)
			continue;
		HRESULT hr=S_OK;
		D3DLOCKED_RECT lockedRect={0};
		if(FAILED(hr=cloud_textures[i]->LockRect(0,&lockedRect,NULL,NULL)))
			return;
		unsigned char *ptr=(unsigned char *)(lockedRect.pBits);
		ptr+=texture_fill.texel_index*sizeof(unsigned);
		memcpy(ptr,texture_fill.uint32_array,texture_fill.num_texels*sizeof(unsigned));
		hr=cloud_textures[i]->UnlockRect(0);
	}
}

void Simul2DCloudRenderer::EnsureTextureCycle()
{
	int cyc=(cloudKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(cloud_textures[0],cloud_textures[1]);
		std::swap(cloud_textures[1],cloud_textures[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}

simul::clouds::CloudKeyframer *Simul2DCloudRenderer::GetCloudKeyframer()
{
	return cloudKeyframer;
}

void Simul2DCloudRenderer::Enable(bool val)
{
	enabled=val;
}

const char *Simul2DCloudRenderer::GetDebugText() const
{
	static char debug_text[256];
	return debug_text;
}

void Simul2DCloudRenderer::RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int screen_width,int )
{
	int w=(screen_width-16)/6;
	LPDIRECT3DVERTEXDECLARATION9	m_pBufferVertexDecl=NULL;
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  8, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pBufferVertexDecl);
	V_CHECK(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl));
#ifdef XBOX
	float x=-1.f,y=1.f;
	float w=2.f;
	float h=-2.f;
	struct Vertext
	{
		float x,y;
		float tx,ty;
	};
	Vertext vertices[4] =
	{
		{x,			y,			0	,0},
		{x+w,		y,			1.f	,0},
		{x+w,		y+h,		1.f	,1.f},
		{x,			y+h,		0	,1.f},
	};
#else
	float x=(float)x0,y=(float)y0;
	struct Vertext
	{
		float x,y,z,h;
		float tx,ty;
	};
	int width=w,height=w;
	Vertext vertices[4] =
	{
		{x,			y,			1,	1, 0.5f	,0.5f},
		{x+width,	y,			1,	1, 1.5f	,0.5f},
		{x+width,	y+height,	1,	1, 1.5f	,1.5f},
		{x,			y+height,	1,	1, 0.5f	,1.5f},
	};
#endif
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

	m_pd3dDevice->SetVertexDeclaration(m_pBufferVertexDecl);

#ifndef XBOX
	m_pd3dDevice->SetTransform(D3DTS_VIEW,&ident);
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&ident);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&ident);
#endif
	m_pd3dDevice->SetTexture(0,cloud_textures[0]);
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));

	for(int i=0;i<4;i++)
		vertices[i].x+=w+16;
	m_pd3dDevice->SetTexture(0,cloud_textures[1]);
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
	for(int i=0;i<4;i++)
		vertices[i].x+=w+16;
	m_pd3dDevice->SetTexture(0,cloud_textures[2]);
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
	SAFE_RELEASE(m_pBufferVertexDecl);
}
