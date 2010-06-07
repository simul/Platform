// Copyright (c) 2007-2009 Simul Software Ltd
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
	static tstring filepath=TEXT("game:\\");
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
	static tstring filepath=TEXT("");
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
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/LicenseKey.h"
//
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }
#endif

typedef std::basic_string<TCHAR> tstring;

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

struct PosTexVert_t
{
    float3 position;	
    float2 texCoords;
};

#define MAX_VERTICES (5000)

/*static void SetBits4()
{
	simul::clouds::TextureGenerator::SetBits(bits[0],bits[1],bits[2],bits[3],2,big_endian);
}*/
static void SetBits8()
{
	simul::clouds::TextureGenerator::SetBits(bits8[0],bits8[1],bits8[2],bits8[3],(unsigned)4,big_endian);
}


Simul2DCloudRenderer::Simul2DCloudRenderer() :
	skyInterface(NULL),
	fadeTableInterface(NULL),
	cloudInterface(NULL),
	m_pd3dDevice(NULL),
	m_pVtxDecl(NULL),
	m_pCloudEffect(NULL),
	noise_texture(NULL),
	image_texture(NULL),
	cloud_texel_index(0),
	texture_scale(0.25f),
	texture_complete(false)
{
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	simul::base::SmartPtr<simul::base::Referenced> test;
	test=new simul::base::Referenced;
	test=NULL;

	cloudNode=new simul::clouds::FastCloudNode;
	cloudNode->SetLicense(SIMUL_LICENSE_KEY);
	cloudInterface=cloudNode.get();
	
	cloudInterface->SetWrap(true);
	cloudInterface->SetThinLayer(true);

	cloudInterface->SetGridLength(128);
	cloudInterface->SetGridWidth(128);
	cloudInterface->SetGridHeight(2);

	cloudInterface->SetCloudBaseZ(20000.f);
	cloudInterface->SetCloudWidth(120000.f);
	cloudInterface->SetCloudLength(120000.f);
	cloudInterface->SetCloudHeight(1200.f);

	cloudInterface->SetFractalEffectScale(20.f);
	cloudInterface->SetFractalPatternScale(100.f);

	cloudInterface->SetOpticalDensity(1.5f);
	cloudInterface->SetHumidity(.45f);

	cloudInterface->SetExtinction(2.9f);
	cloudInterface->SetLightResponse(1.f);
	cloudInterface->SetSecondaryLightResponse(1.f);
	cloudInterface->SetAmbientLightResponse(1.f);
	cloudInterface->SetAnisotropicLightResponse(6.f);

	cloudInterface->SetNoiseResolution(8);
	cloudInterface->SetNoiseOctaves(5);
	cloudInterface->SetNoisePersistence(0.75f);
	cloudInterface->SetNoisePeriod(128);

	cloudInterface->SetSelfShadowScale(0.1f);
	
	cloudNode->SetDiffusivity(.5f);
	cloudKeyframer=new simul::clouds::CloudKeyframer(cloudInterface,true);
	cloudKeyframer->SetUse16Bit(false);
	// 1/2 game-hour for interpolation:
	cloudKeyframer->SetInterpStepTime(1.f/24.f/2.f);

	helper=new simul::clouds::Cloud2DGeometryHelper();
	helper->SetYVertical(true);
	helper->Initialize(6,320000.f);
	helper->SetGrid(6,9);
	helper->SetCurvedEarth(true);
	
	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
}

void Simul2DCloudRenderer::SetSkyInterface(simul::sky::SkyInterface *si)
{
	skyInterface=si;
	cloudKeyframer->SetSkyInterface(si);
}

void Simul2DCloudRenderer::SetFadeTableInterface(simul::sky::FadeTableInterface *fti)
{
	fadeTableInterface=fti;
}

HRESULT Simul2DCloudRenderer::Create( LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr=S_OK;
	return hr;
}
struct Vertex2D_t
{
    float3 position;
    float2 texCoords;
	simul::sky::float4 loss;
    simul::sky::float4 inscatter;
    float2 texCoordNoise;
	float2 imageCoords;
};
Vertex2D_t *vertices=NULL;

HRESULT Simul2DCloudRenderer::RestoreDeviceObjects( LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
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
	V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl))
	V_RETURN(CreateNoiseTexture());
	V_RETURN(CreateImageTexture());
	V_RETURN(CreateDX9Effect(m_pd3dDevice,m_pCloudEffect,"simul_clouds_2d.fx"));

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

	cloudDensity1		=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity1");
	cloudDensity2		=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity2");
	noiseTexture		=m_pCloudEffect->GetParameterByName(NULL,"noiseTexture");
	imageTexture		=m_pCloudEffect->GetParameterByName(NULL,"imageTexture");

	// NOW can set the rendercallback, as we have a device to implement the callback fns with:
	cloudKeyframer->SetRenderCallback(this);
	return hr;
}

HRESULT Simul2DCloudRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pCloudEffect)
        hr=m_pCloudEffect->OnLostDevice();
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pCloudEffect);
	for(int i=0;i<3;i++)
		SAFE_RELEASE(cloud_textures[i]);
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(image_texture);
	return hr;
}

HRESULT Simul2DCloudRenderer::Destroy()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pVtxDecl);
	if(m_pCloudEffect)
        hr=m_pCloudEffect->OnLostDevice();
	SAFE_RELEASE(m_pCloudEffect);
	for(int i=0;i<3;i++)
		SAFE_RELEASE(cloud_textures[i]);
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(image_texture);
	return hr;
}

Simul2DCloudRenderer::~Simul2DCloudRenderer()
{
	Destroy();
}

HRESULT Simul2DCloudRenderer::CreateNoiseTexture()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(noise_texture);
	// Can we load it from disk?
	if((hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/noise.dds"),&noise_texture))==S_OK)
		return hr;
	// Otherwise create it:
	int size=512;
	// NOTE: We specify ONE mipmap for this texture, NOT ZERO. If we use zero, that means
	// automatically generate mipmaps.
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,size,default_mip_levels,default_texture_usage,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&noise_texture)))
		return hr;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=noise_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return hr;
	SetBits8();
	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)(lockedRect.pBits),size,16,8,0.8f);
	hr=noise_texture->UnlockRect(0);
	//noise_texture->GenerateMipSubLevels();
	return hr;
}

HRESULT Simul2DCloudRenderer::CreateImageTexture()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(image_texture);
	if(FAILED(hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/Cirrus2.jpg"),&image_texture)))
		return hr;
	return hr;
}
/*
HRESULT Simul2DCloudRenderer::UpdateCloudTexture()
{
	HRESULT hr=S_OK;
	if(!texture_complete&&cloud_interp-0.6f>0.f)
	{
		unsigned	w=cloudInterface->GetGridWidth(),
					h=cloudInterface->GetGridLength();
		unsigned total_texels=w*h;
		unsigned next_index=(unsigned)((float)(total_texels*(3.3f*(cloud_interp-.6f))));
		unsigned texels=next_index-cloud_texel_index;
		SetBits8();
		D3DLOCKED_RECT lockedBox={0};
		if(FAILED(hr=cloud_textures[2]->LockRect(0,&lockedBox,NULL,NULL)))
			return hr;
		// RGBA bit-shift is 12,8,4,0
		// ARGB however, is 8,0,4,12
		simul::clouds::TextureGenerator::Partial2DCloudTextureFill(
			cloudInterface,cloud_texel_index,texels,(unsigned char *)(lockedBox.pBits));
		hr=cloud_textures[2]->UnlockRect(0);
		cloud_texel_index+=texels;
		// Only mark as complete if all texels have been filled:
		if(cloud_texel_index>=total_texels)
		{
			texture_complete=true;
			cloud_texel_index=0;
		}
	}
	return hr;
}
*/
void Simul2DCloudRenderer::SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	assert(depth_z==1);
	HRESULT hr=S_OK;
	V_CHECK(CanUseTexFormat(m_pd3dDevice,cloud_tex_format));
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,width_x,length_y,1,0,cloud_tex_format,default_d3d_pool,&cloud_textures[i])))
			return;
	}
}

void Simul2DCloudRenderer::FillCloudTexture(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array)
{
	HRESULT hr=S_OK;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=cloud_textures[texture_index]->LockRect(0,&lockedRect,NULL,NULL)))
		return;
	unsigned *ptr=(unsigned *)(lockedRect.pBits);
	ptr+=texel_index;
	memcpy(ptr,uint32_array,num_texels*sizeof(unsigned));
	hr=cloud_textures[texture_index]->UnlockRect(0);
}


void Simul2DCloudRenderer::Update(float )
{
	if(!cloud_textures[2])
		return;
	if(!cloudInterface)
		return;
	float current_time=skyInterface->GetDaytime();
	cloudKeyframer->Update(current_time);
}

HRESULT Simul2DCloudRenderer::RenderTexture()
{
	HRESULT hr=S_OK;
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
	V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl));
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
	float x=0,y=0;
	struct Vertext
	{
		float x,y,z,h;
		float tx,ty;
	};
	int width=256,height=256;
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
		vertices[i].x+=288;
	m_pd3dDevice->SetTexture(0,cloud_textures[1]);
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
	for(int i=0;i<4;i++)
		vertices[i].x+=288;
	m_pd3dDevice->SetTexture(0,cloud_textures[2]);
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
	SAFE_RELEASE(m_pBufferVertexDecl);
	return hr;
}

void Simul2DCloudRenderer::CycleTexturesForward()
{
	std::swap(cloud_textures[0],cloud_textures[1]);
	std::swap(cloud_textures[1],cloud_textures[2]);
}
HRESULT Simul2DCloudRenderer::Render()
{
	PIXBeginNamedEvent(0, "Render 2D Cloud Layers");

	HRESULT hr;
	if(!vertices)
		vertices=new Vertex2D_t[MAX_VERTICES];

	m_pCloudEffect->SetTechnique( m_hTechniqueCloud );

	m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[0]);
	m_pCloudEffect->SetTexture(cloudDensity2				,cloud_textures[1]);
	m_pCloudEffect->SetTexture(noiseTexture					,noise_texture);
	m_pCloudEffect->SetTexture(imageTexture					,image_texture);

	// Mess with the proj matrix to extend the far clipping plane:
	// According to the documentation for D3DXMatrixPerspectiveFovLH:
	// proj._33=zf/(zf-zn)  = 1/(1-zn/zf)
	// proj._43=-zn*zf/(zf-zn)
	// so proj._43/proj._33=-zn.

	float zNear=-proj._43/proj._33;
	float zFar=helper->GetMaxCloudDistance()*1.1f;
	proj._33=zFar/(zFar-zNear);
	proj._43=-zNear*zFar/(zFar-zNear);
		
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pCloudEffect->SetMatrix(worldViewProj, &tmp1);
	D3DXMatrixIdentity(&tmp1);
	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	simul::sky::float4 view_km=(const float*)cam_pos;
	simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
	std::swap(wind_offset.y,wind_offset.z);
	helper->Update((const float*)cam_pos,wind_offset,view_dir,up);
	view_km*=0.001f;
	float alt_km=cloudInterface->GetCloudBaseZ()*0.001f;
static float light_mult=.05f;
	simul::sky::float4 light_response(	cloudInterface->GetLightResponse(),
										light_mult*cloudInterface->GetSecondaryLightResponse(),
										0,
										0);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();
//	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	simul::sky::float4 sky_light_colour=skyInterface->GetSkyColour(simul::sky::float4(0,0,1,0),alt_km);
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(alt_km);
	simul::sky::float4 fractal_scales=helper->GetFractalScales(cloudInterface);
	simul::sky::float4 mie_rayleigh_ratio=skyInterface->GetMieRayleighRatio();

	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	helper->Set2DNoiseTexturing(-0.8f,1.f);
	helper->MakeGeometry(cloudInterface);
	helper->CalcInscatterFactors(cloudInterface,skyInterface,fadeTableInterface,0.f);
	float image_scale=1.f/texture_scale;
	// Make the angular inscatter multipliers:
	unsigned el_start,el_end,az_start,az_end;
	helper->GetCurrentGrid(el_start,el_end,az_start,az_end);
static float image_effect=0.5f;
	D3DXVECTOR4 interp_vec(cloudKeyframer->GetInterpolation(),1.f-cloudKeyframer->GetInterpolation(),0,0);
	m_pCloudEffect->SetVector	(interp				,(D3DXVECTOR4*)(&interp_vec));
	m_pCloudEffect->SetVector	(eyePosition		,&cam_pos);
	m_pCloudEffect->SetVector	(lightResponse		,(D3DXVECTOR4*)(&light_response));
	m_pCloudEffect->SetVector	(lightDir			,(D3DXVECTOR4*)(&sun_dir));
	m_pCloudEffect->SetVector	(skylightColour		,(D3DXVECTOR4*)(&sky_light_colour));
	m_pCloudEffect->SetVector	(sunlightColour		,(D3DXVECTOR4*)(&sunlight));
	m_pCloudEffect->SetVector	(fractalScale		,(D3DXVECTOR4*)(&fractal_scales));

	m_pCloudEffect->SetFloat	(layerDensity		,1.f-exp(-cloudInterface->GetCloudHeight()*0.001f*cloudInterface->GetOpticalDensity()));
	m_pCloudEffect->SetFloat	(imageEffect		,image_effect);
	
	m_pCloudEffect->SetVector	(mieRayleighRatio	,(D3DXVECTOR4*)(&mie_rayleigh_ratio));
	m_pCloudEffect->SetFloat	(hazeEccentricity	,skyInterface->GetMieEccentricity());
	m_pCloudEffect->SetFloat	(cloudEccentricity	,cloudInterface->GetMieAsymmetry());	
	int startv=0;
	int v=0;
	hr=m_pd3dDevice->SetVertexDeclaration( m_pVtxDecl );

	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
	// blending for alpha:
	m_pd3dDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
	m_pd3dDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_INVSRCALPHA);

	startv=v;
	simul::math::Vector3 pos;
	simul::sky::float4 loss2,inscatter2;
	int i=0;
	const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
	size_t qs_vert=0;
	for(std::vector<simul::clouds::Cloud2DGeometryHelper::QuadStrip>::const_iterator j=helper->GetQuadStrips().begin();
		j!=helper->GetQuadStrips().end();j++,i++)
	{
		// The distance-fade for these clouds. At distance dist, how much of the cloud's colour is lost?
		simul::sky::float4 loss;
		// And how much sunlight is scattered in from the air in front of the cloud?
		simul::sky::float4 inscatter1;
		
		bool bit=false;

		for(size_t k=0;k<(j)->num_vertices;k++,qs_vert++,v++,bit=!bit)
		{
			Vertex2D_t &vertex=vertices[v];
			const simul::clouds::Cloud2DGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
			
			simul::sky::float4 inscatter;
			pos.Define(V.x,V.y,V.z);
			if(v>=MAX_VERTICES)
				break;
			vertex.position=float3(V.x,V.y,V.z);
			vertex.texCoords=float2(V.cloud_tex_x,V.cloud_tex_y);
			vertex.texCoordNoise=float2(V.noise_tex_x,V.noise_tex_y);
			inscatter=helper->GetInscatter(0,V);
			loss=helper->GetLoss(0,V);
			vertex.loss=simul::sky::float4(loss.x,loss.y,loss.z,0);
			vertex.inscatter=simul::sky::float4(inscatter.x,inscatter.y,inscatter.z,0);
			vertex.imageCoords=float2(vertex.texCoords.x*image_scale,vertex.texCoords.y*image_scale);
		}
		if(v>=MAX_VERTICES)
			break;
		Vertex2D_t &vertex=vertices[v];
		vertex.position=pos;
		v++;
	}
	UINT passes=1;
	hr=m_pCloudEffect->Begin(&passes,0);
	hr=m_pCloudEffect->BeginPass(0);
	if((v-startv)>2)
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,(v-startv)-2,&(vertices[startv]),sizeof(Vertex2D_t));
	hr=m_pCloudEffect->EndPass();
	hr=m_pCloudEffect->End();

	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	PIXEndNamedEvent();
	return hr;
}

void Simul2DCloudRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

void Simul2DCloudRenderer::SetWind(float speed,float heading_degrees)
{
	cloudKeyframer->SetWindSpeed(speed);
	cloudKeyframer->SetWindHeadingDegrees(heading_degrees);
}

void Simul2DCloudRenderer::SetCloudiness(float c)
{
	cloudKeyframer->SetCloudiness(c);
}

simul::clouds::CloudInterface *Simul2DCloudRenderer::GetCloudInterface()
{
	return cloudInterface;
}

const char *Simul2DCloudRenderer::GetDebugText() const
{
	static char debug_text[256];
	simul::math::Vector3 wo=cloudInterface->GetWindOffset();
	sprintf_s(debug_text,256,"interp %2.2g\nnext noise time %2.2g",cloudKeyframer->GetInterpolation(),cloudNode->GetDaytime());
	return debug_text;
}
