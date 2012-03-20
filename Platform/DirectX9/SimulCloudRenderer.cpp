// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulCloudRenderer.cpp A renderer for 3d clouds.

#include "SimulCloudRenderer.h"
#include "Simul/Base/Timer.h"
// The framebuffer is used for writing 3D clouds to 2D textures for saving.
#include "Framebuffer.h"
#include "SaveTexture.h"
#include "Simul/Base/StringToWString.h"
#include <fstream>
#include <iomanip>
#include <math.h>

#ifdef XBOX
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif
#ifdef XBOX
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
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

#include "CreateDX9Effect.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/ThunderCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Macros.h"
#include "Resources.h"


#define MAX_VERTICES (12000)

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
			static float range=0.134f;
			val=1.f-val;
			val=sqrt(1.f-val*val);
			val-=(1.f-range);
			val*=0.5f/range;
			val+=0.5f;
			if(val<0)
				val=0;
			if(val>1.f)
				val=1.f;
			return val;
		}
	};
CircleFilter circle_f;
#if 1
class ExampleHumidityCallback:public simul::clouds::HumidityCallbackInterface
{
public:
	virtual float GetHumidityMultiplier(float ,float ,float z) const
	{
		static float base_layer=0.46f;
		static float transition=0.2f;
		static float mul=0.5f;
		static float default_=1.f;
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

class CumulonimbusHumidityCallback:public simul::clouds::HumidityCallbackInterface
{
public:
	virtual float GetHumidityMultiplier(float x,float y,float z) const
	{
		static float base_layer=0.125f;
		static float anvil_radius=0.6f;

		float val=1.f;
#if 1
		float R=0.5f;
#if 1
		if(z>base_layer)
			R*=anvil_radius*z;
#endif
		float dx=x-0.5f;
		float dy=y-0.5f;
		float dr=sqrt(dx*dx+dy*dy);
		if(dr>0.7f*R)
			val=(1.f-dr/R)/0.3f;
		else if(dr>R)
			val=0;
#endif
		static float mul=1.f;
		static float cutoff=0.1f;
		if(z<cutoff)
			return val;
		return mul*val;
	}
};

class MushroomHumidityCallback:public simul::clouds::HumidityCallbackInterface
{
public:
	virtual float GetHumidityMultiplier(float x,float y,float z) const
	{
		float val=0;
		float dx=x-0.5f;
		float dy=y-0.5f;
		float r=2.f*sqrt(dx*dx+dy*dy);

		static float column_radius=0.4f;
		static float column_height=0.7f;
		static float doughnut_radius=0.7f;
		static float doughnut_minor_radius=0.3f;
		static float ratio=0.75f;

		if(z<column_height&&r<column_radius)
			val=1.f;

		float r_offset=r-doughnut_radius;
		float z_offset=ratio*(z-column_height);
		float doughnut_r=sqrt(r_offset*r_offset+z_offset*z_offset);
		if(doughnut_r<doughnut_minor_radius)
			val=max(1.f,val);

		return val;
	}
};
ExampleHumidityCallback hum_callback;
MushroomHumidityCallback mushroom_callback;
#endif

SimulCloudRenderer::SimulCloudRenderer(simul::clouds::CloudKeyframer *ck)
	:simul::clouds::BaseCloudRenderer(ck)
	,m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,m_pHudVertexDecl(NULL)
	,m_pCloudEffect(NULL)
	,m_pGPULightingEffect(NULL)
	,noise_texture(NULL)
	,volume_noise_texture(NULL)
	,raytrace_layer_texture(NULL)
	,illumination_texture(NULL)
	,sky_loss_texture(NULL)
	,sky_inscatter_texture(NULL)
	,unitSphereVertexBuffer(NULL)
	,unitSphereIndexBuffer(NULL)
	,cloudScales(NULL)
	,cloudOffset(NULL)
	,timing(0.f)
	,vertices(NULL)
	,cpu_fade_vertices(NULL)
	,last_time(0)
	,GPULightingEnabled(true)
	,y_vertical(true)
	,NumBuffers(1)
{
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);

	//cloudNode->AddHumidityCallback(&hum_callback);
	GetCloudInterface()->Generate();

	if(!ck)
		AddChild(cloudKeyframer.get());
	helper->SetYVertical(y_vertical);
	// A noise filter improves the shape of the clouds:
	//cloudNode->GetNoiseInterface()->SetFilter(&circle_f);
}

void SimulCloudRenderer::EnableFilter(bool f)
{
/*	if(f)
		cloudNode->GetNoiseInterface()->SetFilter(&circle_f);
	else
		cloudNode->GetNoiseInterface()->SetFilter(NULL);*/
}

bool SimulCloudRenderer::RestoreDeviceObjects(void *dev)
{
	simul::base::Timer timer;
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	HRESULT hr=S_OK;
	last_time=0.f;
	// create the unit-sphere vertex buffer determined by the Cloud Geometry Helper:
	SAFE_RELEASE(raytrace_layer_texture);
	
	B_RETURN(D3DXCreateTexture(m_pd3dDevice,128,1,1,0,D3DFMT_A32B32G32R32F,d3d_memory_pool,&raytrace_layer_texture))

	float create_raytrace_layer=timer.UpdateTime()/1000.f;

	SAFE_RELEASE(unitSphereVertexBuffer);
	helper->GenerateSphereVertices();
	B_RETURN(m_pd3dDevice->CreateVertexBuffer((unsigned)(helper->GetVertices().size()*sizeof(PosVert_t)),D3DUSAGE_WRITEONLY,0,
									  D3DPOOL_DEFAULT, &unitSphereVertexBuffer,
									  NULL));
	PosVert_t *unit_sphere_vertices;
	B_RETURN(unitSphereVertexBuffer->Lock(0,sizeof(PosVert_t),(void**)&unit_sphere_vertices,0 ));
	PosVert_t *V=unit_sphere_vertices;
	for(size_t i=0;i<helper->GetVertices().size();i++)
	{
		const simul::clouds::CloudGeometryHelper::Vertex &v=helper->GetVertices()[i];
		V->position.x=v.x;
		V->position.y=v.y;
		V->position.z=v.z;
		V++;
	}
	B_RETURN(unitSphereVertexBuffer->Unlock());
	unsigned num_indices=(unsigned)helper->GetQuadStripIndices().size();
	SAFE_RELEASE(unitSphereIndexBuffer);
	B_RETURN(m_pd3dDevice->CreateIndexBuffer(num_indices*sizeof(unsigned),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
		D3DPOOL_DEFAULT, &unitSphereIndexBuffer, NULL ));
	unsigned *indexData;
	B_RETURN(unitSphereIndexBuffer->Lock(0,num_indices, (void**)&indexData, 0 ));
	unsigned *index=indexData;
	for(unsigned i=0;i<num_indices;i++)
	{
		*(index++)	=helper->GetQuadStripIndices()[i];
	}
	B_RETURN(unitSphereIndexBuffer->Unlock());
	float create_unit_sphere=timer.UpdateTime()/1000.f;
	B_RETURN(InitEffects());
	float init_effects=timer.UpdateTime()/1000.f;
	//B_RETURN(RenderNoiseTexture());
	// NOW can set the rendercallback, as we have a device to implement the callback fns with:
//cloudKeyframer->SetRenderCallback(this);
	float set_callback=timer.UpdateTime()/1000.f;
	std::cout<<std::setprecision(4)<<"CLOUDS RESTORE TIMINGS: create_raytrace_layer="<<create_raytrace_layer
		<<", create_unit_sphere="<<create_unit_sphere<<", init_effects="<<init_effects<<", set_callback="<<set_callback<<std::endl;

	cloudKeyframer->SetGpuLightingCallback(this);
	ClearIterators();
	return (hr==S_OK);
}

static std::string GetCompiledFilename(int fade_mde,int wrap_clouds,bool z_vertical)
{
	std::string compiled_filename="simul_clouds_and_lightning_";
	if(fade_mde==SimulCloudRenderer::FRAGMENT)
		compiled_filename+="f1_";
	else if(fade_mde==SimulCloudRenderer::CPU)
		compiled_filename+="f0_";
	if(wrap_clouds)
		compiled_filename+="w1_";
	else
		compiled_filename+="w0_";
	if(z_vertical)
		compiled_filename+="z";
	else
		compiled_filename+="y";
	compiled_filename+=".fxo";
	return compiled_filename;
}

void SimulCloudRenderer::RecompileShaders()
{
	/*std::map<std::string,std::string> defines;
	for(int fade_mde=0;fade_mde<2;fade_mde++)
	{
		for(int wrap_clouds=0;wrap_clouds<2;wrap_clouds++)
		{
			for(int z_vertical=0;z_vertical<2;z_vertical++)
			{
				std::string compiled_filename=GetCompiledFilename(fade_mde,wrap_clouds,(bool)z_vertical);
				if(fade_mde==FRAGMENT)
					defines["FADE_MODE"]="1";
				else if(fade_mde==CPU)
					defines["FADE_MODE"]="0";
				if(wrap_clouds)
					defines["WRAP_CLOUDS"]="1";
				else
					defines["WRAP_CLOUDS"]="0";
				if(z_vertical)
					defines["Z_VERTICAL"]='1';
				else
					defines["Y_VERTICAL"]='1';
			}
		}
	}*/
	wrap=GetCloudInterface()->GetWrap();
	simul::base::Timer timer;
	std::map<std::string,std::string> defines;
	if(fade_mode==FRAGMENT)
		defines["FADE_MODE"]="1";
	if(fade_mode==CPU)
		defines["FADE_MODE"]="0";
	defines["DETAIL_NOISE"]="1";
	if(GetCloudInterface()->GetWrap())
		defines["WRAP_CLOUDS"]="1";
	if(!y_vertical)
		defines["Z_VERTICAL"]='1';
	else
		defines["Y_VERTICAL"]='1';
	float defines_time=timer.UpdateTime()/1000.f;
	SAFE_RELEASE(m_pCloudEffect);
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pCloudEffect,"simul_clouds_and_lightning.fx",defines));
	float clouds_effect=timer.UpdateTime()/1000.f;
	SAFE_RELEASE(m_pGPULightingEffect);
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pGPULightingEffect,"simul_gpulighting.fx",defines));
	float gpulighting_effect=timer.UpdateTime()/1000.f;

	m_hTechniqueCloud					=GetDX9Technique(m_pCloudEffect,"simul_clouds");
	m_hTechniqueCloudMask				=GetDX9Technique(m_pCloudEffect,"cloud_mask");
	m_hTechniqueRaytraceWithLightning	=GetDX9Technique(m_pCloudEffect,"raytrace_clouds_and_lightning");
	m_hTechniqueCloudsAndLightning		=GetDX9Technique(m_pCloudEffect,"simul_clouds_and_lightning");
	m_hTechniqueCrossSectionXZ			=GetDX9Technique(m_pCloudEffect,"cross_section_xz");
	m_hTechniqueCrossSectionXY			=GetDX9Technique(m_pCloudEffect,"cross_section_xy");
	m_hTechniqueRenderTo2DForSaving		=GetDX9Technique(m_pCloudEffect,"render_to_2d_for_saving");
	m_hTechniqueColourLines				=GetDX9Technique(m_pCloudEffect,"colour_lines");
	float techniques=timer.UpdateTime()/1000.f;

	worldViewProj			=m_pCloudEffect->GetParameterByName(NULL,"worldViewProj");
	eyePosition				=m_pCloudEffect->GetParameterByName(NULL,"eyePosition");
	maxFadeDistanceMetres	=m_pCloudEffect->GetParameterByName(NULL,"maxFadeDistanceMetres");
	lightResponse			=m_pCloudEffect->GetParameterByName(NULL,"lightResponse");
	crossSectionOffset		=m_pCloudEffect->GetParameterByName(NULL,"crossSectionOffset");
	lightDir				=m_pCloudEffect->GetParameterByName(NULL,"lightDir");
	skylightColour			=m_pCloudEffect->GetParameterByName(NULL,"skylightColour");
	fractalScale			=m_pCloudEffect->GetParameterByName(NULL,"fractalScale");
	interp					=m_pCloudEffect->GetParameterByName(NULL,"interp");

	mieRayleighRatio		=m_pCloudEffect->GetParameterByName(NULL,"mieRayleighRatio");
	hazeEccentricity		=m_pCloudEffect->GetParameterByName(NULL,"hazeEccentricity");
	cloudEccentricity		=m_pCloudEffect->GetParameterByName(NULL,"cloudEccentricity");
	fadeInterp				=m_pCloudEffect->GetParameterByName(NULL,"fadeInterp");
	distance				=m_pCloudEffect->GetParameterByName(NULL,"distance");
//	cornerPos				=m_pCloudEffect->GetParameterByName(NULL,"cornerPos");
//	texScales				=m_pCloudEffect->GetParameterByName(NULL,"texScales");
	layerFade				=m_pCloudEffect->GetParameterByName(NULL,"layerFade");
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

	lightningIlluminationTexture=m_pCloudEffect->GetParameterByName(NULL,"lightningIlluminationTexture");
	skyLossTexture			=m_pCloudEffect->GetParameterByName(NULL,"skyLossTexture");
	skyInscatterTexture		=m_pCloudEffect->GetParameterByName(NULL,"skyInscatterTexture");
	
	
	
	invViewProj			=m_pCloudEffect->GetParameterByName(NULL,"invViewProj");
	noiseMatrix			=m_pCloudEffect->GetParameterByName(NULL,"noiseMatrix");
	fractalRepeatLength	=m_pCloudEffect->GetParameterByName(NULL,"fractalRepeatLength");
	cloudScales			=m_pCloudEffect->GetParameterByName(NULL,"cloudScales");
	cloudOffset			=m_pCloudEffect->GetParameterByName(NULL,"cloudOffset");
	raytraceLayerTexture=m_pCloudEffect->GetParameterByName(NULL,"raytraceLayerTexture");

	float params=timer.UpdateTime()/1000.f;
	rebuild_shaders=false;
	std::cout<<std::setprecision(4)<<"CLOUDS RELOAD SHADERS TIMINGS: defines="<<defines_time
		<<"clouds_effect="<<clouds_effect
		<<", gpulighting_effect="<<gpulighting_effect
		<<", techniques="<<techniques
		<<", params="<<params<<std::endl;

}

bool SimulCloudRenderer::InitEffects()
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return (hr==S_OK);
	D3DVERTEXELEMENT9 decl[]=
	{
		{0, 0	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0, 12	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0,	24	,D3DDECLTYPE_FLOAT1,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0,	28	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		{0,	36	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,3},
		D3DDECL_END()
	};
	D3DVERTEXELEMENT9 cpu_decl[]=
	{
		{0, 0	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0, 12	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0,	24	,D3DDECLTYPE_FLOAT1,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0,	28	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		{0,	36	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,3},
		{0,	48	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,4},
		{0,	60	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,5},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pHudVertexDecl);
	if(fade_mode!=CPU)
	{
		B_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl))
	}
	else
	{
		B_RETURN(m_pd3dDevice->CreateVertexDeclaration(cpu_decl,&m_pVtxDecl))
	}
	RecompileShaders();
	return (hr==S_OK);
}

bool SimulCloudRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(raytrace_layer_texture);
	if(m_pCloudEffect)
        hr=m_pCloudEffect->OnLostDevice();
	if(m_pGPULightingEffect)
        hr=m_pGPULightingEffect->OnLostDevice();
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pHudVertexDecl);
	SAFE_RELEASE(m_pCloudEffect);
	SAFE_RELEASE(m_pGPULightingEffect);
	for(int i=0;i<3;i++)
		SAFE_RELEASE(cloud_textures[i]);
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(volume_noise_texture);
	SAFE_RELEASE(illumination_texture);
	SAFE_RELEASE(unitSphereVertexBuffer);
	SAFE_RELEASE(unitSphereIndexBuffer);
	//SAFE_RELEASE(large_scale_cloud_texture);
	cloud_tex_width_x=0;
	cloud_tex_length_y=0;
	cloud_tex_depth_z=0;
	ClearIterators();
	return (hr==S_OK);
}

SimulCloudRenderer::~SimulCloudRenderer()
{
	InvalidateDeviceObjects();
}

bool SimulCloudRenderer::RenderNoiseTexture()
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return (hr==S_OK);
	SAFE_RELEASE(noise_texture);
	LPDIRECT3DSURFACE9				pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9				pNoiseRenderTarget=NULL;
	LPD3DXEFFECT					pRenderNoiseEffect=NULL;
	D3DXHANDLE						inputTexture=NULL;
	D3DXHANDLE						octaves=NULL;
	D3DXHANDLE						persistence=NULL;
	LPDIRECT3DTEXTURE9				input_texture=NULL;

	B_RETURN(CreateDX9Effect(m_pd3dDevice,pRenderNoiseEffect,"simul_rendernoise.fx"));
	inputTexture					=pRenderNoiseEffect->GetParameterByName(NULL,"noiseTexture");
	octaves							=pRenderNoiseEffect->GetParameterByName(NULL,"octaves");
	persistence						=pRenderNoiseEffect->GetParameterByName(NULL,"persistence");
	//
	// Make the input texture:
	{
		if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,noise_texture_frequency,noise_texture_frequency,0,default_texture_usage,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&input_texture)))
			return (hr==S_OK);
		D3DLOCKED_RECT lockedRect={0};
		if(FAILED(hr=input_texture->LockRect(0,&lockedRect,NULL,NULL)))
			return (hr==S_OK);
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

	int size=(int)(log((float)noise_texture_size)/log(2.f));
	int freq=(int)(log((float)noise_texture_frequency)/log(2.f));
	texture_octaves=size-freq;

	pRenderNoiseEffect->SetInt(octaves,texture_octaves);

	RenderTexture(m_pd3dDevice,0,0,noise_texture_size,noise_texture_size,
					  input_texture,pRenderNoiseEffect,NULL);

	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);

	D3DXSaveTextureToFile(TEXT("Media/Textures/noise.dds"),D3DXIFF_DDS,noise_texture,NULL);
	noise_texture->GenerateMipSubLevels();
D3DXSaveTextureToFile(TEXT("Media/Textures/noise.jpg"),D3DXIFF_JPG,noise_texture,NULL);
	SAFE_RELEASE(pOldRenderTarget);
	SAFE_RELEASE(pRenderNoiseEffect);
	SAFE_RELEASE(input_texture);
	SAFE_RELEASE(pNoiseRenderTarget);
	return (hr==S_OK);
}

void SimulCloudRenderer::CreateVolumeNoiseTexture()
{
	bool result=true;
	SAFE_RELEASE(volume_noise_texture);
	HRESULT hr=S_OK;
	// Otherwise create it:
#ifndef XBOX
	D3DFORMAT tex_format=D3DFMT_A32B32G32R32F;
#else
	D3DFORMAT tex_format=D3DFMT_LIN_A32B32G32R32F;
#endif
	int size=GetCloudInterface()->GetNoiseResolution();
	if(FAILED(hr=D3DXCreateVolumeTexture(m_pd3dDevice,size,size,size,1,0,tex_format,default_d3d_pool,&volume_noise_texture)))
		return;
	D3DLOCKED_BOX lockedBox={0};
	if(FAILED(hr=volume_noise_texture->LockBox(0,&lockedBox,NULL,NULL)))
		return;
		// Copy the array of floats into the texture.
	const float *src_ptr=GetCloudGridInterface()->GetNoiseInterface()->GetData();
	float *float_ptr=(float *)(lockedBox.pBits);
	for(int i=0;i<size*size*size;i++)
		*float_ptr++=(*src_ptr++);
	hr=volume_noise_texture->UnlockBox(0);
	volume_noise_texture->GenerateMipSubLevels();
}
bool SimulCloudRenderer::CreateNoiseTexture(bool override_file)
{
	if(!m_pd3dDevice)
		return false;
	bool result=true;
	SAFE_RELEASE(noise_texture);
	HRESULT hr=S_OK;
	if(!override_file&&(hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/noise.dds"),&noise_texture))==S_OK)
		return result;
	// Otherwise create it:
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,noise_texture_size,noise_texture_size,0,default_texture_usage,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&noise_texture)))
		return false;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=noise_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return false;
	SetBits8();

	simul::graph::standardnodes::ShowProgressInterface *progress=GetResourceInterface();
	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)(lockedRect.pBits),
		noise_texture_size,noise_texture_frequency,texture_octaves,texture_persistence,progress,this);
	hr=noise_texture->UnlockRect(0);
	noise_texture->GenerateMipSubLevels();

	D3DXSaveTextureToFile(TEXT("Media/Textures/noise.png"),D3DXIFF_PNG,noise_texture,NULL);
	D3DXSaveTextureToFile(TEXT("Media/Textures/noise.dds"),D3DXIFF_DDS,noise_texture,NULL);
	return true;
}

bool SimulCloudRenderer::CanPerformGPULighting() const
{
	return GPULightingEnabled;
}

void SimulCloudRenderer::SetGPULightingParameters(const float *Matrix4x4LightToDensityTexcoords,const unsigned *light_grid,const float *lightspace_extinctions_float3)
{
	for(int i=0;i<16;i++)
		LightToDensityTransform[i]=Matrix4x4LightToDensityTexcoords[i];
	simul::math::Matrix m1(4,4);
	m1=Matrix4x4LightToDensityTexcoords;
	simul::math::Matrix m2(4,4);
	m1.Inverse(m2);
	for(int i=0;i<16;i++)
		DensityToLightTransform[i]=m2.RowPointer(0)[i];
	for(int i=0;i<3;i++)
	{
		light_gridsizes[i]=light_grid[i];
		light_extinctions[i]=lightspace_extinctions_float3[i];
	}
}

void SimulCloudRenderer::PerformFullGPURelight(int which_texture,
	float *target_direct_grid,float *target_indirect_grid)
{
	// To use the GPU to relight clouds:

	// 1. Create two 2D target rendertextures with X and Y sizes given by the light_gridsizes[0] and [1].
	// 2. Set the transform matrix to LightToDensityTransform.
	// 3. Initialize the first target to 1.0
	// 4. Use the first target and the density volume texture as an input to rendering the second target.
	// 5. Swap targets and repeat for all the positions in the Z grid.

	// 1. Create two 2D target rendertextures:
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return;
	LPDIRECT3DTEXTURE9				target_textures[]={NULL,NULL};
	LPDIRECT3DSURFACE9				pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9				pLightRenderTarget[]={NULL,NULL};
	D3DXHANDLE						inputLightTexture=NULL;
	D3DXHANDLE						cloudDensityTexture=NULL;
	D3DXHANDLE						volumeNoiseTexture=NULL;
	D3DXHANDLE						zPosition=NULL;
	D3DXHANDLE						extinctions;
	D3DXHANDLE						lightToDensityMatrix;
	D3DXHANDLE						texCoordOffset;
	D3DXHANDLE						m_hTechniqueGpuLighting;

	inputLightTexture				=m_pGPULightingEffect->GetParameterByName(NULL,"inputLightTexture");
	cloudDensityTexture				=m_pGPULightingEffect->GetParameterByName(NULL,"cloudDensityTexture");
	volumeNoiseTexture				=m_pGPULightingEffect->GetParameterByName(NULL,"volumeNoiseTexture");
	zPosition						=m_pGPULightingEffect->GetParameterByName(NULL,"zPosition");
	extinctions						=m_pGPULightingEffect->GetParameterByName(NULL,"extinctions");
	lightToDensityMatrix			=m_pGPULightingEffect->GetParameterByName(NULL,"transformMatrix");
	texCoordOffset					=m_pGPULightingEffect->GetParameterByName(NULL,"texCoordOffset");
	m_hTechniqueGpuLighting			=m_pGPULightingEffect->GetTechniqueByName("simul_gpulighting");
	m_pGPULightingEffect->SetTechnique(m_hTechniqueGpuLighting);
	// store the current rendertarget for later:
	hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);
	// Make the input texture:
	static unsigned colr=0x00FFFF00;
	for(int i=0;i<2;i++)
	{
	// Make a floating point texture.
		if(FAILED(hr=m_pd3dDevice->CreateTexture(light_gridsizes[0],
			light_gridsizes[1],0,D3DUSAGE_RENDERTARGET|D3DUSAGE_AUTOGENMIPMAP,
			D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,&(target_textures[i]),NULL)))
			return;
		SAFE_RELEASE(pLightRenderTarget[i]);
		target_textures[i]->GetSurfaceLevel(0,&pLightRenderTarget[i]);
		hr=m_pd3dDevice->SetRenderTarget(0,pLightRenderTarget[i]);
		hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,colr,1.f,0L);
		m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	}
	m_pGPULightingEffect->SetVector(extinctions,(D3DXVECTOR4*)(light_extinctions));
	float offset[]={0.5f/(float)light_gridsizes[0],0.5f/(float)light_gridsizes[1],0,0};
	m_pGPULightingEffect->SetVector(texCoordOffset,(D3DXVECTOR4*)(offset));
	m_pGPULightingEffect->SetMatrix(lightToDensityMatrix,(D3DXMATRIX*)(LightToDensityTransform));
	m_pGPULightingEffect->SetTexture(cloudDensityTexture,cloud_textures[(which_texture)%3]);
	m_pGPULightingEffect->SetTexture(volumeNoiseTexture,volume_noise_texture);
	// 4. Use the first target and the density volume texture as an input to rendering the second target.
	// 5. Swap targets and repeat for all the positions in the Z grid.#m_pd3dDevice->BeginScene();
	m_pd3dDevice->BeginScene();
	IDirect3DSurface9 *pBuf;
	hr=m_pd3dDevice->CreateOffscreenPlainSurface(light_gridsizes[0],light_gridsizes[1],D3DFMT_A32B32G32R32F,D3DPOOL_SYSTEMMEM,&pBuf,NULL);
	// For each position along the light direction
	for(int i=0;i<(int)light_gridsizes[2];i++)
	{
		
		
		// Copy the texture to an offscreen surface:
		hr=m_pd3dDevice->GetRenderTargetData(pLightRenderTarget[0],pBuf);
		D3DLOCKED_RECT rect;
		pBuf->LockRect(&rect,NULL,0);
		float *source=(float*)rect.pBits;
		// Copy the light data to the target 3D grid:
		for(int j=0;j<(int)(light_gridsizes[0]*light_gridsizes[1]);j++)
		{
			if(target_indirect_grid)
			{
				*target_direct_grid=source[j*4];
				*target_indirect_grid=source[j*4+1];
				target_indirect_grid++;
			}
			else
			{
				*target_direct_grid=0.5f*(source[j*4]+source[j*4+1]);
			}
			target_direct_grid++;
		}
		pBuf->UnlockRect();
		
		
		
// Set up the rendertarget:
		hr=m_pd3dDevice->SetRenderTarget(0,pLightRenderTarget[1]);
		m_pGPULightingEffect->SetTexture(inputLightTexture,target_textures[0]);
		m_pGPULightingEffect->SetFloat(zPosition,((float)i)/(float(light_gridsizes[2])));
		RenderTexture(m_pd3dDevice,0,0,light_gridsizes[0],light_gridsizes[1],
					  target_textures[0],m_pGPULightingEffect,NULL);
		std::swap(target_textures[0],target_textures[1]);
		std::swap(pLightRenderTarget[0],pLightRenderTarget[1]);
		
	}
	SAFE_RELEASE(pBuf);
	m_pd3dDevice->EndScene();
	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);

	SAFE_RELEASE(pOldRenderTarget);
	for(int i=0;i<2;i++)
	{
		SAFE_RELEASE(target_textures[i]);
		SAFE_RELEASE(pLightRenderTarget[i]);
	}
}
// Use the GPU to transfer the oblique light grid data to the main grid
void SimulCloudRenderer::GPUTransferDataToTexture(	int which_texture
													,unsigned char *target_grid
													,const unsigned char *direct_grid
													,const unsigned char *indirect_grid
													,const unsigned char *ambient_grid)
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return;
	LPDIRECT3DTEXTURE9					target_texture=NULL;
	LPDIRECT3DVOLUMETEXTURE9			direct_texture=NULL;
	LPDIRECT3DVOLUMETEXTURE9			indirect_texture=NULL;
	LPDIRECT3DVOLUMETEXTURE9			ambient_texture=NULL;
	LPDIRECT3DVOLUMETEXTURE9			density_texture=cloud_textures[(which_texture)%3];
	LPDIRECT3DSURFACE9					pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9					pRenderTarget=NULL;
	D3DXHANDLE							inputDirectLightTexture		=NULL;
	D3DXHANDLE							inputIndirectLightTexture	=NULL;
	D3DXHANDLE							inputAmbientLightTexture	=NULL;
	D3DXHANDLE							cloudDensityTexture			=NULL;
	D3DXHANDLE							zPosition=NULL;
	D3DXHANDLE							densityToLightMatrix;
	D3DXHANDLE							texCoordOffset;
	D3DXHANDLE							m_hTechniqueGpuTransformLightgrid;

	inputDirectLightTexture				=m_pGPULightingEffect->GetParameterByName(NULL,"inputDirectLightTexture");
	inputIndirectLightTexture			=m_pGPULightingEffect->GetParameterByName(NULL,"inputIndirectLightTexture");
	inputAmbientLightTexture			=m_pGPULightingEffect->GetParameterByName(NULL,"inputAmbientLightTexture");
	cloudDensityTexture					=m_pGPULightingEffect->GetParameterByName(NULL,"cloudDensityTexture");
	zPosition							=m_pGPULightingEffect->GetParameterByName(NULL,"zPosition");
	densityToLightMatrix				=m_pGPULightingEffect->GetParameterByName(NULL,"transformMatrix");
	texCoordOffset						=m_pGPULightingEffect->GetParameterByName(NULL,"texCoordOffset");
	m_hTechniqueGpuTransformLightgrid	=m_pGPULightingEffect->GetTechniqueByName("simul_transform_lightgrid");
	m_pGPULightingEffect->SetTechnique(m_hTechniqueGpuTransformLightgrid);
	// store the current rendertarget for later:
	hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);
	// Make the input textures:
	static unsigned colr=0x00FFFF00;
	
	// Make a 32-bit target texture.
	if(FAILED(hr=m_pd3dDevice->CreateTexture(cloud_tex_width_x,
		cloud_tex_length_y,0,D3DUSAGE_RENDERTARGET|D3DUSAGE_AUTOGENMIPMAP,
		cloud_tex_format,D3DPOOL_DEFAULT,&target_texture,NULL)))
		return;
	SAFE_RELEASE(pRenderTarget);
	target_texture->GetSurfaceLevel(0,&pRenderTarget);
	hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
	hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,colr,1.f,0L);
	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	
	D3DLOCKED_RECT rect;
	D3DLOCKED_BOX lockedBox;
	unsigned size2d=4*sizeof(unsigned char)*cloud_tex_width_x*cloud_tex_length_y;
	unsigned size3d=sizeof(unsigned char)*light_gridsizes[0]*light_gridsizes[1]*light_gridsizes[2];
	// the light textures:
	if(direct_grid)
	{
		if(FAILED(hr=D3DXCreateVolumeTexture(m_pd3dDevice,
			light_gridsizes[0],light_gridsizes[1],light_gridsizes[2],
			1,0,D3DFMT_L8,D3DPOOL_MANAGED,&direct_texture)))
			return;
		// Copy the light data to the target 3D grid:
		direct_texture->LockBox(0,&lockedBox,NULL,NULL);
		unsigned char *target=(unsigned char*)lockedBox.pBits;
		memcpy(target,direct_grid,size3d);
		direct_texture->UnlockBox(0);
	}
	if(indirect_grid)
	{
		if(FAILED(hr=D3DXCreateVolumeTexture(m_pd3dDevice,
			light_gridsizes[0],light_gridsizes[1],light_gridsizes[2],
			1,0,D3DFMT_L8,D3DPOOL_MANAGED,&indirect_texture)))
			return;
		indirect_texture->LockBox(0,&lockedBox,NULL,NULL);
		unsigned char *target=(unsigned char*)lockedBox.pBits;
		memcpy(target,indirect_grid,size3d);
		indirect_texture->UnlockBox(0);
	}
	if(ambient_grid)
	{
		if(FAILED(hr=D3DXCreateVolumeTexture(m_pd3dDevice,
			cloud_tex_width_x,cloud_tex_length_y,cloud_tex_depth_z
			,1,0,D3DFMT_L8,D3DPOOL_MANAGED,&ambient_texture)))
			return;
		ambient_texture->LockBox(0,&lockedBox,NULL,NULL);
		unsigned char *target=(unsigned char*)lockedBox.pBits;
		memcpy(target,ambient_grid,size3d);
		ambient_texture->UnlockBox(0);
	}
	float offset[]={0.5f/(float)cloud_tex_width_x,0.5f/(float)cloud_tex_length_y,0,0};
	m_pGPULightingEffect->SetVector(texCoordOffset,(D3DXVECTOR4*)(offset));
	m_pGPULightingEffect->SetMatrix(densityToLightMatrix,(D3DXMATRIX*)(DensityToLightTransform));
	m_pGPULightingEffect->SetTexture(cloudDensityTexture,density_texture);
	m_pGPULightingEffect->SetTexture(inputDirectLightTexture,direct_texture);
	m_pGPULightingEffect->SetTexture(inputIndirectLightTexture,indirect_texture);
	m_pGPULightingEffect->SetTexture(inputAmbientLightTexture,ambient_texture);
	// 4. Use the first target and the density volume texture as an input to rendering the second target.
	// 5. Swap targets and repeat for all the positions in the Z grid.#m_pd3dDevice->BeginScene();
	m_pd3dDevice->BeginScene();
	IDirect3DSurface9 *pBuf;
	hr=m_pd3dDevice->CreateOffscreenPlainSurface(cloud_tex_width_x,cloud_tex_length_y,cloud_tex_format,D3DPOOL_SYSTEMMEM,&pBuf,NULL);
	// For each xy surface
	for(int i=0;i<(int)cloud_tex_depth_z;i++)
	{
// Set up the rendertarget:
		hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		m_pGPULightingEffect->SetFloat(zPosition,((float)i+0.5f)/(float(cloud_tex_depth_z)));
		RenderTexture(m_pd3dDevice,0,0,cloud_tex_width_x,cloud_tex_length_y,NULL,m_pGPULightingEffect,m_hTechniqueGpuTransformLightgrid);
		m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
		// Copy the texture to an offscreen surface:
		hr=m_pd3dDevice->GetRenderTargetData(pRenderTarget,pBuf);
		pBuf->LockRect(&rect,NULL,0);
		unsigned char *source=(unsigned char*)rect.pBits;
		// Copy the light data to the target 3D grid:
		memcpy(target_grid,source,size2d);
		target_grid+=size2d;
		pBuf->UnlockRect();
	}
	SAFE_RELEASE(pBuf);
	m_pd3dDevice->EndScene();

	SAFE_RELEASE(pOldRenderTarget);
	SAFE_RELEASE(target_texture);
	SAFE_RELEASE(direct_texture);
	SAFE_RELEASE(indirect_texture);
	SAFE_RELEASE(ambient_texture);
	SAFE_RELEASE(pRenderTarget);
}
void SimulCloudRenderer::EnsureTextureCycle()
{
	int cyc=(cloudKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(cloud_textures[0],cloud_textures[1]);
		std::swap(cloud_textures[1],cloud_textures[2]);
		std::swap(seq_texture_iterator[0],seq_texture_iterator[1]);
		std::swap(seq_texture_iterator[1],seq_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
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

bool SimulCloudRenderer::Render(bool cubemap,bool depth_testing,bool default_fog)
{
	return Render(cubemap,depth_testing,default_fog,0);
}

void SimulCloudRenderer::EnsureCorrectIlluminationTextureSizes()
{
	simul::clouds::CloudKeyframer::int3 i=cloudKeyframer->GetIlluminationTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	if(!width_x||!length_y||!depth_z)
		return;
	if(width_x==illum_tex_width_x&&length_y==illum_tex_length_y&&depth_z==illum_tex_depth_z)
		return;
	illum_tex_width_x	=width_x;
	illum_tex_length_y	=length_y;
	illum_tex_depth_z	=depth_z;
	SAFE_RELEASE(illumination_texture);
	VOID_RETURN(D3DXCreateVolumeTexture(m_pd3dDevice,width_x,length_y,depth_z,1,0,illumination_tex_format,default_d3d_pool,&illumination_texture));
	D3DLOCKED_BOX lockedBox={0};
	if(FAILED(illumination_texture->LockBox(0,&lockedBox,NULL,NULL)))
		return;
	memset(lockedBox.pBits,0,4*width_x*length_y*depth_z);
	VOID_RETURN(illumination_texture->UnlockBox(0));
}

void SimulCloudRenderer::EnsureCorrectTextureSizes()
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
	V_CHECK(CanUseTexFormat(m_pd3dDevice,cloud_tex_format));
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		if(FAILED(hr=D3DXCreateVolumeTexture(m_pd3dDevice,width_x,length_y,depth_z,1,0,cloud_tex_format,default_d3d_pool,&cloud_textures[i])))
			return;
	}
}

void SimulCloudRenderer::EnsureTexturesAreUpToDate()
{
	if(!volume_noise_texture)
	{
		CreateVolumeNoiseTexture();
	}
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		simul::sky::BaseKeyframer::seq_texture_fill texture_fill=cloudKeyframer->GetSequentialTextureFill(seq_texture_iterator[i]);
		if(!texture_fill.num_texels)
			continue;
		if(!cloud_textures[i])
			continue;
		D3DLOCKED_BOX lockedBox={0};
		if(FAILED(cloud_textures[i]->LockBox(0,&lockedBox,NULL,NULL)))
			continue;
		unsigned *ptr=(unsigned *)(lockedBox.pBits);
		ptr+=texture_fill.texel_index;
		memcpy(ptr,texture_fill.uint32_array,texture_fill.num_texels*sizeof(unsigned));
		cloud_textures[i]->UnlockBox(0);
	}
}

void SimulCloudRenderer::EnsureIlluminationTexturesAreUpToDate()
{
	for(int i=0;i<4;i++)
	{
		simul::sky::BaseKeyframer::seq_texture_fill texture_fill=cloudKeyframer->GetSequentialIlluminationTextureFill(seq_illum_texture_iterator[i]);
		if(!texture_fill.num_texels)
			return;
		D3DLOCKED_BOX lockedBox={0};
		HRESULT hr=illumination_texture->LockBox(0,&lockedBox,NULL,NULL);
		if(FAILED(hr))
			return;
		unsigned *ptr=(unsigned *)(lockedBox.pBits);
		ptr+=texture_fill.texel_index;
		static unsigned offset[]={16,8,0,24};
		const unsigned char *uchar8_array=(const unsigned char *)texture_fill.uint32_array;
		for(int i=0;i<texture_fill.num_texels;i++)
		{
			unsigned ui=(unsigned)(*uchar8_array);
			ui<<=offset[i];
			unsigned msk=255<<offset[i];
			msk=~msk;
			(*ptr)&=msk;
			(*ptr)|=ui;
			uchar8_array++;
			ptr++;
		}
		hr=illumination_texture->UnlockBox(0);
	}
}

bool SimulCloudRenderer::Render(bool cubemap,bool depth_testing,bool default_fog,int buffer_index)
{
	EnsureTexturesAreUpToDate();
	depth_testing;
	default_fog;
	PIXBeginNamedEvent(0xFF00FFFF,"SimulCloudRenderer::Render");
	if(wrap!=GetCloudInterface()->GetWrap())
	{
		rebuild_shaders=true;
	}
	if(rebuild_shaders)
		InitEffects();
	HRESULT hr=S_OK;
	enable_lightning=cloudKeyframer->GetInterpolatedKeyframe().lightning>0;
	if(!noise_texture)
	{
		B_RETURN(CreateNoiseTexture());
	}
	// Disable any in-texture gamma-correction that might be lingering from some other bit of rendering:
	/*m_pd3dDevice->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(1,D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(2,D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(3,D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(4,D3DSAMP_SRGBTEXTURE,0);
	m_pd3dDevice->SetSamplerState(5,D3DSAMP_SRGBTEXTURE,0);*/
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	if(!vertices&&fade_mode!=CPU)
		vertices=new Vertex_t[MAX_VERTICES];
	if(!cpu_fade_vertices&&fade_mode==CPU)
		cpu_fade_vertices=new CPUFadeVertex_t[MAX_VERTICES];
	static D3DTEXTUREADDRESS wrap_u=D3DTADDRESS_WRAP,wrap_v=D3DTADDRESS_WRAP,wrap_w=D3DTADDRESS_CLAMP;

	m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[0]);
	m_pCloudEffect->SetTexture(cloudDensity2				,cloud_textures[1]);
	m_pCloudEffect->SetTexture(noiseTexture					,noise_texture);

	if(enable_lightning)
	{
		EnsureCorrectIlluminationTextureSizes();
		EnsureIlluminationTexturesAreUpToDate();
		m_pCloudEffect->SetTexture(lightningIlluminationTexture	,illumination_texture);

	}

	if(fade_mode==FRAGMENT)
	{
		m_pCloudEffect->SetTexture(skyLossTexture				,sky_loss_texture);
		m_pCloudEffect->SetTexture(skyInscatterTexture			,sky_inscatter_texture);
	}

	// Mess with the proj matrix to extend the far clipping plane? not now

	GetCameraPosVector(view,y_vertical,cam_pos);
	simul::math::Vector3 wind_offset=GetCloudInterface()->GetWindOffset();

	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);
	
	if(!y_vertical)
		view_dir.Define(-view._13,-view._23,-view._33);

	float t=skyInterface->GetTime();
	float delta_t=(t-last_time)*cloudKeyframer->GetTimeFactor();
	if(!last_time)
		delta_t=0;
	last_time=t;

	helper->SetChurn(GetCloudInterface()->GetChurn());
	helper->Update((const float*)cam_pos,wind_offset,view_dir,up,delta_t,cubemap);
	//if(y_vertical)
	//	std::swap(cam_pos.y,cam_pos.z);

	static float direct_light_mult=0.25f;
	static float indirect_light_mult=0.03f;
	simul::sky::float4 light_response(	direct_light_mult*GetCloudInterface()->GetLightResponse(),
										indirect_light_mult*GetCloudInterface()->GetSecondaryLightResponse(),
										0,
										0);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();
	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	// Get the overall ambient light at this altitude, and multiply it by the cloud's ambient response.
	float base_alt_km=0.001f*(GetCloudInterface()->GetCloudBaseZ());//+.5f*GetCloudInterface()->GetCloudHeight());
	simul::sky::float4 sky_light_colour=skyInterface->GetAmbientLight(base_alt_km)*GetCloudInterface()->GetAmbientLightResponse();


	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	//helper->SetNoFrustumLimit(true);
	helper->MakeGeometry(GetCloudInterface(),GetCloudGridInterface(),false);

	if(fade_mode==CPU)
		helper->CalcInscatterFactors(skyInterface);

	float cloud_interp=cloudKeyframer->GetInterpolation();
	m_pCloudEffect->SetFloat	(interp					,cloud_interp);
	m_pCloudEffect->SetVector	(eyePosition			,(D3DXVECTOR4*)(&cam_pos));
	m_pCloudEffect->SetFloat	(maxFadeDistanceMetres	,max_fade_distance_metres);
	m_pCloudEffect->SetVector	(lightResponse			,(D3DXVECTOR4*)(&light_response));
	m_pCloudEffect->SetVector	(lightDir			,(D3DXVECTOR4*)(&sun_dir));
	m_pCloudEffect->SetVector	(skylightColour		,(D3DXVECTOR4*)(&sky_light_colour));
	simul::sky::float4 fractal_scales=(cubemap?0.f:1.f)*helper->GetFractalScales(GetCloudInterface());
	m_pCloudEffect->SetVector	(fractalScale		,(D3DXVECTOR4*)(&fractal_scales));
	m_pCloudEffect->SetVector	(mieRayleighRatio	,MakeD3DVector(skyInterface->GetMieRayleighRatio()));
	m_pCloudEffect->SetFloat	(hazeEccentricity	,skyInterface->GetMieEccentricity());
	m_pCloudEffect->SetFloat	(cloudEccentricity	,GetCloudInterface()->GetMieAsymmetry());
	m_pCloudEffect->SetFloat	(fadeInterp			,fade_interp);
	m_pCloudEffect->SetFloat	(alphaSharpness		,GetCloudInterface()->GetAlphaSharpness());

	if(enable_lightning)
	{
		static float bb=2.f;
		simul::sky::float4 lightning_multipliers;
		lightning_colour=lightningRenderInterface->GetLightningColour();
		for(unsigned i=0;i<4;i++)
		{
			if(i<lightningRenderInterface->GetNumLightSources())
				lightning_multipliers[i]=bb*lightningRenderInterface->GetLightSourceBrightness(i);
			else lightning_multipliers[i]=0;
		}
		static float lightning_effect_on_cloud=20.f;
		lightning_colour.w=lightning_effect_on_cloud;
		m_pCloudEffect->SetVector	(lightningMultipliers	,(D3DXVECTOR4*)(&lightning_multipliers));
		m_pCloudEffect->SetVector	(lightningColour		,&lightning_colour);

		simul::math::Vector3 light_X1,light_X2,light_DX;
		light_X1=lightningRenderInterface->GetIlluminationOrigin();
		light_DX=lightningRenderInterface->GetIlluminationScales();

		m_pCloudEffect->SetVector	(illuminationOrigin		,(D3DXVECTOR4*)(&light_X1));
		m_pCloudEffect->SetVector	(illuminationScales		,(D3DXVECTOR4*)(&light_DX));
	}
	
	hr=m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl);
//UINT passes=1;

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

	//InternalRenderHorizontal(buffer_index);
	if(render_mode==SLICES)
		InternalRenderVolumetric(buffer_index);
	else
		InternalRenderRaytrace(buffer_index);
	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	PIXEndNamedEvent();
	return (hr==S_OK);
}

void SimulCloudRenderer::NumBuffersChanged()
{
}

void SimulCloudRenderer::InternalRenderHorizontal(int buffer_index)
{
	buffer_index;
	HRESULT hr=S_OK;
	float max_dist=max_fade_distance_metres;
	//set up matrices
	D3DXMATRIX wvp;
	FixProjectionMatrix(proj,max_dist*1.1f,y_vertical);
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	m_pCloudEffect->SetMatrix(worldViewProj,&wvp);
	unsigned passes=0;
	hr=m_pCloudEffect->Begin(&passes,0);
	hr=m_pCloudEffect->BeginPass(0);
	int R_GRID=20;
	int A_GRID=20;
	simul::math::Vector3 pos,tex_pos;
	int v=0;
	// arbitrary:
	float min_dist=helper->GetMaxCloudDistance()*0.5f;
	float fadeout_dist=helper->GetMaxCloudDistance();
	float base_alt_km=0.001f*(GetCloudInterface()->GetCloudBaseZ());
	float view_alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(base_alt_km);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();
	// Convert metres to km:
	float scale=0.001f;
	static int num_layers=4;
	for(int l=0;l<num_layers;l++)
	{
		float u=0.f;
		if(view_alt_km>base_alt_km)
			u=0.5f+(float)l;
		else
			u=(float)num_layers-0.5f-(float)l;
		float z0=GetCloudInterface()->GetCloudBaseZ()+GetCloudInterface()->GetCloudHeight()*(u)/(float)num_layers;
		for(int i=0;i<R_GRID;i++)
		{
			for(int j=0;j<=A_GRID;j++)
			{
				int jj=j;
				if(i%2)
				{
					jj=A_GRID-j;
				}
				float angle=(float)jj/(float)A_GRID*2.f*pi;
				float cs=cos(angle);
				float sn=sin(angle);
				for(int k=0;k<2;k++)
				{
					float interp=(float)(R_GRID-i-1+k)/(float)R_GRID;
					float dist=max_dist*interp;
					float fadeout=(dist-min_dist)/(fadeout_dist-min_dist);
					if(fadeout<=0)
						continue;
					if(fadeout>1.f)
						fadeout=1.f;
					float x=cam_pos.x+dist*cs;
					float y=(y_vertical?cam_pos.z:cam_pos.y)+dist*sn;
					float z=z0+helper->GetVerticalShiftDueToCurvature(dist,z0);
					float elev=atan2(z,dist);
					pos.Define(x,y,z);
					simul::sky::float4 dir_to_vertex(x-cam_pos.x,y-cam_pos.y,z-cam_pos.z,0);
					dir_to_vertex/=simul::sky::length(dir_to_vertex);
					tex_pos=helper->WorldCoordToTexCoord(GetCloudInterface(),pos);
					std::swap(pos.y,pos.z);
					if(v>=MAX_VERTICES)
						break;
					Vertex_t *vertex=NULL;
					if(fade_mode!=CPU)
						vertex=&vertices[v];
					else
						vertex=&cpu_fade_vertices[v];
					vertex->position=pos;
					vertex->texCoords=tex_pos;
					vertex->texCoordsNoise.x=0;
					vertex->texCoordsNoise.y=0;
					vertex->layerFade=fadeout;
					vertex->sunlightColour.x=sunlight.x;
					vertex->sunlightColour.y=sunlight.y;
					vertex->sunlightColour.z=sunlight.z;
					float cos0=simul::sky::float4::dot(sun_dir,dir_to_vertex);
					if(fade_mode==CPU)
					{
						simul::sky::float4 loss=skyInterface->GetIsotropicColourLossFactor(view_alt_km,elev,0,scale*dist);
						simul::sky::float4 inscatter=
							skyInterface->GetIsotropicInscatterFactor(view_alt_km,elev,0,scale*dist,false,0);
						inscatter*=skyInterface->GetInscatterAngularMultiplier(cos0,inscatter.w,view_alt_km);
						CPUFadeVertex_t *cpu_vertex=&cpu_fade_vertices[v];
						cpu_vertex->loss.x=loss.x;
						cpu_vertex->loss.y=loss.y;
						cpu_vertex->loss.z=loss.z;
						cpu_vertex->inscatter.x=inscatter.x;
						cpu_vertex->inscatter.y=inscatter.y;
						cpu_vertex->inscatter.z=inscatter.z;
					}
					v++;
					if(v>=MAX_VERTICES)
						break;
				}
			}
		}
		if(v>2)
		{
			if(fade_mode!=CPU)
				hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,v-2,&(vertices[0]),sizeof(Vertex_t));
			else
				hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,v-2,&(cpu_fade_vertices[0]),sizeof(CPUFadeVertex_t));
		}
	}

	hr=m_pCloudEffect->EndPass();
	hr=m_pCloudEffect->End();
}

bool SimulCloudRenderer::FillRaytraceLayerTexture()
{
	D3DLOCKED_RECT lockedRect={0};
	HRESULT hr=S_OK;
	B_RETURN(raytrace_layer_texture->LockRect(0,&lockedRect,NULL,NULL));
	float *float_ptr=(float *)(lockedRect.pBits);
	typedef std::vector<simul::clouds::CloudGeometryHelper::Slice*>::const_reverse_iterator iter;
	static float cutoff=100000.f;
	int j=0;
	for(iter i=helper->GetSlices().rbegin();i!=helper->GetSlices().rend()&&j<128;i++,j++)
	{
		float distance=(*i)->distance;
		*float_ptr++=distance;
		*float_ptr++=(*i)->noise_tex_x;
		*float_ptr++=(*i)->noise_tex_y;
		*float_ptr++=0;
	}
	memset(float_ptr,0,4*sizeof(float)*(128-j));
	hr=raytrace_layer_texture->UnlockRect(0);
	return (hr==S_OK);
}

void SimulCloudRenderer::InternalRenderRaytrace(int buffer_index)
{
	FillRaytraceLayerTexture();
	buffer_index;
	HRESULT hr=S_OK;
	m_pCloudEffect->SetTechnique(m_hTechniqueRaytraceWithLightning);
	PIXWrapper(0xFFFFFF00,"Render Cloud Raytrace")
	{
#ifndef XBOX
		m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
		m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
		D3DXMATRIX wvp,vpt,ivp;
		//FixProjectionMatrix(proj,helper->GetMaxCloudDistance()*1.1f,y_vertical);
		D3DXMATRIX viewproj;
		//view._41=view._42=view._43=0;
		D3DXMatrixMultiply(&viewproj,&view,&proj);
		MakeWorldViewProjMatrix(&wvp,world,view,proj);
		m_pCloudEffect->SetMatrix(worldViewProj, &wvp);
		D3DXMatrixTranspose(&vpt,&viewproj);
		D3DXMatrixInverse(&ivp,NULL,&vpt);


		hr=m_pCloudEffect->SetMatrix(invViewProj,&ivp);

		// The NOISE matrix is for 2D noise texcoords:
		const simul::geometry::SimulOrientation &noise_orient=helper->GetNoiseOrientation();
		hr=m_pCloudEffect->SetMatrix(noiseMatrix,(const D3DXMATRIX*)noise_orient.GetInverseMatrix().RowPointer(0));
		hr=m_pCloudEffect->SetFloat(fractalRepeatLength,GetCloudInterface()->GetFractalRepeatLength());

		hr=m_pCloudEffect->SetTexture(raytraceLayerTexture,raytrace_layer_texture);
		simul::sky::float4 cloud_scales=GetCloudScales();
		simul::sky::float4 cloud_offset=GetCloudOffset();
	
		m_pCloudEffect->SetVector	(cloudScales	,(const D3DXVECTOR4*)&cloud_scales);
		m_pCloudEffect->SetVector	(cloudOffset	,(const D3DXVECTOR4*)&cloud_offset);
		D3DXVECTOR3 d3dcam_pos;
		GetCameraPosVector(view,y_vertical,(float*)&d3dcam_pos);
		float altitude=y_vertical?d3dcam_pos.y:d3dcam_pos.z;
		hr=m_pCloudEffect->SetFloat(fadeInterp,fade_interp);
		if(skyInterface)
		{
//		hr=m_pCloudEffect->SetFloat(HazeEccentricity,skyInterface->GetMieEccentricity());
			D3DXVECTOR4 mie_rayleigh_ratio(skyInterface->GetMieRayleighRatio());
			D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight());
			D3DXVECTOR4 light_colour(skyInterface->GetLocalIrradiance(altitude));
			
			//light_colour*=strength;
			if(y_vertical)
				std::swap(sun_dir.y,sun_dir.z);

			m_pCloudEffect->SetVector	(lightDir			,&sun_dir);
			//m_pCloudEffect->SetVector	(lightColour	,(const D3DXVECTOR4*)&light_colour);
		}
		m_pCloudEffect->SetVector	(cloudScales	,(const D3DXVECTOR4*)&cloud_scales);
		m_pCloudEffect->SetVector	(cloudOffset	,(const D3DXVECTOR4*)&cloud_offset);
		m_pCloudEffect->SetVector	(eyePosition	,(const D3DXVECTOR4*)&(cam_pos));

		hr=DrawFullScreenQuad(m_pd3dDevice,m_pCloudEffect);
	}
}

void SimulCloudRenderer::InternalRenderVolumetric(int buffer_index)
{
	buffer_index;
	HRESULT hr=S_OK;
	//set up matrices
	D3DXMATRIX wvp;
	FixProjectionMatrix(proj,helper->GetMaxCloudDistance()*1.1f,y_vertical);
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	m_pCloudEffect->SetMatrix(worldViewProj, &wvp);
	unsigned passes=0;
	hr=m_pCloudEffect->Begin(&passes,0);
	hr=m_pCloudEffect->BeginPass(0);
	int v=0;
	simul::math::Vector3 pos;
	float base_alt_km=0.001f*(GetCloudInterface()->GetCloudBaseZ());//+.5f*GetCloudInterface()->GetCloudHeight());
	unsigned grid_x,el_grid;
	helper->GetGrid(el_grid,grid_x);
	simul::sky::float4 view_km=(const float*)cam_pos;
	static std::vector<simul::sky::float4> light_colours;
	light_colours.resize(el_grid+1);
	view_km*=0.001f;
	typedef std::vector<simul::clouds::CloudGeometryHelper::Slice*>::const_iterator iter;
	static float cutoff=100000.f;	// Get the sunlight at this altitude:
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(base_alt_km);
	for(iter i=helper->GetSlices().begin();i!=helper->GetSlices().end();i++)
	{
		float distance=(*i)->distance;
		if(NumBuffers==2)
		{
			if(buffer_index==0&&distance<cutoff)
				continue;
			else if(buffer_index==1&&distance>=cutoff)
				continue;
		}
		helper->MakeLayerGeometry(GetCloudInterface(),*i);
		const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
		size_t qs_vert=0;
		float fade=(*i)->fadeIn;
		bool start=true;
		if((*i)->quad_strips.size())
		for(int j=(*i)->elev_start;j<=(*i)->elev_end;j++)
		{
			float j_interp=(float)j/(float)el_grid;
			float sine=(2.f*j_interp-1.f);
			float alt_km=min(max(0.f,view_km.z+sine*0.001f*distance),0.001f*(GetCloudInterface()->GetCloudBaseZ()+GetCloudInterface()->GetCloudHeight()));
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
					break;
				}
				if(start)
				{
					Vertex_t *startvertex=NULL;
					if(fade_mode!=CPU)
						startvertex=&vertices[v];
					if(fade_mode==CPU)
						startvertex=&cpu_fade_vertices[v];
					startvertex->position=pos;
					v++;
					start=false;
				}
				Vertex_t *vertex=NULL;
				if(fade_mode!=CPU)
					vertex=&vertices[v];
				else
					vertex=&cpu_fade_vertices[v];
				vertex->position=pos;
				vertex->texCoords=tex_pos;
				vertex->texCoordsNoise.x=V.noise_tex_x;
				vertex->texCoordsNoise.y=V.noise_tex_y;
				vertex->layerFade=fade;
				vertex->sunlightColour.x=sunlight.x;
				vertex->sunlightColour.y=sunlight.y;
				vertex->sunlightColour.z=sunlight.z;
				if(fade_mode==CPU)
				{
					simul::sky::float4 loss			=helper->GetLoss(*i,V);
					simul::sky::float4 inscatter	=helper->GetInscatter(*i,V);
					CPUFadeVertex_t *cpu_vertex=&cpu_fade_vertices[v];
					cpu_vertex->loss.x=loss.x;
					cpu_vertex->loss.y=loss.y;
					cpu_vertex->loss.z=loss.z;
					cpu_vertex->inscatter.x=inscatter.x;
					cpu_vertex->inscatter.y=inscatter.y;
					cpu_vertex->inscatter.z=inscatter.z;
				}
			}
		}
		if(v>=MAX_VERTICES)
		{
			break;
		}
		Vertex_t *vertex=NULL;
		if(fade_mode!=CPU)
			vertex=&vertices[v];
		else
			vertex=&cpu_fade_vertices[v];
		vertex->position=pos;
		v++;
	}
	if(v>2)
	{
		if(fade_mode!=CPU)
			hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,v-2,&(vertices[0]),sizeof(Vertex_t));
		else
			hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,v-2,&(cpu_fade_vertices[0]),sizeof(CPUFadeVertex_t));
	}

	hr=m_pCloudEffect->EndPass();
	hr=m_pCloudEffect->End();
}

#ifdef XBOX
void SimulCloudRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
#endif

bool SimulCloudRenderer::MakeCubemap()
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

		Render(true,false,false,0);
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
	return (hr==S_OK);
}
float SimulCloudRenderer::GetTiming() const
{
	return timing;
}

void *SimulCloudRenderer::GetIlluminationTexture()
{
	return (void *)illumination_texture;
}

void **SimulCloudRenderer::GetCloudTextures()
{
	return (void **)cloud_textures;
}
void SimulCloudRenderer::SaveCloudTexture(const char *filename)
{
	std::wstring fn=simul::base::StringToWString(filename);
	std::wstring ext=L".png";
	Framebuffer fb;
	static D3DFORMAT f=D3DFMT_A8R8G8B8;
	fb.SetFormat(f);
	fb.SetWidthAndHeight(cloud_tex_width_x*2,cloud_tex_length_y*cloud_tex_depth_z);
	fb.RestoreDeviceObjects(m_pd3dDevice);
	fb.Activate();
	static unsigned b=0x00000000;
	HRESULT hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,b,1.f,0L);
	m_pCloudEffect->SetTechnique(m_hTechniqueRenderTo2DForSaving);
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	m_pd3dDevice->SetTransform(D3DTS_VIEW,&ident);
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&ident);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&ident);
	m_pCloudEffect->SetMatrix(worldViewProj,&ident);



	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	// blending for alpha:
	m_pd3dDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_SRCALPHA);
	m_pd3dDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_ZERO);

	hr=DrawFullScreenQuad(m_pd3dDevice,m_pCloudEffect);

	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);

	fb.Deactivate();
	fn+=ext;
	//hr=D3DXSaveSurfaceToFile((std::wstring(fn)+L".bmp").c_str(),D3DXIFF_BMP,fb.m_pHDRRenderTarget,NULL,NULL);
	SaveTexture(fb.hdr_buffer_texture,(std::string(filename)+".png").c_str());
	fb.InvalidateDeviceObjects();
}

bool SimulCloudRenderer::RenderCrossSections(int width)
{
	HRESULT hr=S_OK;
	int w=(width-16)/3;
	int h=(GetCloudGridInterface()->GetGridHeight()*w)/GetCloudGridInterface()->GetGridWidth();
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				dynamic_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		D3DXVECTOR4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		m_pCloudEffect->SetVector	(lightResponse		,(D3DXVECTOR4*)(&light_response));
		m_pCloudEffect->SetFloat(crossSectionOffset,GetCloudInterface()->GetWrap()?0.5f:0.f);
		m_pCloudEffect->SetTexture(cloudDensity1,cloud_textures[(i)%3]);
		RenderTexture(m_pd3dDevice,i*w+8,4,w-2,h,cloud_textures[(i)%3],m_pCloudEffect,m_hTechniqueCrossSectionXZ);
	}
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				dynamic_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		D3DXVECTOR4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		m_pCloudEffect->SetVector(lightResponse		,(D3DXVECTOR4*)(&light_response));
		m_pCloudEffect->SetFloat(crossSectionOffset	,GetCloudInterface()->GetWrap()?0.5f:0.f);
		m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[(i)%3]);
		RenderTexture(m_pd3dDevice,i*130+8,h+8,128,128,
					  cloud_textures[(i)%3],m_pCloudEffect,m_hTechniqueCrossSectionXY);
		
	}
	return (hr==S_OK);
}

bool SimulCloudRenderer::RenderLightVolume()
{
	HRESULT hr=S_OK;
	D3DVERTEXELEMENT9 decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 12, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
		{ 0, 28, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1 },
		D3DDECL_END()
	};
	if(!m_pHudVertexDecl)
	{
		B_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pHudVertexDecl));
	}

	struct Vertext
	{
		float x,y,z;
		float r,g,b,a;
		float tx,ty;
	};

	m_pd3dDevice->SetTexture(0,NULL);
	
	Vertext *lines=new Vertext[24];

	static float3 vertices[8] =
	{
		{0.f,		0.f,	1.f},
		{1.f,		0.f,	1.f},
		{1.f,		1.f,	1.f},
		{0.f,		1.f,	1.f},
		{0.f,		0.f,	0.f},
		{1.f,		0.f,	0.f},
		{1.f,		1.f,	0.f},
		{0.f,		1.f,	0.f},
	};
	for(int i=0;i<12;i++)
	{
		int i1=0;
		int i2=0;
		if(i<4||i>=8)
		{
			i1=i%4;
			i2=(i+1)%4;
			if(i>=8)
			{
				i1+=4;
				i2+=4;
			}
		}
		else
		{
			i1=i%4;
			i2=i1+4;
		}
		simul::math::Vector3 X1=cloudKeyframer->GetCloudInterface()->ConvertLightCoordsToReal(vertices[i1].x,vertices[i1].y,vertices[i1].z);
		simul::math::Vector3 X2=cloudKeyframer->GetCloudInterface()->ConvertLightCoordsToReal(vertices[i2].x,vertices[i2].y,vertices[i2].z);
		std::swap(X1.y,X1.z);
		std::swap(X2.y,X2.z);
		lines[i*2].x=X1.x; 
		lines[i*2].y=X1.y;  
		lines[i*2].z=X1.z;
		lines[i*2].r=1.f;
		lines[i*2].g=1.f;
		lines[i*2].b=0.f;
		lines[i*2].a=1.f;
		lines[i*2].tx=0;
		lines[i*2].ty=0;
		lines[i*2+1].x=X2.x; 
		lines[i*2+1].y=X2.y;  
		lines[i*2+1].z=X2.z;
		lines[i*2+1].r=1.f;
		lines[i*2+1].g=1.f;
		lines[i*2+1].b=0.f;
		lines[i*2+1].a=1.f;
		lines[i*2+1].tx=0;
		lines[i*2+1].ty=0;
	}
	unsigned passes=0;
	m_pCloudEffect->SetTechnique(m_hTechniqueColourLines);
	hr=m_pCloudEffect->Begin(&passes,0);
	hr=m_pCloudEffect->BeginPass(0);
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST,12,lines,(unsigned)sizeof(Vertext));

	// Now draw the density volume:
	
	for(int i=0;i<12;i++)
	{
		int i1=0;
		int i2=0;
		if(i<4||i>=8)
		{
			i1=i%4;
			i2=(i+1)%4;
			if(i>=8)
			{
				i1+=4;
				i2+=4;
			}
		}
		else
		{
			i1=i%4;
			i2=i1+4;
		}
		simul::math::Vector3 X1=cloudKeyframer->GetCloudInterface()->ConvertTextureCoordsToReal(vertices[i1].x,vertices[i1].y,vertices[i1].z);
		simul::math::Vector3 X2=cloudKeyframer->GetCloudInterface()->ConvertTextureCoordsToReal(vertices[i2].x,vertices[i2].y,vertices[i2].z);
		std::swap(X1.y,X1.z);
		std::swap(X2.y,X2.z);
		lines[i*2].x=X1.x; 
		lines[i*2].y=X1.y;  
		lines[i*2].z=X1.z;
		lines[i*2].r=0.f;
		lines[i*2].g=0.5f;
		lines[i*2].b=1.f;
		lines[i*2].a=1.f;
		lines[i*2+1].x=X2.x; 
		lines[i*2+1].y=X2.y;  
		lines[i*2+1].z=X2.z;
		lines[i*2+1].r=0.f;
		lines[i*2+1].g=0.5f;
		lines[i*2+1].b=1.f;
		lines[i*2+1].a=1.f;
	}
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST,12,lines,(unsigned)sizeof(Vertext));
	hr=m_pCloudEffect->EndPass();
	hr=m_pCloudEffect->End();
	delete [] lines;
	return (hr==S_OK);
}

bool SimulCloudRenderer::RenderDistances(int width,int height)
{
	HRESULT hr=S_OK;
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT4	,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT4	,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0 },
		{ 0, 32, D3DDECLTYPE_FLOAT2	,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
		D3DDECL_END()
	};
	if(!m_pHudVertexDecl)
	{
		B_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pHudVertexDecl));
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
	float max_distance=helper->GetMaxCloudDistance();
	for(std::vector<simul::clouds::CloudGeometryHelper::Slice*>::const_iterator i=helper->GetSlices().begin();i!=helper->GetSlices().end();i++,j++)
	{
		if(!(*i))
			continue;
		float d=(*i)->distance/max_distance*width*.9f;
		lines[j].x=width*0.05f+d; 
		lines[j].y=0.9f*height;  
		lines[j].z=0;
		lines[j].r=1.f;
		lines[j].g=1.f;
		lines[j].b=0.f;
		lines[j].a=(*i)->fadeIn;
		j++;
		lines[j].x=width*0.05f+d; 
		lines[j].y=0.95f*height; 
		lines[j].z=0;
		lines[j].r=1.f;
		lines[j].g=1.f;
		lines[j].b=0.f;
		lines[j].a=(*i)->fadeIn;
	}
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST,(unsigned)helper->GetSlices().size(),lines,(unsigned)sizeof(Vertext));
	delete [] lines;
	return (hr==S_OK);
}
void SimulCloudRenderer::SetLossTextures(void *t1)
{
	sky_loss_texture=(LPDIRECT3DBASETEXTURE9)t1;
}

void SimulCloudRenderer::SetInscatterTextures(void *t1)
{
	sky_inscatter_texture=(LPDIRECT3DBASETEXTURE9)t1;
}

void SimulCloudRenderer::SetFadeMode(FadeMode f)
{
	if(fade_mode!=f)
	{
		BaseCloudRenderer::SetFadeMode(f);
		rebuild_shaders=true;
		InitEffects();
	}
}

void SimulCloudRenderer::SetYVertical(bool y)
{
	if(y_vertical!=y)
	{
		y_vertical=y;
		rebuild_shaders=true;
		helper->SetYVertical(y);
		rebuild_shaders=true;
		InitEffects();
	}
}

const char *SimulCloudRenderer::GetDebugText() const
{
	static char debug_text[256];
	sprintf_s(debug_text,256,"SimulCloudRenderer: %d slices visible",
		helper->GetNumActiveLayers());
	return debug_text;
}