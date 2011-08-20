// Copyright (c) 2007-2011 Simul Software Ltd
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


Simul2DCloudRenderer::Simul2DCloudRenderer(const char *license_key) :
	BaseCloudRenderer(license_key,false),
	m_pd3dDevice(NULL),
	m_pVtxDecl(NULL),
	m_pCloudEffect(NULL),
	noise_texture(NULL),
	image_texture(NULL),
	own_image_texture(true),
	texture_scale(0.25f),
	enabled(true)
	,y_vertical(true)
{
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	simul::base::SmartPtr<simul::base::Referenced> test;
	test=new simul::base::Referenced;
	test=NULL;

	cloudNode->SetLicense(license_key);
	cloudInterface=cloudNode.get();
	
	cloudInterface->SetWrap(true);
	cloudInterface->SetThinLayer(true);

	cloudInterface->SetGridLength(128);
	cloudInterface->SetGridWidth(128);
	cloudInterface->SetGridHeight(2);

	cloudInterface->SetCloudBaseZ(12000.f);

	static float ff=12000.f;
	cloudInterface->SetCloudWidth(ff);
	cloudInterface->SetCloudLength(ff);
	cloudInterface->SetCloudHeight(1200.f);

	cloudInterface->SetFractalAmplitude(2.f);
	cloudInterface->SetFractalWavelength(100.f);

	cloudInterface->SetOpticalDensity(1.5f);
	cloudInterface->SetHumidity(.65f);

	cloudInterface->SetExtinction(1.9f);
	cloudInterface->SetLightResponse(0.5f);
	cloudInterface->SetSecondaryLightResponse(0.5f);
	cloudInterface->SetAmbientLightResponse(0.5f);

	cloudInterface->SetNoiseResolution(8);
	cloudInterface->SetNoiseOctaves(5);
	cloudInterface->SetNoisePersistence(0.75f);
	cloudInterface->SetNoisePeriod(1);

	cloudInterface->SetSelfShadowScale(0.1f);
	
	cloudInterface->SetDiffusivity(.5f);


//	cloudKeyframer=new simul::clouds::CloudKeyframer(cloudInterface,true);
	cloudKeyframer->SetMake2DTextures(true);
	cloudKeyframer->SetUse16Bit(false);
	cloudKeyframer->InitKeyframesFromClouds();

	helper=new simul::clouds::Cloud2DGeometryHelper();
	helper->SetYVertical(true);
	static float max_distance=500000.f;
	helper->Initialize(8,max_distance);
	helper->SetGrid(6,9);
	helper->SetCurvedEarth(true);
	
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
	simul::sky::float4 loss;
    simul::sky::float4 inscatter;
    float2 texCoordNoise;
	float2 imageCoords;
};
Vertex2D_t *vertices=NULL;

void Simul2DCloudRenderer::ReloadShaders()
{
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

	cloudDensity1		=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity1");
	cloudDensity2		=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity2");
	noiseTexture		=m_pCloudEffect->GetParameterByName(NULL,"noiseTexture");
	imageTexture		=m_pCloudEffect->GetParameterByName(NULL,"imageTexture");
}

bool Simul2DCloudRenderer::RestoreDeviceObjects(void *dev)
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
	B_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl))
	B_RETURN(CreateNoiseTexture());
	hr=CreateImageTexture();
	ReloadShaders();
	// NOW can set the rendercallback, as we have a device to implement the callback fns with:
	cloudKeyframer->SetRenderCallback(this);
	return (hr==S_OK);
}

bool Simul2DCloudRenderer::InvalidateDeviceObjects()
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
	return (hr==S_OK);
}

Simul2DCloudRenderer::~Simul2DCloudRenderer()
{
	InvalidateDeviceObjects();
}

bool Simul2DCloudRenderer::CreateNoiseTexture(bool override_file)
{
	SAFE_RELEASE(noise_texture);
	// Can we load it from disk?
	HRESULT hr=S_OK;
	if(!override_file)
		if((hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/noise.dds"),&noise_texture))==S_OK)
			return false;
	// Otherwise create it:
	int size=512;
	// NOTE: We specify ONE mipmap for this texture, NOT ZERO. If we use zero, that means
	// automatically generate mipmaps.
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,size,default_mip_levels,default_texture_usage,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&noise_texture)))
		return false;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=noise_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return false;
	SetBits8();
	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)(lockedRect.pBits),size,16,8,0.8f);
	hr=noise_texture->UnlockRect(0);
	//noise_texture->GenerateMipSubLevels();
	return true;
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

void Simul2DCloudRenderer::SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
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

void Simul2DCloudRenderer::FillCloudTextureSequentially(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array)
{
	HRESULT hr=S_OK;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=cloud_textures[texture_index]->LockRect(0,&lockedRect,NULL,NULL)))
		return;
	unsigned char *ptr=(unsigned char *)(lockedRect.pBits);
	ptr+=texel_index*sizeof(unsigned);
	//unsigned char *src=(unsigned char *)uint32_array;
	//for(int i=0;i<num_texels*2;i++)
	//	*ptr++=(*src++);
	memcpy(ptr,uint32_array,num_texels*sizeof(unsigned));
	hr=cloud_textures[texture_index]->UnlockRect(0);
}

void Simul2DCloudRenderer::CycleTexturesForward()
{
	std::swap(cloud_textures[0],cloud_textures[1]);
	std::swap(cloud_textures[1],cloud_textures[2]);
}

void SetTexture()
{
}

bool Simul2DCloudRenderer::Render(bool cubemap,bool depth_testing,bool default_fog)
{
	cubemap;
	depth_testing;
	default_fog;
	HRESULT hr=S_OK;
	if(!enabled)
		return (hr==S_OK);
	// Disable any in-texture gamma-correction that might be lingering from some other bit of rendering:
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(1, D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(2, D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(3, D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(4, D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(5, D3DSAMP_SRGBTEXTURE,0);
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	if(!vertices)
		vertices=new Vertex2D_t[MAX_VERTICES];

	m_pCloudEffect->SetTechnique( m_hTechniqueCloud );

	m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[0]);
	m_pCloudEffect->SetTexture(cloudDensity2				,cloud_textures[1]);
	m_pCloudEffect->SetTexture(noiseTexture					,noise_texture);
	m_pCloudEffect->SetTexture(imageTexture					,image_texture);

	// Mess with the proj matrix to extend the far clipping plane:
	 FixProjectionMatrix(proj,helper->GetMaxCloudDistance()*1.1f,IsYVertical());
		
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
static float light_mult=.03f;
	simul::sky::float4 light_response(	cloudInterface->GetLightResponse(),
										light_mult*cloudInterface->GetSecondaryLightResponse(),
										0,
										0);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();
//	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	simul::sky::float4 sky_light_colour=skyInterface->GetAmbientLight(alt_km);

	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(alt_km);
	simul::sky::float4 fractal_scales=helper->GetFractalScales(cloudInterface);
	simul::sky::float4 mie_rayleigh_ratio=skyInterface->GetMieRayleighRatio();

	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	static float sc=7.f;
	helper->Set2DNoiseTexturing(-0.8f,1.f,1.f);
	helper->MakeGeometry(cloudInterface);
	helper->CalcInscatterFactors(cloudInterface,skyInterface,0.f);
	float image_scale=1.f/texture_scale;
	// Make the angular inscatter multipliers:
	unsigned el_start,el_end,az_start,az_end;
	helper->GetCurrentGrid(el_start,el_end,az_start,az_end);
	static float image_effect=0.9f;
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
			vertex.texCoords=float2(sc*V.cloud_tex_x,sc*V.cloud_tex_y);
			vertex.texCoordNoise=float2(V.noise_tex_x,V.noise_tex_y);
			inscatter=helper->GetInscatter(V);
			loss=helper->GetLoss(V);
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
	return (hr==S_OK);
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

void Simul2DCloudRenderer::SetCloudiness(float c)
{
	simul::clouds::CloudKeyframer::Keyframe *K=cloudKeyframer->GetNextModifiableKeyframe();
	if(K)
		K->cloudiness=c;
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

simul::clouds::CloudInterface *Simul2DCloudRenderer::GetCloudInterface()
{
	return cloudInterface;
}

simul::clouds::CloudKeyframer *Simul2DCloudRenderer::GetCloudKeyframer()
{
	return cloudKeyframer.get();
}

void Simul2DCloudRenderer::Enable(bool val)
{
	enabled=val;
}

const char *Simul2DCloudRenderer::GetDebugText() const
{
	static char debug_text[256];
	simul::math::Vector3 wo=cloudInterface->GetWindOffset();
	sprintf_s(debug_text,256,"interp %2.2g\nnext noise time %2.2g",cloudKeyframer->GetInterpolation(),cloudNode->GetTime());
	return debug_text;
}

bool Simul2DCloudRenderer::RenderCrossSections(int screen_width)
{
	int w=(screen_width-16)/6;
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
	B_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl));
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
	return (hr==S_OK);
}
