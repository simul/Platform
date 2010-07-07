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
#include "Simul/LicenseKey.h"
#include "Macros.h"
#include "Resources.h"
/*
static float lerp(float f,float l,float h)
{
	return l+((h-l)*f);
}
*/
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
float max_dist=240000.f;

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
	virtual float GetHumidityMultiplier(float,float,float z) const
	{
		static float mul=1.f;
		static float cutoff=0.1f;
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
	last_time(0.f),
	timing(0.f),
	detail(0.75f),
	fade_interp(0.f),
	noise_texture_frequency(8),
	texture_octaves(7),
	texture_persistence(0.79f),
	noise_texture_size(1024),
	altitude_tex_coord(0.f),
	precip_strength(1.f),
	cloud_tex_width_x(0),
	cloud_tex_length_y(0),
	cloud_tex_depth_z(0)
{
	lightning_colour.x=1.5f;
	lightning_colour.y=1.5f;
	lightning_colour.z=2.4f;

	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	cloudNode=new simul::clouds::ThunderCloudNode;
	
	cloudNode->SetLightExtend(0.5f);
	AddChild(cloudNode.get());
	cloudNode->SetLicense(SIMUL_LICENSE_KEY);

	cloudNode->SetHumidityCallback(&hum_callback);

	cloudInterface=cloudNode.get();
	cloudInterface->SetWrap(true);

	cloudInterface->SetGridLength(128);
	cloudInterface->SetGridWidth(128);
	cloudInterface->SetGridHeight(16);

	cloudInterface->SetExtinction(.2f);
	cloudInterface->SetAlphaSharpness(0.15f);
	cloudInterface->SetFractalEffectScale(3.f);
	cloudInterface->SetFractalPatternScale(140.f);
	cloudInterface->SetCloudBaseZ(3300.f);
	cloudInterface->SetCloudWidth(60000.f);
	cloudInterface->SetCloudLength(60000.f);
	cloudInterface->SetCloudHeight(6000.f);
	// How many days for the time-based noise signal to repeat:
	cloudInterface->SetNoisePeriod(1.f);

	cloudInterface->SetOpticalDensity(.2f);
	cloudInterface->SetHumidity(0);

	cloudInterface->SetLightResponse(0.75f);
	cloudInterface->SetSecondaryLightResponse(0.75f);
	cloudInterface->SetAmbientLightResponse(1.f);

	cloudInterface->SetNoiseResolution(8);
	cloudInterface->SetNoiseOctaves(3);
	cloudInterface->SetNoisePersistence(.55f);

	cloudNode->SetCacheNoise(true);
	
	cloudInterface->Generate();

	cloudKeyframer=new simul::clouds::CloudKeyframer(cloudInterface);
	AddChild(cloudKeyframer.get());
	cloudKeyframer->SetUserKeyframes(false);
	cloudKeyframer->SetUse16Bit(false);
// 30 game-minutes for interpolation:
	cloudKeyframer->SetInterpStepTime(1.f/24.f/2.f);

	helper=new simul::clouds::CloudGeometryHelper();
	helper->SetYVertical(y_vertical);
	helper->Initialize((unsigned)(160.f*detail),min_dist+(max_dist-min_dist)*detail);

	helper->SetCurvedEarth(true);
	helper->SetAdjustCurvature(false);
	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
	illumination_texel_index[0]=illumination_texel_index[1]=illumination_texel_index[2]=illumination_texel_index[3]=0;

#ifdef DO_SOUND
	sound=new simul::sound::fmod::NodeSound();
	sound->Init("Media/Sound/IntelDemo.fev");
	int ident=sound->GetOrAddSound("rain");
	sound->StartSound(ident,0);
	sound->SetSoundVolume(ident,0.f);
#endif

	// A noise filter improves the shape of the clouds:
	cloudNode->GetNoiseInterface()->SetFilter(&circle_f);

	// Try to use Threading Building Blocks?
	cloudInterface->SetUseTbb(true);
}

void SimulCloudRenderer::SetSkyInterface(simul::sky::BaseSkyInterface *si)
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
	V_RETURN(CreateLightningTexture());
	V_RETURN(CreateIlluminationTexture());
	V_RETURN(CreateDX9Effect(m_pd3dDevice,m_pCloudEffect,"simul_clouds_and_lightning.fx"));

	m_hTechniqueCloud					=GetDX9Technique(m_pCloudEffect,"simul_clouds");
	m_hTechniqueCloudMask				=GetDX9Technique(m_pCloudEffect,"cloud_mask");
	m_hTechniqueCloudsAndLightning		=GetDX9Technique(m_pCloudEffect,"simul_clouds_and_lightning");
	m_hTechniqueCrossSectionXZ			=GetDX9Technique(m_pCloudEffect,"cross_section_xz");
	m_hTechniqueCrossSectionXY			=GetDX9Technique(m_pCloudEffect,"cross_section_xy");

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
	m_hTechniqueLightningLines	=m_pLightningEffect->GetTechniqueByName("simul_lightning_lines");
	m_hTechniqueLightningQuads	=m_pLightningEffect->GetTechniqueByName("simul_lightning_quads");
	l_worldViewProj			=m_pLightningEffect->GetParameterByName(NULL,"worldViewProj");

	// create the unit-sphere vertex buffer determined by the Cloud Geometry Helper:
	
	SAFE_RELEASE(unitSphereVertexBuffer);
	helper->GenerateSphereVertices();
	V_RETURN(m_pd3dDevice->CreateVertexBuffer((unsigned)(helper->GetVertices().size()*sizeof(PosVert_t)),D3DUSAGE_WRITEONLY,0,
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
	V_RETURN(m_pd3dDevice->CreateIndexBuffer(num_indices*sizeof(unsigned),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
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
	cloud_tex_width_x=0;
	cloud_tex_length_y=0;
	cloud_tex_depth_z=0;
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

HRESULT SimulCloudRenderer::RenderNoiseTexture()
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return hr;
	SAFE_RELEASE(noise_texture);
	LPDIRECT3DSURFACE9				pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9				pNoiseRenderTarget=NULL;
	LPD3DXEFFECT					pRenderNoiseEffect=NULL;
	D3DXHANDLE						inputTexture=NULL;
	D3DXHANDLE						octaves=NULL;
	D3DXHANDLE						persistence=NULL;
	LPDIRECT3DTEXTURE9				input_texture=NULL;

	V_RETURN(CreateDX9Effect(m_pd3dDevice,pRenderNoiseEffect,"simul_rendernoise.fx"));
	inputTexture					=pRenderNoiseEffect->GetParameterByName(NULL,"noiseTexture");
	octaves							=pRenderNoiseEffect->GetParameterByName(NULL,"octaves");
	persistence						=pRenderNoiseEffect->GetParameterByName(NULL,"persistence");
	//
	// Make the input texture:
	{
		if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,noise_texture_frequency,noise_texture_frequency,0,default_texture_usage,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&input_texture)))
			return hr;
		D3DLOCKED_RECT lockedRect={0};
		if(FAILED(hr=input_texture->LockRect(0,&lockedRect,NULL,NULL)))
			return hr;
		SetBits8();

		simul::clouds::TextureGenerator::Make2DRandomTexture((unsigned char *)(lockedRect.pBits),noise_texture_frequency);
		hr=input_texture->UnlockRect(0);
		input_texture->GenerateMipSubLevels();
	}

	{
		hr=(m_pd3dDevice->CreateTexture(	noise_texture_size,
											noise_texture_size,
											0,
											D3DUSAGE_RENDERTARGET,
											D3DFMT_A8R8G8B8,
											D3DPOOL_DEFAULT,
											&noise_texture,
											NULL
										));
		noise_texture->GetSurfaceLevel(0,&pNoiseRenderTarget);
		hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);

		hr=m_pd3dDevice->SetRenderTarget(0,pNoiseRenderTarget);
		hr=m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0x0000000,1.f,0L);
	}
	
	pRenderNoiseEffect->SetTexture(inputTexture,input_texture);
	pRenderNoiseEffect->SetFloat(persistence,texture_persistence);

	int size=log((float)noise_texture_size)/log(2.f);
	int freq=log((float)noise_texture_frequency)/log(2.f);
	texture_octaves=size-freq;

	pRenderNoiseEffect->SetInt(octaves,texture_octaves);

	RenderTexture(m_pd3dDevice,0,0,noise_texture_size,noise_texture_size,
					  input_texture,pRenderNoiseEffect,NULL);

	{
		m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	}
	//D3DXSaveTextureToFile(TEXT("Media/Textures/noise.dds"),D3DXIFF_DDS,noise_texture,NULL);
	noise_texture->GenerateMipSubLevels();
D3DXSaveTextureToFile(TEXT("Media/Textures/noise.jpg"),D3DXIFF_JPG,noise_texture,NULL);
	SAFE_RELEASE(pOldRenderTarget);
	SAFE_RELEASE(pRenderNoiseEffect);
	SAFE_RELEASE(input_texture);
	SAFE_RELEASE(pNoiseRenderTarget);
	return hr;
}

HRESULT SimulCloudRenderer::CreateNoiseTexture(bool override_file)
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(noise_texture);

	// Can we load it from disk?
//return RenderNoiseTexture();
	//SAFE_RELEASE(noise_texture);
	if((hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/noise.dds"),&noise_texture))==S_OK)
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

	D3DXSaveTextureToFile(TEXT("Media/Textures/noise.png"),D3DXIFF_PNG,noise_texture,NULL);
	D3DXSaveTextureToFile(TEXT("Media/Textures/noise.dds"),D3DXIFF_DDS,noise_texture,NULL);
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
	cloudNode->SetLightningCentreX(simul::math::Vector3(cam_pos.x,cam_pos.z,cam_pos.y));
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
		t[i]=(float)illumination_texel_index[i]/(float)(max_texels);
		unsigned &index=illumination_texel_index[i];
		
		simul::clouds::TextureGenerator::PartialMake3DLightningTexture(
			cloudNode.get(),i,
			w,l,h,
			X1,X2,
			index,
			texels,
			lightning_cloud_tex_data,
			cloudInterface->GetUseTbb());
		
		if(enable_lightning&&index>=max_texels)
		{
			index=0;
			cloudNode->StartSource(i);
			float x1[3],x2[3],br1,br2;
			//cloudNode->GetSegment(0,0,0,br1,br2,x1,x2);
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
	if(width_x==cloud_tex_width_x&&length_y==cloud_tex_length_y&&depth_z==cloud_tex_depth_z)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
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

void SimulCloudRenderer::Update(float dt)
{
	if(!cloud_textures[2])
		return;
	if(!cloudInterface)
		return;
	static bool no_update=false;
	if(no_update)
		return;
	simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);

	float current_time=skyInterface->GetDaytime();
	float real_dt=0.f;
	if(last_time!=0.f)
		real_dt=3600.f*(current_time-last_time);
	last_time=current_time;
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

HRESULT SimulCloudRenderer::Render(bool cubemap)
{
	HRESULT hr=S_OK;
	if(!noise_texture)
	{
		V_RETURN(CreateNoiseTexture());
	}
static float effect_on_cloud=20.f;
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
	PIXBeginNamedEvent(0, "Render Clouds Layers");
		PIXBeginNamedEvent(0,"Setup");
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
			helper->Update((const float*)cam_pos,wind_offset,view_dir,up,0.f,cubemap);
			view_km*=0.001f;
			float alt_km=0.001f*(cloudInterface->GetCloudBaseZ());//+.5f*cloudInterface->GetCloudHeight());

			static float light_mult=0.03f;
			simul::sky::float4 light_response(	cloudInterface->GetLightResponse(),
												0,
												0,
												light_mult*cloudInterface->GetSecondaryLightResponse());
			simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();

			// calculate sun occlusion for any external classes that need it:
			if(y_vertical)
				std::swap(X.y,X.z);
			float vis=1;//GetVisibility(X.FloatPointer(0),(const float*)sun_dir);
			sun_occlusion=1.f-vis;
			if(y_vertical)
				std::swap(X.y,X.z);

			if(y_vertical)
				std::swap(sun_dir.y,sun_dir.z);
			// Get the overall ambient light at this altitude, and multiply it by the cloud's ambient response.
			simul::sky::float4 sky_light_colour=fadeTableInterface->GetAmbientLight(alt_km)*cloudInterface->GetAmbientLightResponse();
			// Get the sunlight at this altitude:
			simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(alt_km);
			simul::sky::float4 fractal_scales=(cubemap?0.f:1.f)*helper->GetFractalScales(cloudInterface);

			float tan_half_fov_vertical=1.f/proj._22;
			float tan_half_fov_horizontal=1.f/proj._11;
			helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
			helper->MakeGeometry(cloudInterface,enable_lightning);

			float cloud_interp=cloudKeyframer->GetInterpolation();
			m_pCloudEffect->SetFloat	(interp				,cloud_interp);
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
			
			
				simul::math::Vector3 pos;
				int v=0;
				static std::vector<simul::sky::float4> light_colours;
				unsigned grid_x,el_grid;
				helper->GetGrid(el_grid,grid_x);
				light_colours.resize(el_grid+1);
			PIXEndNamedEvent();

				hr=m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl);
				UINT passes=1;
#if 0
				m_pCloudEffect->SetTechnique(m_hTechniqueCloudMask);
				hr=m_pCloudEffect->Begin(&passes,0);

			PIXBeginNamedEvent(0,"Mask");
				hr=m_pCloudEffect->BeginPass(0);
				int max=85;
				for(std::vector<simul::clouds::CloudGeometryHelper::RealtimeSlice*>::const_reverse_iterator i=helper->GetSlices().rbegin();
					i!=helper->GetSlices().rend()&&max!=0;i++)
				{
					max--;
					helper->MakeLayerGeometry(cloudInterface,*i);
					const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
					size_t qs_vert=0;
					float fade=(*i)->fadeIn;
					bool start=true;
					for(std::vector<const simul::clouds::CloudGeometryHelper::QuadStrip*>::const_iterator j=(*i)->quad_strips.begin();
						j!=(*i)->quad_strips.end();j++)
					{
						bool l=0;
						for(size_t k=0;k<(*j)->num_vertices;k++,qs_vert++,l++,v++)
						{
							const simul::clouds::CloudGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
							
							pos.Define(V.x,V.y,V.z);
							simul::math::Vector3 tex_pos(V.cloud_tex_x,V.cloud_tex_y,V.cloud_tex_z);
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
				}
			PIXEndNamedEvent();
			if(v>2)
				hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,v-2,&(vertices[0]),sizeof(Vertex_t));

				hr=m_pCloudEffect->EndPass();
				hr=m_pCloudEffect->End();
#endif

			m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
			m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			// blending for alpha:
			m_pd3dDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
			m_pd3dDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_INVSRCALPHA);

			if(enable_lightning)
				m_pCloudEffect->SetTechnique(m_hTechniqueCloudsAndLightning);
			else
				m_pCloudEffect->SetTechnique(m_hTechniqueCloud);
				hr=m_pCloudEffect->Begin(&passes,0);
				hr=m_pCloudEffect->BeginPass(0);
			PIXBeginNamedEvent(0,"Make Slice Geometry");
				
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
						float alt_km=min(max(0.f,view_km.y+sine*0.001f*(*i)->distance),0.001f*(cloudInterface->GetCloudBaseZ()+cloudInterface->GetCloudHeight()));
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
							vertex.sunlightColour.x=sunlight.x;
							vertex.sunlightColour.y=sunlight.y;
							vertex.sunlightColour.z=sunlight.z;
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
			{
				hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,v-2,&(vertices[0]),sizeof(Vertex_t));
			}
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
		
	//set up matrices
	D3DXMATRIX wvp;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	cam_pos=GetCameraPosVector(view);

    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetPixelShader( NULL );

#ifndef XBOX
	//m_pd3dDevice->SetTransform(D3DTS_VIEW,&view);
	//m_pd3dDevice->SetTransform(D3DTS_WORLD,&world);
	//m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&proj);
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
	m_pLightningEffect->SetMatrix(l_worldViewProj,&wvp);
	m_pLightningEffect->SetTechnique(m_hTechniqueLightningQuads);
	UINT passes=1;
	hr=m_pLightningEffect->Begin(&passes,0);
	hr=m_pLightningEffect->BeginPass(0);
	for(unsigned i=0;i<lri->GetNumLightSources();i++)
	{
		if(!lri->GetSourceStarted(i))
			continue;
		bool quads=true;
		simul::math::Vector3 x1,x2;
		float bright1=0.f,bright2=0.f;
		simul::math::Vector3 camPos(cam_pos);
		lri->GetSegmentVertex(i,0,0,bright1,x1.FloatPointer(0));
		float dist=(x1-camPos).Magnitude();
		float vertical_shift=helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(unsigned jj=0;jj<lri->GetNumBranches(i);jj++)
		{
			if(jj==1)
			{
				hr=m_pLightningEffect->EndPass();
				hr=m_pLightningEffect->End();
				
				m_pLightningEffect->SetTechnique(m_hTechniqueLightningLines);
				quads=false;

				hr=m_pLightningEffect->Begin(&passes,0);
				hr=m_pLightningEffect->BeginPass(0);
			}
			simul::math::Vector3 last_transverse;
			vert_start=vert_num;
			for(unsigned k=0;k<lri->GetNumSegments(i,jj)&&vert_num<4500;k++)
			{
				lri->GetSegmentVertex(i,jj,k,bright1,x1.FloatPointer(0));
			simul::math::Vector3 dir;
				lri->GetSegmentVertexDirection(i,jj,k,dir.FloatPointer(0));

				static float ww=50.f;
				float width=lri->GetBranchWidth(i,jj);
				float width1=bright1*width;
				if(quads)
					width1*=ww;
				simul::math::Vector3 transverse;
				view_dir=x1-simul::math::Vector3(cam_pos.x,cam_pos.z,cam_pos.y);
				CrossProduct(transverse,view_dir,dir);
				transverse.Unit();
				transverse*=width1;
				simul::math::Vector3 t=transverse;
				if(k)
					t=0.5f*(last_transverse+transverse);
				simul::math::Vector3 x1a=x1;//-t;
				if(quads)
					x1a=x1-t;
				simul::math::Vector3 x1b=x1+t;
				if(!k)
					bright1=0;
				if(k==lri->GetNumSegments(i,jj)-1)
					bright1=0;
				PosTexVert_t &v1=lightning_vertices[vert_num++];
				if(y_vertical)
				{
					std::swap(x1a.y,x1a.z);
					std::swap(x1b.y,x1b.z);
				}
				v1.position.x=x1a.x;
				v1.position.y=x1a.y+vertical_shift;
				v1.position.z=x1a.z;
				v1.texCoords.x=width1;
				v1.texCoords.y=0;

				if(quads)
				{
					PosTexVert_t &v2=lightning_vertices[vert_num++];
					v2.position.x=x1b.x;
					v2.position.y=x1b.y+vertical_shift;
					v2.position.z=x1b.z;
					v2.texCoords.x=1.f;
					v2.texCoords.x=width1;
					v2.texCoords.y=1.f;
				}
				last_transverse=transverse;
			}
			if(vert_num-vert_start>2)
			{
				if(quads)
				{
					hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,vert_num-vert_start-2,
					&(lightning_vertices[vert_start]),
					sizeof(PosTexVert_t));
				}
				else
				{
					hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINESTRIP,vert_num-vert_start-2,
					&(lightning_vertices[vert_start]),
					sizeof(PosTexVert_t));
				}
			}
			
		}
	}
	hr=m_pLightningEffect->EndPass();
	hr=m_pLightningEffect->End();
	PIXEndNamedEvent();
	return hr;
}
#ifdef XBOX
void SimulCloudRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
#endif

void SimulCloudRenderer::SetWind(float speed,float heading_degrees)
{
	cloudKeyframer->SetWindSpeed(speed);
	cloudKeyframer->SetWindHeadingDegrees(heading_degrees);
}

simul::clouds::CloudInterface *SimulCloudRenderer::GetCloudInterface()
{
	return cloudInterface;
}

simul::clouds::CloudKeyframer *SimulCloudRenderer::GetCloudKeyframer()
{
	return cloudKeyframer.get();
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
	float p=cloudKeyframer->GetPrecipitation();
	float below=((cloudInterface->GetCloudBaseZ()-y_vertical?cam_pos.y:cam_pos.z)*cloudInterface->GetGridHeight()/cloudInterface->GetCloudHeight())+1.f;
	below=min(below,1.f);
	below=max(below,0.f);
	simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
	if(y_vertical)
		std::swap(X.y,X.z);
	simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
	X+=wind_offset;
	X.z=cloudInterface->GetCloudBaseZ()+cloudInterface->GetCloudHeight()/2.f;
	float dens=cloudInterface->GetCloudDensity(X);
	return p*precip_strength*below*dens;
}

void SimulCloudRenderer::SetStepsPerHour(unsigned steps)
{
	cloudKeyframer->SetInterpStepTime(1.f/(24.f*(float)steps));
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
	sprintf_s(debug_text,256,"%2.2g %2.2g %2.2g",
		cloudKeyframer->GetTime0(),cloudKeyframer->GetTime1(),cloudKeyframer->GetTime2());
	return debug_text;
}

void SimulCloudRenderer::SetCloudiness(float h)
{
	cloudKeyframer->SetCloudiness(h);
}

void SimulCloudRenderer::SetEnableStorms(bool s)
{
	enable_lightning=s;
	simul::clouds::LightningRenderInterface *lri=cloudNode.get();
	lri->SetLightningEnabled(s);
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
		helper->Initialize((unsigned)(160.f*detail),min_dist+(max_dist-min_dist)*detail);
	}
}

HRESULT SimulCloudRenderer::RenderCrossSections(int width)
{
	int w=(width-16)/3;

	int h=(cloudInterface->GetGridHeight()*w)/cloudInterface->GetGridWidth();
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
			dynamic_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(
					skyInterface->GetDaytime())+i));

		D3DXVECTOR4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		m_pCloudEffect->SetVector	(lightResponse		,(D3DXVECTOR4*)(&light_response));
			m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[i]);
		RenderTexture(m_pd3dDevice,i*w+8,4,w-2,h,
					  cloud_textures[i],m_pCloudEffect,m_hTechniqueCrossSectionXZ);
		
	}
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
			dynamic_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(
					skyInterface->GetDaytime())+i));

		D3DXVECTOR4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		m_pCloudEffect->SetVector	(lightResponse		,(D3DXVECTOR4*)(&light_response));
			m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[i]);
		RenderTexture(m_pd3dDevice,i*130+8,h+8,128,128,
					  cloud_textures[i],m_pCloudEffect,m_hTechniqueCrossSectionXY);
		
	}
	return S_OK;
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
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST,helper->GetSlices().size(),lines,(unsigned)sizeof(Vertext));
	delete [] lines;
	return S_OK;
}

simul::sky::OvercastCallback *SimulCloudRenderer::GetOvercastCallback()
{
	return cloudKeyframer.get();
}

void SimulCloudRenderer::SetLossTextures(LPDIRECT3DBASETEXTURE9 t1,LPDIRECT3DBASETEXTURE9 t2)
{
	sky_loss_texture_1=t1;
	sky_loss_texture_2=t2;
}

void SimulCloudRenderer::SetInscatterTextures(LPDIRECT3DBASETEXTURE9 t1,LPDIRECT3DBASETEXTURE9 t2)
{
	sky_inscatter_texture_1=t1;
	sky_inscatter_texture_2=t2;
}

void SimulCloudRenderer::SetNoiseTextureProperties(int size,int freq,int octaves,float persistence)
{
	if(	noise_texture_size==size&&
		noise_texture_frequency==freq&&
		texture_octaves==octaves&&
		texture_persistence==persistence)
		return;
	noise_texture_size=size;
	noise_texture_frequency=freq;
	texture_octaves=octaves;
	texture_persistence=persistence;
	CreateNoiseTexture(true);
}

// Save and load a sky sequence
std::ostream &SimulCloudRenderer::Save(std::ostream &os) const
{
	return cloudKeyframer->Save(os);
}

std::istream &SimulCloudRenderer::Load(std::istream &is) const
{
	return cloudKeyframer->Load(is);
}

void SimulCloudRenderer::New()
{
	cloudKeyframer->New();
}