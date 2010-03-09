// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulCloudRenderer.cpp A renderer for 3d clouds.

#include "SimulCloudRenderer.h"
#ifdef DO_SOUND
#include "Simul/Sound/FMOD/NodeSound.h"
#endif
#include "Simul/Base/Timer.h"
#include <fstream>
#include <math.h>

#ifdef XBOX
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
	D3DFORMAT cloud_tex_format=D3DFMT_LIN_A4R4G4B4;
	D3DFORMAT illumination_tex_format=D3DFMT_LIN_A8R8G8B8;
	const bool big_endian=true;
	static DWORD default_texture_usage=0;
	// NOTE: We specify ONE mipmap for this texture, NOT ZERO. If we use zero, that means
	// automatically generate mipmaps.
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
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
	D3DFORMAT cloud_tex_format=D3DFMT_A8R8G8B8;
	D3DFORMAT illumination_tex_format=D3DFMT_A8R8G8B8;
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
float thunder_volume=0.f;
#include "CreateDX9Effect.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/ThunderCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Samples/LicenseKey.h"
#include "Macros.h"
#include "Resources.h"

static float lerp(float f,float l,float h)
{
	return l+((h-l)*f);
}

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#ifndef V_RETURN
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }
#endif

typedef std::basic_string<TCHAR> tstring;
simul::math::Vector3 wind_vector(100,0,100);
simul::math::Vector3 next_sun_direction(0,.7f,.7f);
struct float2
{
	float x,y;
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
	}
};
struct float3
{
	float x,y,z;
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
		z=f[2];
	}
};
struct Vertex_t
{
    float3 position;
    float3 texCoords;
    float layerFade;
    float2 texCoordsNoise;
	float3 sunlightColour;
};
struct PosTexVert_t
{
    float3 position;	
    float2 texCoords;
};
struct PosVert_t
{
    float3 position;
};
Vertex_t *vertices=NULL;
PosTexVert_t *lightning_vertices=NULL;
#define MAX_VERTICES (12000)


float min_dist=60000.f;
float max_dist=320000.f;

static void SetBits4()
{
	simul::clouds::TextureGenerator::SetBits(bits[0],bits[1],bits[2],bits[3],2,big_endian);
}
static void SetBits8()
{
	simul::clouds::TextureGenerator::SetBits(bits8[0],bits8[1],bits8[2],bits8[3],(unsigned)4,big_endian);
}

// This is an example of a noise filter:
class CircleFilter:public simul::math::NoiseFilter
	{
	public:
		float Filter(float val) const
		{
			val=1.f-val;
			val=sqrt(1.f-val*val);
			val-=0.866f;
			val*=0.5f/0.134f;
			val+=0.5f;
			if(val<0)
				val=0;
			if(val>1.f)
				val=1.f;
			return val;
		}
	};
CircleFilter circle_f;

class ExampleHumidityCallback:public simul::clouds::HumidityCallbackInterface
{
public:
	virtual float GetHumidityMultiplier(float x,float y,float z) const
	{
		static float mul=0.95f;
		static float cutoff=0.125f;
		if(z<cutoff)
			return 1.f;
		return mul;
	}
};
ExampleHumidityCallback hum_callback;

SimulCloudRenderer::SimulCloudRenderer() :
	skyInterface(NULL),
	fadeTableInterface(NULL),
	cloudInterface(NULL),
	m_pd3dDevice(NULL),
	m_pVtxDecl(NULL),
	m_pLightningVtxDecl(NULL),
	m_pHudVertexDecl(NULL),
	m_pCloudEffect(NULL),
	m_pLightningEffect(NULL),
	noise_texture(NULL),
	lightning_texture(NULL),
	illumination_texture(NULL),
	sky_loss_texture_1(NULL),
	sky_loss_texture_2(NULL),
	sky_inscatter_texture_1(NULL),
	sky_inscatter_texture_2(NULL),
	unitSphereVertexBuffer(NULL),
	unitSphereIndexBuffer(NULL),
	y_vertical(true),
	enable_lightning(false),
	sun_occlusion(0.f),
#ifdef DO_SOUND
	sound(NULL),
#endif
	lightning_active(false),
	last_time(0.f),
	timing(0.f),
	detail(0.5f),
	fade_interp(0.f),
	noise_texture_frequency(8),
	texture_octaves(7),
	texture_persistence(0.79f),
	noise_texture_size(1024),
	altitude_tex_coord(0.f)
{
	lightning_colour.x=0.75f;
	lightning_colour.y=0.75f;
	lightning_colour.z=1.2f;

	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	cloudNode=new simul::clouds::ThunderCloudNode;
	cloudNode->SetLicense(SIMUL_LICENSE_KEY);

	cloudNode->SetHumidityCallback(&hum_callback);

	cloudInterface=cloudNode.get();
	cloudInterface->SetWrap(true);

	cloudInterface->SetGridLength(128);
	cloudInterface->SetGridWidth(128);
	cloudInterface->SetGridHeight(16);

	cloudInterface->SetExtinction(1.8f);
	cloudInterface->SetAlphaSharpness(0.5f);
	cloudInterface->SetFractalEffectScale(5.f);
	cloudInterface->SetFractalPatternScale(140.f);
	cloudInterface->SetCloudBaseZ(3300.f);
	cloudInterface->SetCloudWidth(60000.f);
	cloudInterface->SetCloudLength(60000.f);
	cloudInterface->SetCloudHeight(10000.f);
	cloudInterface->SetNoisePeriod(2500000);

	cloudInterface->SetOpticalDensity(2.4f);
	cloudInterface->SetHumidity(0);

	cloudInterface->SetLightResponse(0.75f);
	cloudInterface->SetSecondaryLightResponse(0.75f);
	cloudInterface->SetAmbientLightResponse(1.f);
	cloudInterface->SetAnisotropicLightResponse(3.f);

	cloudInterface->SetNoiseResolution(4);
	cloudInterface->SetNoiseOctaves(4);
	cloudInterface->SetNoisePersistence(.6f);

	cloudNode->SetCacheNoise(true);
	
	cloudInterface->Generate();

	cloudKeyframer=new simul::clouds::CloudKeyframer(cloudInterface);
	cloudKeyframer->SetUse16Bit(false);
	// 30 game-minutes for interpolation:
	cloudKeyframer->SetInterpStepTime(1.f/24.f/2.f);

	helper=new simul::clouds::CloudGeometryHelper();
	helper->SetYVertical(y_vertical);
	helper->Initialize((unsigned)(120.f*detail),min_dist+(max_dist-min_dist)*detail);

	helper->SetCurvedEarth(true);
	
	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
	texel_index[0]=texel_index[1]=texel_index[2]=texel_index[3]=0;

#ifdef DO_SOUND
	sound=new simul::sound::fmod::NodeSound();
	sound->Init("Media/Sound/IntelDemo.fev");
	int ident=sound->GetOrAddSound("rain");
	sound->StartSound(ident,0);
	sound->SetSoundVolume(ident,0.f);
#endif

	// A noise filter improves the shape of the clouds:
	cloudNode->GetNoiseInterface()->SetFilter(&circle_f);
}

void SimulCloudRenderer::SetSkyInterface(simul::sky::SkyInterface *si)
{
	skyInterface=si;
	cloudKeyframer->SetSkyInterface(si);
}
void SimulCloudRenderer::EnableFilter(bool f)
{
	if(f)
		cloudNode->GetNoiseInterface()->SetFilter(&circle_f);
	else
		cloudNode->GetNoiseInterface()->SetFilter(NULL);
}

void SimulCloudRenderer::SetFadeTableInterface(simul::sky::FadeTableInterface *fti)
{
	fadeTableInterface=fti;
}


HRESULT SimulCloudRenderer::RestoreDeviceObjects( LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr;
	D3DVERTEXELEMENT9 decl[]=
	{
		{0, 0	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0, 12	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0,	24	,D3DDECLTYPE_FLOAT1,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0,	28	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		{0,	36	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,3},
		D3DDECL_END()
	};
	D3DVERTEXELEMENT9 std_decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 12, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);
	SAFE_RELEASE(m_pHudVertexDecl);
	V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl))
	V_RETURN(m_pd3dDevice->CreateVertexDeclaration(std_decl,&m_pLightningVtxDecl))
	V_RETURN(CreateNoiseTexture());
	V_RETURN(CreateLightningTexture());
	V_RETURN(CreateIlluminationTexture());
	V_RETURN(CreateCloudEffect());

	m_hTechniqueCloud					=GetDX9Technique(m_pCloudEffect,"simul_clouds");
	m_hTechniqueCloudsAndLightning		=GetDX9Technique(m_pCloudEffect,"simul_clouds_and_lightning");

	worldViewProj			=m_pCloudEffect->GetParameterByName(NULL,"worldViewProj");
	eyePosition				=m_pCloudEffect->GetParameterByName(NULL,"eyePosition");
	lightResponse			=m_pCloudEffect->GetParameterByName(NULL,"lightResponse");
	lightDir				=m_pCloudEffect->GetParameterByName(NULL,"lightDir");
	skylightColour			=m_pCloudEffect->GetParameterByName(NULL,"skylightColour");
	sunlightColour			=m_pCloudEffect->GetParameterByName(NULL,"sunlightColour");
	fractalScale			=m_pCloudEffect->GetParameterByName(NULL,"fractalScale");
	interp					=m_pCloudEffect->GetParameterByName(NULL,"interp");
	large_texcoords_scale	=m_pCloudEffect->GetParameterByName(NULL,"large_texcoords_scale");
	mieRayleighRatio		=m_pCloudEffect->GetParameterByName(NULL,"mieRayleighRatio");
	hazeEccentricity		=m_pCloudEffect->GetParameterByName(NULL,"hazeEccentricity");
	cloudEccentricity		=m_pCloudEffect->GetParameterByName(NULL,"cloudEccentricity");
	fadeInterp				=m_pCloudEffect->GetParameterByName(NULL,"fadeInterp");
	distance				=m_pCloudEffect->GetParameterByName(NULL,"distance");
	cornerPos				=m_pCloudEffect->GetParameterByName(NULL,"cornerPos");
	texScales				=m_pCloudEffect->GetParameterByName(NULL,"texScales");
	layerFade				=m_pCloudEffect->GetParameterByName(NULL,"layerFade");
	altitudeTexCoord		=m_pCloudEffect->GetParameterByName(NULL,"altitudeTexCoord");
	alphaSharpness			=m_pCloudEffect->GetParameterByName(NULL,"alphaSharpness");
	//if(enable_lightning)
	{
		lightningMultipliers=m_pCloudEffect->GetParameterByName(NULL,"lightningMultipliers");
		lightningColour		=m_pCloudEffect->GetParameterByName(NULL,"lightningColour");
		illuminationOrigin	=m_pCloudEffect->GetParameterByName(NULL,"illuminationOrigin");
		illuminationScales	=m_pCloudEffect->GetParameterByName(NULL,"illuminationScales");
	}

	cloudDensity1			=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity1");
	cloudDensity2			=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity2");
	noiseTexture			=m_pCloudEffect->GetParameterByName(NULL,"noiseTexture");
	largeScaleCloudTexture	=m_pCloudEffect->GetParameterByName(NULL,"largeScaleCloudTexture");
	lightningIlluminationTexture=m_pCloudEffect->GetParameterByName(NULL,"lightningIlluminationTexture");
	skyLossTexture1			=m_pCloudEffect->GetParameterByName(NULL,"skyLossTexture1");
	skyLossTexture2			=m_pCloudEffect->GetParameterByName(NULL,"skyLossTexture2");
	skyInscatterTexture1	=m_pCloudEffect->GetParameterByName(NULL,"skyInscatterTexture1");
	skyInscatterTexture2	=m_pCloudEffect->GetParameterByName(NULL,"skyInscatterTexture2");

	V_RETURN(CreateDX9Effect(m_pd3dDevice,m_pLightningEffect,"simul_lightning.fx"));
	m_hTechniqueLightning	=m_pLightningEffect->GetTechniqueByName("simul_lightning");
	l_worldViewProj			=m_pLightningEffect->GetParameterByName(NULL,"worldViewProj");

	// create the unit-sphere vertex buffer determined by the Cloud Geometry Helper:
	
	SAFE_RELEASE(unitSphereVertexBuffer);
	helper->GenerateSphereVertices();
	V_RETURN(m_pd3dDevice->CreateVertexBuffer((unsigned)(helper->GetVertices().size()*sizeof(PosVert_t)),0,0,
									  D3DPOOL_DEFAULT, &unitSphereVertexBuffer,
									  NULL));
	PosVert_t *unit_sphere_vertices;
	V_RETURN(unitSphereVertexBuffer->Lock(0,sizeof(PosVert_t),(void**)&unit_sphere_vertices,0 ));
	PosVert_t *V=unit_sphere_vertices;
	for(size_t i=0;i<helper->GetVertices().size();i++)
	{
		const simul::clouds::CloudGeometryHelper::Vertex &v=helper->GetVertices()[i];
		V->position.x=v.x;
		V->position.y=v.y;
		V->position.z=v.z;
		V++;
	}
	V_RETURN(unitSphereVertexBuffer->Unlock());
	unsigned num_indices=(unsigned)helper->GetQuadStripIndices().size();
	SAFE_RELEASE(unitSphereIndexBuffer);
	V_RETURN(m_pd3dDevice->CreateIndexBuffer(num_indices*sizeof(unsigned),0,D3DFMT_INDEX32,
		D3DPOOL_DEFAULT, &unitSphereIndexBuffer, NULL ));
	unsigned *indexData;
	V_RETURN(unitSphereIndexBuffer->Lock(0,num_indices, (void**)&indexData, 0 ));

	unsigned *index=indexData;
	for(unsigned i=0;i<num_indices;i++)
	{
		*(index++)	=helper->GetQuadStripIndices()[i];
	}
	V_RETURN(unitSphereIndexBuffer->Unlock());

	// NOW can set the rendercallback, as we have a device to implement the callback fns with:
	cloudKeyframer->SetRenderCallback(this);
	cloudKeyframer->Init();
	return hr;
}

HRESULT SimulCloudRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pCloudEffect)
        hr=m_pCloudEffect->OnLostDevice();
	if(m_pLightningEffect)
        hr=m_pLightningEffect->OnLostDevice();
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);
	SAFE_RELEASE(m_pHudVertexDecl);
	SAFE_RELEASE(m_pCloudEffect);
	SAFE_RELEASE(m_pLightningEffect);
	for(int i=0;i<3;i++)
		SAFE_RELEASE(cloud_textures[i]);
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(lightning_texture);
	SAFE_RELEASE(illumination_texture);
	SAFE_RELEASE(unitSphereVertexBuffer);
	SAFE_RELEASE(unitSphereIndexBuffer);
	//SAFE_RELEASE(large_scale_cloud_texture);
	return hr;
}

HRESULT SimulCloudRenderer::Destroy()
{
	HRESULT hr=InvalidateDeviceObjects();
#ifdef DO_SOUND
	delete sound;
	sound=NULL;
#endif
	return hr;
}

SimulCloudRenderer::~SimulCloudRenderer()
{
	Destroy();
}

HRESULT SimulCloudRenderer::CreateNoiseTexture()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(noise_texture);

	// Can we load it from disk?
	if((hr=D3DXCreateTextureFromFile(m_pd3dDevice,L"Media/Textures/noise.dds",&noise_texture))==S_OK)
		return hr;

	// Otherwise create it:
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,noise_texture_size,noise_texture_size,0,default_texture_usage,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&noise_texture)))
		return hr;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=noise_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return hr;
	SetBits8();

	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)(lockedRect.pBits),
		noise_texture_size,noise_texture_frequency,texture_octaves,texture_persistence);
	hr=noise_texture->UnlockRect(0);
	noise_texture->GenerateMipSubLevels();
	return hr;
}

HRESULT SimulCloudRenderer::CreateLightningTexture()
{
	HRESULT hr=S_OK;
	unsigned size=64;
	SAFE_RELEASE(lightning_texture);
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,1,default_mip_levels,default_texture_usage,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&lightning_texture)))
		return hr;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=lightning_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return hr;
	unsigned char *lightning_tex_data=(unsigned char *)(lockedRect.pBits);
	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float level=.5f*linear*linear+5.f*(linear>.97f);
		float r=lightning_colour.x	*level;
		float g=lightning_colour.y *level;
		float b=lightning_colour.z	*level;
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
	hr=lightning_texture->UnlockRect(0);
	return hr;
}

HRESULT SimulCloudRenderer::CreateIlluminationTexture()
{
	unsigned w=64;
	unsigned l=64;
	unsigned h=8;
	SAFE_RELEASE(illumination_texture);
	HRESULT hr=D3DXCreateVolumeTexture(m_pd3dDevice,w,l,h,1,0,illumination_tex_format,default_d3d_pool,&illumination_texture);
	D3DLOCKED_BOX lockedBox={0};
	if(FAILED(hr=illumination_texture->LockBox(0,&lockedBox,NULL,NULL)))
		return hr;
	memset(lockedBox.pBits,0,4*w*l*h);
	hr=illumination_texture->UnlockBox(0);
	return hr;
}
float t[4];
float u[4];
HRESULT SimulCloudRenderer::UpdateIlluminationTexture(float dt)
{
	HRESULT hr=S_OK;
	// RGBA bit-shift is 12,8,4,0
	simul::clouds::TextureGenerator::SetBits((unsigned)255<<16,(unsigned)255<<8,(unsigned)255<<0,(unsigned)255<<24,4,false);

	unsigned w=64;
	unsigned l=64;
	unsigned h=8;
	simul::math::Vector3 X1,X2,DX;
	DX.Define(cloudNode->GetLightningZoneSize(),
		cloudNode->GetLightningZoneSize(),
		cloudNode->GetCloudBaseZ()+cloudNode->GetCloudHeight());
	X1=cloudNode->GetLightningCentreX()-0.5f*DX;
	X2=cloudNode->GetLightningCentreX()+0.5f*DX;
	X1.z=0;
	unsigned max_texels=w*l*h;
	D3DLOCKED_BOX lockedBox={0};
	if(FAILED(hr=illumination_texture->LockBox(0,&lockedBox,NULL,NULL)))
		return hr;
	unsigned char *lightning_cloud_tex_data=(unsigned char *)(lockedBox.pBits);
	// lightning rate : strikes per second
	static float rr=1.7f;
	float lightning_rate=rr;
#ifdef THREADED_VERSION
//	if(cloudInterface->GetUseTbb())
		lightning_rate*=4.f;
#else
	//if(cloudInterface->GetUseTbb())
		lightning_rate*=4.f;
#endif
	unsigned texels=(unsigned)(lightning_rate*dt/4.f*(float)max_texels);
	
	for(unsigned i=0;i<4;i++)
	{
		//thunder_volume[i]*=0.95f;
		if(cloudNode->GetSourceStarted(i))
		{
			u[i]=cloudNode->GetLightSourceProgress(i);
			if(u[i]<0.5f)
			{
				static float ff=0.05f;
				thunder_volume+=ff;
			}
			continue;
		}
		else
			u[i]=0;
		if(!cloudNode->CanStartSource(i))
			continue;
		t[i]=(float)texel_index[i]/(float)(max_texels);
		unsigned &index=texel_index[i];
		
		simul::clouds::TextureGenerator::PartialMake3DLightningTexture(
			cloudNode.get(),i,
			w,l,h,
			X1,X2,
			index,
			texels,
			lightning_cloud_tex_data,
			cloudInterface->GetUseTbb());
		
		if(lightning_active&&index>=max_texels)
		{
			index=0;
			cloudNode->StartSource(i);
			float x1[3],x2[3],br1,br2;
			cloudNode->GetSegment(0,0,0,br1,br2,x1,x2);
#ifdef DO_SOUND
			char sound_name[20];
			sprintf(sound_name,"thunderclap%d",i);
			int ident=sound->GetOrAddSound(sound_name);
			sound->StartSound(ident,x1);
#endif
		}
	}
	hr=illumination_texture->UnlockBox(0);
	return hr;
}

void SimulCloudRenderer::SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	if(!width_x||!length_y||!depth_z)
		return;
	HRESULT hr=S_OK;
	V_CHECK(CanUseTexFormat(m_pd3dDevice,cloud_tex_format));
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		if(FAILED(hr=D3DXCreateVolumeTexture(m_pd3dDevice,width_x,length_y,depth_z,1,0,cloud_tex_format,default_d3d_pool,&cloud_textures[i])))
			return;
	}
}
void SimulCloudRenderer::FillCloudTexture(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array)
{
	if(!num_texels)
		return;
	D3DLOCKED_BOX lockedBox={0};
	if(FAILED(cloud_textures[texture_index]->LockBox(0,&lockedBox,NULL,NULL)))
		return;
	unsigned *ptr=(unsigned *)(lockedBox.pBits);
	ptr+=texel_index;
	memcpy(ptr,uint32_array,num_texels*sizeof(unsigned));
	cloud_textures[texture_index]->UnlockBox(0);
}
void SimulCloudRenderer::CycleTexturesForward()
{
	std::swap(cloud_textures[0],cloud_textures[1]);
	std::swap(cloud_textures[1],cloud_textures[2]);
}
HRESULT SimulCloudRenderer::CreateCloudEffect()
{
	return CreateDX9Effect(m_pd3dDevice,m_pCloudEffect,"simul_clouds_and_lightning.fx");
}

void SimulCloudRenderer::Update(float dt)
{
	if(!cloud_textures[2])
		return;
	if(!cloudInterface)
		return;
	simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);

	float current_time=skyInterface->GetDaytime();
	float real_dt=0.f;
	if(last_time!=0.f)
		real_dt=3600.f*(current_time-last_time);
	last_time=current_time;
	simul::math::AddFloatTimesVector(wind_offset,real_dt,wind_vector);
	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);
	cloudInterface->SetWindOffset(wind_offset);
	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);

static simul::base::Timer timer;
timer.StartTime();
	cloudKeyframer->Update(current_time);
timer.FinishTime();
timing=timer.Time;
	simul::graph::meta::TimeStepVisitor tsv;
	tsv.SetTimeStep(dt);
	cloudNode->Accept(tsv);
	if(enable_lightning)
		UpdateIlluminationTexture(dt);
#ifdef DO_SOUND
	int ident=sound->GetOrAddSound("rain");
	sound->SetSoundVolume(ident,precipitation);
	sound->Update(NULL,dt);
#endif
}

void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj)
{
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(wvp,&tmp2);
}

D3DXVECTOR4 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR4 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}

bool SimulCloudRenderer::IsCameraAboveCloudBase() const
{
	if(y_vertical)
		return(cam_pos.y>cloudInterface->GetCloudBaseZ());
	else
		return(cam_pos.z>cloudInterface->GetCloudBaseZ());
}

static const D3DXVECTOR4 *MakeD3DVector(const simul::sky::float4 v)
{
	static D3DXVECTOR4 x;
	x.x=v.x;
	x.y=v.y;
	x.z=v.z;
	x.w=v.w;
	return &x;
}

// This method is experimental and deprecated at the moment, as it is slower.
HRESULT SimulCloudRenderer::RenderNewMethod()
{
	PIXBeginNamedEvent(0, "Render Clouds Layers");
		PIXBeginNamedEvent(0,"Setup");
			HRESULT hr=S_OK;
			static D3DTEXTUREADDRESS wrap_u=D3DTADDRESS_WRAP,wrap_v=D3DTADDRESS_WRAP,wrap_w=D3DTADDRESS_CLAMP;

			PIXBeginNamedEvent(0,"Set Textures");
				m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[0]);
				m_pCloudEffect->SetTexture(cloudDensity2				,cloud_textures[1]);
				m_pCloudEffect->SetTexture(noiseTexture					,noise_texture);
				m_pCloudEffect->SetTexture(lightningIlluminationTexture	,illumination_texture);

				m_pCloudEffect->SetTexture(skyLossTexture1				,sky_loss_texture_1);
				m_pCloudEffect->SetTexture(skyLossTexture2				,sky_loss_texture_2);
				m_pCloudEffect->SetTexture(skyInscatterTexture1			,sky_inscatter_texture_1);
				m_pCloudEffect->SetTexture(skyInscatterTexture2			,sky_inscatter_texture_2);
			PIXEndNamedEvent();

		//	if(enable_lightning)
		//		m_pCloudEffect->SetTechnique(m_hTechniqueCloudsAndLightning);
		//	else
				m_pCloudEffect->SetTechnique(m_hTechniqueCloud);

			PIXBeginNamedEvent(0,"Set Properties");
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
			D3DXMATRIX wvp;
			cam_pos=GetCameraPosVector(view);
			D3DXMatrixTranslation(&world,cam_pos.x,cam_pos.y,cam_pos.z);
			MakeWorldViewProjMatrix(&wvp,world,view,proj);
			simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
			simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
			if(y_vertical)
				std::swap(wind_offset.y,wind_offset.z);
			X+=wind_offset;
			m_pCloudEffect->SetMatrix(worldViewProj, &wvp);
			simul::math::Vector3 view_dir	(view._13,view._23,view._33);
			simul::math::Vector3 up			(view._12,view._22,view._32);
			simul::sky::float4 view_km=(const float*)cam_pos;
			helper->Update((const float*)cam_pos,wind_offset,view_dir,up);
			view_km*=0.001f;
			float alt_km=0.001f*(cloudInterface->GetCloudBaseZ()+.5f*cloudInterface->GetCloudHeight());
			static float light_mult=1.f;
			simul::sky::float4 light_response(	0,
												0,
												0,
												light_mult*cloudInterface->GetSecondaryLightResponse());
			simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();
			// calculate sun occlusion for any external classes that need it:
			if(y_vertical)
				std::swap(X.y,X.z);
			float len=cloudInterface->GetOpticalPathLength(X.FloatPointer(0),(const float*)sun_dir);
			float vis=min(1.f,max(0.f,exp(-0.001f*cloudInterface->GetOpticalDensity()*len)));
			sun_occlusion=1.f-vis;
			if(y_vertical)
				std::swap(X.y,X.z);
			if(y_vertical)
				std::swap(sun_dir.y,sun_dir.z);
			simul::sky::float4 sky_light_colour=skyInterface->GetSkyColour(simul::sky::float4(0,0,1,0),alt_km);
			simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(alt_km)/25.f;
			simul::sky::float4 fractal_scales=helper->GetFractalScales(cloudInterface);
			float tan_half_fov_vertical=1.f/proj._22;
			float tan_half_fov_horizontal=1.f/proj._11;
			helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
			helper->MakeGeometry(cloudInterface,enable_lightning);
			float cloud_interp=cloudKeyframer->GetInterpolation();
			D3DXVECTOR4 interp_vec(cloud_interp,1.f-cloud_interp,0,0);
			m_pCloudEffect->SetVector	(interp				,(D3DXVECTOR4*)(&interp_vec));
			m_pCloudEffect->SetVector	(eyePosition		,&cam_pos);
			m_pCloudEffect->SetVector	(lightResponse		,(D3DXVECTOR4*)(&light_response));
			m_pCloudEffect->SetVector	(lightDir			,(D3DXVECTOR4*)(&sun_dir));
			m_pCloudEffect->SetVector	(skylightColour		,(D3DXVECTOR4*)(&sky_light_colour));
			m_pCloudEffect->SetVector	(sunlightColour		,(D3DXVECTOR4*)(&sunlight));
			m_pCloudEffect->SetVector	(fractalScale		,(D3DXVECTOR4*)(&fractal_scales));
			m_pCloudEffect->SetVector	(mieRayleighRatio	,MakeD3DVector(skyInterface->GetMieRayleighRatio()));
			m_pCloudEffect->SetFloat	(hazeEccentricity	,skyInterface->GetMieEccentricity());
			m_pCloudEffect->SetFloat	(cloudEccentricity	,cloudInterface->GetMieAsymmetry());			
			m_pCloudEffect->SetFloat	(fadeInterp			,fade_interp);
			if(enable_lightning)
			{
				static float bb=.1f;
				simul::sky::float4 lightning_multipliers;
				for(unsigned i=0;i<4;i++)
				{
					if(i<cloudNode->GetNumLightSources())
						lightning_multipliers[i]=bb*cloudNode->GetLightSourceBrightness(i);
					else lightning_multipliers[i]=0;
				}
				static float effect_on_cloud=20.f;
				//static float lm=2.f;
				//simul::sky::float4 lightning_colour(lm*lightning_red,lm*lightning_green,lm*lightning_blue,25.f);
				lightning_colour.w=effect_on_cloud;
				m_pCloudEffect->SetVector	(lightningMultipliers	,(D3DXVECTOR4*)(&lightning_multipliers));
				m_pCloudEffect->SetVector	(lightningColour		,&lightning_colour);

				simul::math::Vector3 light_X1,light_X2,light_DX;
				light_DX.Define(cloudNode->GetLightningZoneSize(),
					cloudNode->GetLightningZoneSize(),
					cloudNode->GetCloudBaseZ()+cloudNode->GetCloudHeight());

				light_X1=cloudNode->GetLightningCentreX();
				light_X1-=0.5f*light_DX;
				light_X1.z=0;
				light_X2=cloudNode->GetLightningCentreX();
				light_X2+=0.5f*light_DX;
				light_X2.z=light_DX.z;

				m_pCloudEffect->SetVector	(illuminationOrigin		,(D3DXVECTOR4*)(&light_X1));
				m_pCloudEffect->SetVector	(illuminationScales		,(D3DXVECTOR4*)(&light_DX));
			}
			PIXEndNamedEvent();
			PIXBeginNamedEvent(0,"Prepare");
				hr=m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl);

				V_RETURN(m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
				V_RETURN(m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE));
				V_RETURN(m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
				// blending for alpha:
				V_RETURN(m_pd3dDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO));
				V_RETURN(m_pd3dDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_INVSRCALPHA));
			PIXEndNamedEvent();

			simul::math::Vector3 X1=cloudInterface->GetOrigin();
			simul::math::Vector3 InverseDX=cloudInterface->GetInverseScales();
			m_pCloudEffect->SetVector	(cornerPos		,(D3DXVECTOR4*)(&X1));
			m_pCloudEffect->SetVector	(texScales		,(D3DXVECTOR4*)(&InverseDX));
			PIXBeginNamedEvent(0,"Make Slice Geometry");
				simul::math::Vector3 pos;
				m_pd3dDevice->SetStreamSource(0,unitSphereVertexBuffer,0,
					sizeof(PosVert_t));
				V_RETURN(hr=m_pd3dDevice->SetIndices(unitSphereIndexBuffer));
				UINT passes=1;
				hr=m_pCloudEffect->Begin(&passes,0);
				for(std::vector<simul::clouds::CloudGeometryHelper::RealtimeSlice*>::const_iterator i=helper->GetSlices().begin();
					i!=helper->GetSlices().end();i++)
				{
					m_pCloudEffect->SetFloat(distance	,(*i)->distance);
					m_pCloudEffect->SetFloat(layerFade	,(*i)->fadeIn);
					unsigned num_indices=(*i)->index_end-(*i)->index_start;
					int BaseVertexIndex=0;
					int MinIndex=(*i)->min_index;
					int NumVertices=(*i)->max_index-(*i)->min_index+1;
					int StartIndex=(*i)->index_start;
					int PrimitiveCount=num_indices-2;
			// for each slice draw the sphere with a different radius.
					PIXBeginNamedEvent(0,"Draw Sphere");
						hr=m_pCloudEffect->BeginPass(0);
						hr=m_pd3dDevice->DrawIndexedPrimitive(	D3DPT_TRIANGLESTRIP,
																BaseVertexIndex,
																MinIndex,
																NumVertices,
																StartIndex,
																PrimitiveCount	);
						hr=m_pCloudEffect->EndPass();
					PIXEndNamedEvent();
				}
				hr=m_pCloudEffect->End();
			PIXEndNamedEvent();
		PIXEndNamedEvent();

		m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	PIXEndNamedEvent();
	return hr;
}

HRESULT SimulCloudRenderer::Render()
{
	PIXBeginNamedEvent(0, "Render Clouds Layers");
		PIXBeginNamedEvent(0,"Setup");
			HRESULT hr=S_OK;
			if(!vertices)
				vertices=new Vertex_t[MAX_VERTICES];
			static D3DTEXTUREADDRESS wrap_u=D3DTADDRESS_WRAP,wrap_v=D3DTADDRESS_WRAP,wrap_w=D3DTADDRESS_CLAMP;

			PIXBeginNamedEvent(0,"Set Textures");
			m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[0]);
			m_pCloudEffect->SetTexture(cloudDensity2				,cloud_textures[1]);
			m_pCloudEffect->SetTexture(noiseTexture					,noise_texture);
			m_pCloudEffect->SetTexture(lightningIlluminationTexture	,illumination_texture);

			m_pCloudEffect->SetTexture(skyLossTexture1				,sky_loss_texture_1);
			m_pCloudEffect->SetTexture(skyLossTexture2				,sky_loss_texture_2);
			m_pCloudEffect->SetTexture(skyInscatterTexture1			,sky_inscatter_texture_1);
			m_pCloudEffect->SetTexture(skyInscatterTexture2			,sky_inscatter_texture_2);
			PIXEndNamedEvent();

			if(enable_lightning)
				m_pCloudEffect->SetTechnique(m_hTechniqueCloudsAndLightning);
			else
				m_pCloudEffect->SetTechnique(m_hTechniqueCloud);

			PIXBeginNamedEvent(0,"Set Properties");
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
			D3DXMATRIX wvp;
			MakeWorldViewProjMatrix(&wvp,world,view,proj);
			cam_pos=GetCameraPosVector(view);
			simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
			simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
			if(y_vertical)
				std::swap(wind_offset.y,wind_offset.z);
			X+=wind_offset;


			m_pCloudEffect->SetMatrix(worldViewProj, &wvp);

			simul::math::Vector3 view_dir	(view._13,view._23,view._33);
			simul::math::Vector3 up			(view._12,view._22,view._32);

			simul::sky::float4 view_km=(const float*)cam_pos;
			helper->Update((const float*)cam_pos,wind_offset,view_dir,up);
			view_km*=0.001f;
			float alt_km=0.001f*(cloudInterface->GetCloudBaseZ()+.5f*cloudInterface->GetCloudHeight());

			static float light_mult=0.05f;
			simul::sky::float4 light_response(	cloudInterface->GetLightResponse(),
												0,
												0,
												light_mult*cloudInterface->GetSecondaryLightResponse());
			simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();

			// calculate sun occlusion for any external classes that need it:
			if(y_vertical)
				std::swap(X.y,X.z);
			float len=cloudInterface->GetOpticalPathLength(X.FloatPointer(0),(const float*)sun_dir);
			float vis=min(1.f,max(0.f,exp(-0.001f*cloudInterface->GetOpticalDensity()*len)));
			sun_occlusion=1.f-vis;
			if(y_vertical)
				std::swap(X.y,X.z);

			if(y_vertical)
				std::swap(sun_dir.y,sun_dir.z);
			simul::sky::float4 sky_light_colour=skyInterface->GetSkyColour(simul::sky::float4(0,0,1,0),alt_km);
			simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(alt_km);
			simul::sky::float4 fractal_scales=helper->GetFractalScales(cloudInterface);

			float tan_half_fov_vertical=1.f/proj._22;
			float tan_half_fov_horizontal=1.f/proj._11;
			helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
			helper->MakeGeometry(cloudInterface,enable_lightning);

			float cloud_interp=cloudKeyframer->GetInterpolation();
			D3DXVECTOR4 interp_vec(cloud_interp,1.f-cloud_interp,0,0);
			m_pCloudEffect->SetVector	(interp				,(D3DXVECTOR4*)(&interp_vec));
			m_pCloudEffect->SetVector	(eyePosition		,&cam_pos);
			m_pCloudEffect->SetVector	(lightResponse		,(D3DXVECTOR4*)(&light_response));
			m_pCloudEffect->SetVector	(lightDir			,(D3DXVECTOR4*)(&sun_dir));
			m_pCloudEffect->SetVector	(skylightColour		,(D3DXVECTOR4*)(&sky_light_colour));
			m_pCloudEffect->SetVector	(sunlightColour		,(D3DXVECTOR4*)(&sunlight));
			m_pCloudEffect->SetVector	(fractalScale		,(D3DXVECTOR4*)(&fractal_scales));
			m_pCloudEffect->SetVector	(mieRayleighRatio	,MakeD3DVector(skyInterface->GetMieRayleighRatio()));
			
			m_pCloudEffect->SetFloat	(hazeEccentricity	,skyInterface->GetMieEccentricity());
			m_pCloudEffect->SetFloat	(cloudEccentricity	,cloudInterface->GetMieAsymmetry());
			m_pCloudEffect->SetFloat	(fadeInterp			,fade_interp);
			m_pCloudEffect->SetFloat	(altitudeTexCoord	,altitude_tex_coord);
			m_pCloudEffect->SetFloat	(alphaSharpness		,cloudInterface->GetAlphaSharpness());

			if(enable_lightning)
			{
				static float bb=.1f;
				simul::sky::float4 lightning_multipliers;
				for(unsigned i=0;i<4;i++)
				{
					if(i<cloudNode->GetNumLightSources())
						lightning_multipliers[i]=bb*cloudNode->GetLightSourceBrightness(i);
					else lightning_multipliers[i]=0;
				}
				static float effect_on_cloud=20.f;
				//static float lm=2.f;
				//simul::sky::float4 lightning_colour(lm*lightning_red,lm*lightning_green,lm*lightning_blue,25.f);
				lightning_colour.w=effect_on_cloud;
				m_pCloudEffect->SetVector	(lightningMultipliers	,(D3DXVECTOR4*)(&lightning_multipliers));
				m_pCloudEffect->SetVector	(lightningColour		,&lightning_colour);

				simul::math::Vector3 light_X1,light_X2,light_DX;
				light_DX.Define(cloudNode->GetLightningZoneSize(),
					cloudNode->GetLightningZoneSize(),
					cloudNode->GetCloudBaseZ()+cloudNode->GetCloudHeight());

				light_X1=cloudNode->GetLightningCentreX();
				light_X1-=0.5f*light_DX;
				light_X1.z=0;
				light_X2=cloudNode->GetLightningCentreX();
				light_X2+=0.5f*light_DX;
				light_X2.z=light_DX.z;

				m_pCloudEffect->SetVector	(illuminationOrigin		,(D3DXVECTOR4*)(&light_X1));
				m_pCloudEffect->SetVector	(illuminationScales		,(D3DXVECTOR4*)(&light_DX));
			}
			PIXEndNamedEvent();
			PIXBeginNamedEvent(0,"Prepare");
				hr=m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl);
				UINT passes=1;
				hr=m_pCloudEffect->Begin(&passes,0);
				hr=m_pCloudEffect->BeginPass(0);

				m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
				m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
				// blending for alpha:
				m_pd3dDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
				m_pd3dDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_INVSRCALPHA);
			PIXEndNamedEvent();
			
			int v=0;
					static std::vector<simul::sky::float4> light_colours;
					unsigned grid_x,el_grid;
					helper->GetGrid(el_grid,grid_x);
					light_colours.resize(el_grid+1);

			PIXBeginNamedEvent(0,"Make Slice Geometry");
				
				simul::math::Vector3 pos;
				for(std::vector<simul::clouds::CloudGeometryHelper::RealtimeSlice*>::const_iterator i=helper->GetSlices().begin();
					i!=helper->GetSlices().end();i++)
				{
					PIXBeginNamedEvent(0,"MakeLayerGeometry");
						helper->MakeLayerGeometry(cloudInterface,*i);
					PIXEndNamedEvent();
					PIXBeginNamedEvent(0,"Build Quad Strip");
					const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
					size_t qs_vert=0;
					float fade=(*i)->fadeIn;
					bool start=true;
					if((*i)->quad_strips.size())
					for(int j=(*i)->elev_start;j<=(*i)->elev_end;j++)
					{
						float j_interp=(float)j/(float)el_grid;
						float sine=(2.f*j_interp-1.f);
						float alt_km=min(max(0.f,view_km.y+sine*0.001f*(*i)->distance),skyInterface->GetAtmosphereThickness());
						light_colours[j]=skyInterface->GetLocalIrradiance(alt_km);
					}
					for(std::vector<const simul::clouds::CloudGeometryHelper::QuadStrip*>::const_iterator j=(*i)->quad_strips.begin();
						j!=(*i)->quad_strips.end();j++)
					{
						bool l=0;
						for(size_t k=0;k<(*j)->num_vertices;k++,qs_vert++,l++,v++)
						{
							const simul::clouds::CloudGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
							
							pos.Define(V.x,V.y,V.z);
							simul::math::Vector3 tex_pos(V.cloud_tex_x,V.cloud_tex_y,V.cloud_tex_z);
							if(v>=MAX_VERTICES)
							{
								PIXEndNamedEvent();
								break;
							}
							if(start)
							{
								Vertex_t &startvertex=vertices[v];
								startvertex.position=pos;
								v++;
								start=false;
							}
							Vertex_t &vertex=vertices[v];
							vertex.position=pos;
							vertex.texCoords=tex_pos;
							vertex.texCoordsNoise.x=V.noise_tex_x;
							vertex.texCoordsNoise.y=V.noise_tex_y;
							vertex.layerFade=fade;
							const simul::sky::float4 &light_c=light_colours[V.grid_y];
							vertex.sunlightColour.x=light_c.x;
							vertex.sunlightColour.y=light_c.y;
							vertex.sunlightColour.z=light_c.z;
						}
					}
					if(v>=MAX_VERTICES)
					{
						PIXEndNamedEvent();
						break;
					}
					Vertex_t &vertex=vertices[v];
					vertex.position=pos;
					v++;
					PIXEndNamedEvent();
				}
			PIXEndNamedEvent();
		PIXEndNamedEvent();
		PIXBeginNamedEvent(0,"Draw");
			if(v>2)
				hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,v-2,&(vertices[0]),sizeof(Vertex_t));
		PIXEndNamedEvent();
		hr=m_pCloudEffect->EndPass();
		hr=m_pCloudEffect->End();

		m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	PIXEndNamedEvent();
	return hr;
}

HRESULT SimulCloudRenderer::RenderLightning()
{
	if(!enable_lightning)
		return S_OK;
	using namespace simul::clouds;
	LightningRenderInterface *lri=cloudNode.get();

	if(!lightning_vertices)
		lightning_vertices=new PosTexVert_t[4500];

	HRESULT hr=S_OK;

	PIXBeginNamedEvent(0, "Render Lightning");
	m_pd3dDevice->SetTexture(0,lightning_texture);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1);

	//m_pCloudEffect->SetTechnique( m_hTechniqueLightning );
		
	//set up matrices
	D3DXMATRIX wvp;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	cam_pos=GetCameraPosVector(view);

    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetPixelShader( NULL );

#ifndef XBOX
	m_pd3dDevice->SetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&world);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&proj);
#endif

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	hr=m_pd3dDevice->SetVertexDeclaration( m_pLightningVtxDecl );

	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

	simul::math::Vector3 pos;

	static float lm=10.f;
	static float main_bright=1.f;
	int vert_start=0;
	int vert_num=0;
	m_pLightningEffect->SetTechnique( m_hTechniqueLightning );
	m_pLightningEffect->SetMatrix(l_worldViewProj, &wvp);
	UINT passes=1;
	hr=m_pLightningEffect->Begin(&passes,0);
	hr=m_pLightningEffect->BeginPass(0);
	for(unsigned i=0;i<lri->GetNumLightSources();i++)
	{
		if(!lri->GetSourceStarted(i))
			continue;
		simul::math::Vector3 x1,x2;
		float bright1=0.f,bright2=0.f;
		simul::math::Vector3 camPos(cam_pos);
		lri->GetSegment(i,0,0,bright1,bright2,x1.FloatPointer(0),x2.FloatPointer(0));
		float dist=(x1-camPos).Magnitude();
		float vertical_shift=helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(unsigned jj=0;jj<lri->GetNumBranches(i);jj++)
		{
			simul::math::Vector3 last_transverse;
			vert_start=vert_num;
			for(unsigned k=0;k<lri->GetNumSegments(i,jj)&&vert_num<4500;k++)
			{
				lri->GetSegment(i,jj,k,bright1,bright2,x1.FloatPointer(0),x2.FloatPointer(0));

				static float ww=700.f;
				float width=ww*lri->GetBranchWidth(i,jj);
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
				if(k==lri->GetNumSegments(i,jj)-1)
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
				PosTexVert_t &v2=lightning_vertices[vert_num++];
				v2.position.x=x1b.x;
				v2.position.y=x1b.y+vertical_shift;
				v2.position.z=x1b.z;
				v2.texCoords.x=1.f;
				last_transverse=transverse;
			}
			if(vert_num-vert_start>2)
				hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,vert_num-vert_start-2,&(lightning_vertices[vert_start]),sizeof(PosTexVert_t));
		}
	}
	hr=m_pLightningEffect->EndPass();
	hr=m_pLightningEffect->End();
	PIXEndNamedEvent();
	return hr;
}

void SimulCloudRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

void SimulCloudRenderer::SetWindVelocity(float x,float y)
{
	wind_vector.Define(x,0,y);
}

simul::clouds::CloudInterface *SimulCloudRenderer::GetCloudInterface()
{
	return cloudInterface;
}


simul::clouds::LightningRenderInterface *SimulCloudRenderer::GetLightningRenderInterface()
{
	simul::clouds::LightningRenderInterface *lri=cloudNode.get();
	return lri;
}

float SimulCloudRenderer::GetInterpolation() const
{
	return cloudKeyframer->GetInterpolation();
}

float SimulCloudRenderer::GetOvercastFactor() const
{
	return cloudKeyframer->GetOvercastFactor();
}

float SimulCloudRenderer::GetPrecipitationIntensity() const
{
	return cloudKeyframer->GetPrecipitation();
}

float SimulCloudRenderer::GetSunOcclusion() const
{
	return sun_occlusion;
}

HRESULT SimulCloudRenderer::MakeCubemap()
{
	HRESULT hr=S_OK;
	D3DSURFACE_DESC desc;
	cloud_cubemap->GetLevelDesc(0,&desc);
	//HANDLE *handle=NULL;
	//calc proj matrix
	D3DXMATRIX faceViewMatrix, faceProjMatrix;
	float w,h;
	//vertical only specified
	h = 1 / tanf((3.14159f * 0.5f) * 0.5f);
	w = h ;
	float zFar=helper->GetMaxCloudDistance();
	float zNear=1.f;
	float Q = zFar / (zFar - zNear);

	// build projection matrix
	memset(&faceProjMatrix, 0, sizeof(D3DXMATRIX));
	faceProjMatrix.m[0][0] = w;
	faceProjMatrix.m[1][1] = h;
	faceProjMatrix.m[2][2] = Q;
	faceProjMatrix.m[2][3] = 1;
	faceProjMatrix.m[3][2] = -Q * zNear;

	LPDIRECT3DSURFACE9	pRenderTarget;
	LPDIRECT3DSURFACE9	pOldRenderTarget;
	LPDIRECT3DSURFACE9	pOldDepthSurface;
	hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);
	hr=m_pd3dDevice->GetDepthStencilSurface(&pOldDepthSurface);

	for(int face=0;face<6;face++)
	{
		hr=m_pd3dDevice->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&pRenderTarget,NULL);
		m_pd3dDevice->SetRenderTarget(0, pRenderTarget);
		float3	lookDir={0,0,0};
		float3	upDir={0,0,0};
		float3	side={0,0,0};
		switch(face)
		{
			case D3DCUBEMAP_FACE_POSITIVE_X:
				lookDir.x	= 1;
				lookDir.y	= 0;
				lookDir.z	= 0;
				upDir.x		= 0;
				upDir.y		= 1;
				upDir.z		= 0;
				side.x		= 0;
				side.y		= 0;
				side.z		= -1;
				break;
			case D3DCUBEMAP_FACE_NEGATIVE_X:
				lookDir.x	= -1;
				lookDir.y	= 0;
				lookDir.z	= 0;
				upDir.x		= 0;
				upDir.y		= 1;
				upDir.z		= 0;
				side.x		= 0;
				side.y		= 0;
				side.z		= 1;
				break;
			case D3DCUBEMAP_FACE_POSITIVE_Y:
				lookDir.x	= 0;
				lookDir.y	= 1;
				lookDir.z	= 0;
				upDir.x		= 0;
				upDir.y		= 0;
				upDir.z		= -1;
				side.x		= 1;
				side.y		= 0;
				side.z		= 0;
				break;
			case D3DCUBEMAP_FACE_NEGATIVE_Y:
				lookDir.x	= 0;
				lookDir.y	= -1;
				lookDir.z	= 0;
				upDir.x		= 0;
				upDir.y		= 0;
				upDir.z		= 1;
				side.x		= 1;
				side.y		= 0;
				side.z		= 0;
				break;
			case D3DCUBEMAP_FACE_POSITIVE_Z:
				lookDir.x	= 0;
				lookDir.y	= 0;
				lookDir.z	= 1;
				upDir.x		= 0;
				upDir.y		= 1;
				upDir.z		= 0;
				side.x		= 1;
				side.y		= 0;
				side.z		= 0;
				break;
			case D3DCUBEMAP_FACE_NEGATIVE_Z:
				lookDir.x	= 0;
				lookDir.y	= 0;
				lookDir.z	= -1;
				upDir.x		= 0;
				upDir.y		= 1;
				upDir.z		= 0;
				side.x		= -1;
				side.y		= 0;
				side.z		= 0;
				break;
			default:
				break;
		}

		faceViewMatrix.m[0][0]=side.x;
		faceViewMatrix.m[1][0]=side.y;
		faceViewMatrix.m[2][0]=side.z;

		faceViewMatrix.m[0][1]=upDir.x;
		faceViewMatrix.m[1][1]=upDir.y;
		faceViewMatrix.m[2][1]=upDir.z;

		faceViewMatrix.m[0][2]=lookDir.x;
		faceViewMatrix.m[1][2]=lookDir.y;
		faceViewMatrix.m[2][2]=lookDir.z;

		faceViewMatrix.m[3][0]=0.0f;
		faceViewMatrix.m[3][1]=0.0f;
		faceViewMatrix.m[3][2]=0.0f;

		faceViewMatrix.m[0][3]=0;
		faceViewMatrix.m[1][3]=0;
		faceViewMatrix.m[2][3]=0;
		faceViewMatrix.m[3][3]=1;

		Render();
#ifdef XBOX
		m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, cloud_cubemap, NULL, 0, face, NULL, 0.0f, 0, NULL);
#endif
		m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
		m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

		pRenderTarget->Release();
	}
	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	if(pOldRenderTarget)
		pOldRenderTarget->Release();

	//m_pd3dDevice->SetDepthStencilSurface(pOldDepthSurface);
	if(pOldDepthSurface)
		pOldDepthSurface->Release();
	return hr;
}

const char *SimulCloudRenderer::GetDebugText() const
{
	static char debug_text[256];
	simul::math::Vector3 wo=cloudInterface->GetWindOffset();
	sprintf_s(debug_text,256,"\n%2.2g %2.2g %2.2g %2.2g\n%2.2g %2.2g %2.2g %2.2g",t[0],t[1],t[2],t[3],u[0],u[1],u[2],u[3]);
	return debug_text;
}

void SimulCloudRenderer::SetCloudiness(float h)
{
	cloudKeyframer->SetCloudiness(h);
}

void SimulCloudRenderer::SetEnableStorms(bool s)
{
	enable_lightning=s;
}

float SimulCloudRenderer::GetTiming() const
{
	return timing;
}

LPDIRECT3DVOLUMETEXTURE9 *SimulCloudRenderer::GetCloudTextures()
{
	return cloud_textures;
}

const float *SimulCloudRenderer::GetCloudScales() const
{
	static float s[3];
	s[0]=1.f/cloudInterface->GetCloudWidth();
	s[1]=1.f/cloudInterface->GetCloudLength();
	s[2]=1.f/cloudInterface->GetCloudHeight();
	return s;
}

const float *SimulCloudRenderer::GetCloudOffset() const
{
	static simul::math::Vector3 offset,X1,X2;
	offset=cloudInterface->GetWindOffset();
	cloudInterface->GetExtents(X1,X2);
	offset+=X1;
	return offset.FloatPointer(0);
} 

void SimulCloudRenderer::SetDetail(float d)
{
	if(d!=detail)
	{
		detail=d;
		helper->Initialize((unsigned)(120.f*detail),min_dist+(max_dist-min_dist)*detail);
	}
}

HRESULT SimulCloudRenderer::RenderDistances()
{
	HRESULT hr=S_OK;
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0 },
		{ 0, 32, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
		D3DDECL_END()
	};
	if(!m_pHudVertexDecl)
	{
		V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pHudVertexDecl));
	}

	struct Vertext
	{
		float x,y,z,h;
		float r,g,b,a;
		float tx,ty;
	};
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

    m_pd3dDevice->SetVertexShader(NULL);
    m_pd3dDevice->SetPixelShader(NULL);
	m_pd3dDevice->SetVertexDeclaration(m_pHudVertexDecl);

#ifndef XBOX
	m_pd3dDevice->SetTransform(D3DTS_VIEW,&ident);
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&ident);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&ident);
#endif
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );

	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_CONSTANT);

// Set the constant to 0.25 alpha (0x40 = 64 = 64/256 = 0.25)
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_CONSTANT, 0x80000080);

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
	m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

	m_pd3dDevice->SetTexture(0,NULL);
	
	Vertext *lines=new Vertext[2*helper->GetSlices().size()];
	int j=0;
	for(std::vector<simul::clouds::CloudGeometryHelper::RealtimeSlice*>::const_iterator i=helper->GetSlices().begin();
					i!=helper->GetSlices().end();i++,j++)
	{
		if(!(*i))
			continue;
		float d=(*i)->distance*0.004f;
		lines[j].x=120+d; 
		lines[j].y=300;  
		lines[j].z=0;
		lines[j].r=1.f;
		lines[j].g=1.f;
		lines[j].b=0.f;
		lines[j].a=(*i)->fadeIn;
		j++;
		lines[j].x=120+d; 
		lines[j].y=340; 
		lines[j].z=0;
		lines[j].r=1.f;
		lines[j].g=1.f;
		lines[j].b=0.f;
		lines[j].a=(*i)->fadeIn;
	}
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST,helper->GetSlices().size(),lines,sizeof(Vertext));
	delete [] lines;
	return S_OK;
}