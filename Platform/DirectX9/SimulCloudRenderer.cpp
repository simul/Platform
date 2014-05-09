#define NOMINMAX
// Copyright (c) 2007-2014 Simul Software Ltd
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
#include "Simul/Clouds/lightningRenderInterface.h"

using namespace simul;
using namespace dx9;

static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;

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


#include "CreateDX9Effect.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Macros.h"
#include "Resources.h"


#define MAX_VERTICES (12000)


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
			val=std::max(1.f,val);

		return val;
	}
};
ExampleHumidityCallback hum_callback;
MushroomHumidityCallback mushroom_callback;
#endif


SimulCloudRenderer::SimulCloudRenderer(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem)
	:simul::clouds::BaseCloudRenderer(ck,mem)
	,m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,m_pHudVertexDecl(NULL)
	,m_pCloudEffect(NULL)
	,raytrace_layer_texture(NULL)
	,illumination_texture(NULL)
	,sky_loss_texture(NULL)
	,sky_inscatter_texture(NULL)
	,skylight_texture(NULL)
	,unitSphereVertexBuffer(NULL)
	,unitSphereIndexBuffer(NULL)
	,cloudScales(NULL)
	,cloudOffset(NULL)
	,timing(0.f)
	,vertices(NULL)
	,cpu_fade_vertices(NULL)
	,last_time(0)
	,NumBuffers(1)
	,rebuild_shaders(true)
{
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);

	//cloudNode->AddHumidityCallback(&hum_callback);
	GetCloudInterface()->Generate();

	// A noise filter improves the shape of the clouds:
	//cloudNode->GetNoiseInterface()->SetFilter(&circle_f);
}

void SimulCloudRenderer::EnableFilter(bool )
{
/*	if(f)
		cloudNode->GetNoiseInterface()->SetFilter(&circle_f);
	else
		cloudNode->GetNoiseInterface()->SetFilter(NULL);*/
}

void SimulCloudRenderer::RestoreDeviceObjects(void *dev)
{
	simul::base::Timer timer;
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	delete [] vertices;
	vertices=new Vertex_t[MAX_VERTICES];
	//gpuCloudGenerator.RestoreDeviceObjects(dev);
	last_time=0.f;
	// create the unit-sphere vertex buffer determined by the Cloud Geometry Helper:
	SAFE_RELEASE(raytrace_layer_texture);
	
	V_CHECK(D3DXCreateTexture(m_pd3dDevice,128,1,1,0,D3DFMT_A32B32G32R32F,d3d_memory_pool,&raytrace_layer_texture))
		
	float create_raytrace_layer=timer.UpdateTime()/1000.f;

	SAFE_RELEASE(unitSphereVertexBuffer);
	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(0);

	std::vector<vec3> vertices;
	std::vector<unsigned int> indices;
	clouds::CloudGeometryHelper::GenerateSphereVertices(vertices,indices,12,15);


	V_CHECK(m_pd3dDevice->CreateVertexBuffer((unsigned)(vertices.size()*sizeof(PosVert_t)),D3DUSAGE_WRITEONLY,0,
									  D3DPOOL_DEFAULT, &unitSphereVertexBuffer,NULL));
	PosVert_t *unit_sphere_vertices;
	if(unitSphereVertexBuffer)
	{
		V_CHECK(unitSphereVertexBuffer->Lock(0,sizeof(PosVert_t),(void**)&unit_sphere_vertices,0 ));
		PosVert_t *V=unit_sphere_vertices;
		for(size_t i=0;i<vertices.size();i++)
		{
			const vec3 &v=vertices[i];
			V->position.x=v.x;
			V->position.y=v.y;
			V->position.z=v.z;
			V++;
		}
		V_CHECK(unitSphereVertexBuffer->Unlock());
	}
	unsigned num_indices=(unsigned)indices.size();
	SAFE_RELEASE(unitSphereIndexBuffer);
	if(num_indices)
	{
		V_CHECK(m_pd3dDevice->CreateIndexBuffer(num_indices*sizeof(unsigned),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
			D3DPOOL_DEFAULT, &unitSphereIndexBuffer, NULL ));
		unsigned *indexData;
		V_CHECK(unitSphereIndexBuffer->Lock(0,num_indices, (void**)&indexData, 0 ));
		unsigned *index=indexData;
		for(unsigned i=0;i<num_indices;i++)
		{
			*(index++)	=indices[i];
		}
		V_CHECK(unitSphereIndexBuffer->Unlock());
	}
	float create_unit_sphere=timer.UpdateTime()/1000.f;

	D3DVERTEXELEMENT9 decl[]=
	{
		{0, 0	,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0, 12	,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0, 20	,D3DDECLTYPE_FLOAT1,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0, 24	,D3DDECLTYPE_FLOAT1,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pHudVertexDecl);
	V_CHECK(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl))
	
	// NOW can set the rendercallback, as we have a device to implement the callback fns with:
//cloudKeyframer->SetRenderCallback(this);
	float set_callback=timer.UpdateTime()/1000.f;
	std::cout<<std::setprecision(4)
		<<"CLOUDS RESTORE TIMINGS: create_raytrace_layer="<<create_raytrace_layer
		<<"\n\tcreate_unit_sphere="<<create_unit_sphere

		<<"\n\tset_callback="<<set_callback<<std::endl;

	//cloudKeyframer->SetGpuCloudGenerator(&gpuCloudGenerator);
	ClearIterators();
}

/*static std::string GetCompiledFilename(int wrap_clouds)
{
	std::string compiled_filename="simul_clouds_and_lightning_";
	if(wrap_clouds)
		compiled_filename+="w1_";
	else
		compiled_filename+="w0_";
	compiled_filename+=".fxo";
	return compiled_filename;
}*/

void SimulCloudRenderer::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	//gpuCloudGenerator.RecompileShaders();
	wrap=GetCloudInterface()->GetWrap();
	simul::base::Timer timer;
	std::map<std::string,std::string> defines=MakeDefinesList(GetCloudInterface()->GetWrap(),y_vertical);
	float defines_time=timer.UpdateTime()/1000.f;
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pCloudEffect,"simul_clouds.fx",defines));
	float clouds_effect=timer.UpdateTime()/1000.f;
	
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
		lightningMultipliers	=m_pCloudEffect->GetParameterByName(NULL,"lightningMultipliers");
		lightningColour			=m_pCloudEffect->GetParameterByName(NULL,"lightningColour");
		illuminationOrigin		=m_pCloudEffect->GetParameterByName(NULL,"illuminationOrigin");
		illuminationScales		=m_pCloudEffect->GetParameterByName(NULL,"illuminationScales");
	}

	cloudDensity1				=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity1");
	cloudDensity2				=m_pCloudEffect->GetParameterByName(NULL,"cloudDensity2");
	noiseTexture				=m_pCloudEffect->GetParameterByName(NULL,"noiseTexture");

	lightningIlluminationTexture=m_pCloudEffect->GetParameterByName(NULL,"lightningIlluminationTexture");
	skyLossTexture				=m_pCloudEffect->GetParameterByName(NULL,"skyLossTexture");
	skyInscatterTexture			=m_pCloudEffect->GetParameterByName(NULL,"skyInscatterTexture");
	skylightTexture				=m_pCloudEffect->GetParameterByName(NULL,"skylightTexture");
	illuminationTexture			=m_pCloudEffect->GetParameterByName(NULL,"illuminationTexture");
	
	invViewProj			=m_pCloudEffect->GetParameterByName(NULL,"invViewProj");
	//noiseMatrix			=m_pCloudEffect->GetParameterByName(NULL,"noiseMatrix");
	fractalRepeatLength	=m_pCloudEffect->GetParameterByName(NULL,"fractalRepeatLength");
	cloudScales			=m_pCloudEffect->GetParameterByName(NULL,"cloudScales");
	cloudOffset			=m_pCloudEffect->GetParameterByName(NULL,"cloudOffset");
	raytraceLayerTexture=m_pCloudEffect->GetParameterByName(NULL,"raytraceLayerTexture");

	float params=timer.UpdateTime()/1000.f;
	rebuild_shaders=false;
	std::cout<<std::setprecision(4)<<"CLOUDS RELOAD SHADERS TIMINGS: defines="<<defines_time
		<<"\n\tclouds_effect="<<clouds_effect
		<<"\n\ttechniques="<<techniques
		<<"\n\tparams="<<params<<std::endl;

}

void SimulCloudRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(raytrace_layer_texture);
	if(m_pCloudEffect)
        hr=m_pCloudEffect->OnLostDevice();
//gpuCloudGenerator.InvalidateDeviceObjects();

	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pHudVertexDecl);
	SAFE_RELEASE(m_pCloudEffect);
	for(int i=0;i<3;i++)
		SAFE_RELEASE(cloud_textures[i]);
	noise_fb.InvalidateDeviceObjects();
	SAFE_RELEASE(unitSphereVertexBuffer);
	SAFE_RELEASE(unitSphereIndexBuffer);
	delete vertices;
	vertices=NULL;
	//SAFE_RELEASE(large_scale_cloud_texture);
	cloud_tex_width_x=0;
	cloud_tex_length_y=0;
	cloud_tex_depth_z=0;
	ClearIterators();
	rebuild_shaders=true;
	delete [] vertices;
	vertices=NULL;
	noise_fb.InvalidateDeviceObjects();
}

SimulCloudRenderer::~SimulCloudRenderer()
{
	InvalidateDeviceObjects();
}

void SimulCloudRenderer::CreateNoiseTexture(void *)
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return;
	
	LPDIRECT3DSURFACE9				pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9				pNoiseRenderTarget=NULL;
	LPD3DXEFFECT					pRenderNoiseEffect=NULL;

	CreateDX9Effect(m_pd3dDevice,pRenderNoiseEffect,"simul_rendernoise.fx");
	D3DXHANDLE inputTexture		=pRenderNoiseEffect->GetParameterByName(NULL,"noiseTexture");
	D3DXHANDLE octaves			=pRenderNoiseEffect->GetParameterByName(NULL,"octaves");
	
	D3DXHANDLE persistence		=pRenderNoiseEffect->GetParameterByName(NULL,"persistence");
	//
	int noise_texture_size		=cloudKeyframer->GetEdgeNoiseTextureSize();
	int noise_texture_frequency	=cloudKeyframer->GetEdgeNoiseFrequency();
	int texture_octaves			=cloudKeyframer->GetEdgeNoiseOctaves();
	float texture_persistence	=cloudKeyframer->GetEdgeNoisePersistence();
	// Make the input texture:
	simul::dx9::Framebuffer	random_fb;
	{
		unsigned passes;
		random_fb.RestoreDeviceObjects(m_pd3dDevice);
		random_fb.SetWidthAndHeight(noise_texture_frequency,noise_texture_frequency);
		random_fb.SetFormat((int)D3DFMT_A32B32G32R32F);
		pRenderNoiseEffect->SetTechnique(pRenderNoiseEffect->GetTechniqueByName("random"));
		random_fb.Activate(NULL);
			pRenderNoiseEffect->Begin(&passes,0);
			pRenderNoiseEffect->BeginPass(0);
			simul::dx9::DrawQuad(m_pd3dDevice);
			pRenderNoiseEffect->EndPass();
			pRenderNoiseEffect->End();
		random_fb.Deactivate(NULL);
	//..	random_fb.InvalidateDeviceObjects();
	}
	{
		unsigned passes;
		noise_fb.RestoreDeviceObjects(m_pd3dDevice);
		noise_fb.SetWidthAndHeight(noise_texture_size,noise_texture_size);
		noise_fb.SetFormat((int)D3DFMT_A8R8G8B8);
		noise_fb.Activate(NULL);
		hr=m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xAA88FF88,1.f,0L);
	pRenderNoiseEffect->SetTechnique(pRenderNoiseEffect->GetTechniqueByName("noise"));
	pRenderNoiseEffect->SetTexture(inputTexture,(LPDIRECT3DTEXTURE9)random_fb.GetColorTex());
	pRenderNoiseEffect->SetFloat(persistence,texture_persistence);
	int size=(int)(log((float)noise_texture_size)/log(2.f));
	int freq=(int)(log((float)noise_texture_frequency)/log(2.f));
	texture_octaves=size-freq;

	pRenderNoiseEffect->SetInt(octaves,texture_octaves);
			pRenderNoiseEffect->Begin(&passes,0);
			pRenderNoiseEffect->BeginPass(0);
			simul::dx9::DrawQuad(m_pd3dDevice);
			pRenderNoiseEffect->EndPass();
			pRenderNoiseEffect->End();
		noise_fb.Deactivate(NULL);
	}
	
	D3DXSaveTextureToFile(TEXT("Media/Textures/noise.dds"),D3DXIFF_DDS,(LPDIRECT3DTEXTURE9)noise_fb.GetColorTex(),NULL);
	//noise_texture->GenerateMipSubLevels();
D3DXSaveTextureToFile(TEXT("Media/Textures/noise.jpg"),D3DXIFF_JPG,(LPDIRECT3DTEXTURE9)noise_fb.GetColorTex(),NULL);
	SAFE_RELEASE(pOldRenderTarget);

	SAFE_RELEASE(pNoiseRenderTarget);
	SAFE_RELEASE(pRenderNoiseEffect);
};

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

void SimulCloudRenderer::PreRenderUpdate(void *)
{
}
void SimulCloudRenderer::EnsureCorrectIlluminationTextureSizes()
{
}

void SimulCloudRenderer::EnsureCorrectTextureSizes()
{
	simul::sky::int3 i=cloudKeyframer->GetTextureSizes();
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

void SimulCloudRenderer::EnsureTexturesAreUpToDate(void* context)
{
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	if(FailedNoiseChecksum())
		noise_fb.InvalidateDeviceObjects();
	if(!noise_fb.GetColorTex())
		CreateNoiseTexture(context);
	for(int i=0;i<3;i++)
	{
		simul::sky::seq_texture_fill texture_fill=cloudKeyframer->GetSequentialTextureFill(seq_texture_iterator[i]);
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
}

bool SimulCloudRenderer::Render(void *context,float exposure,bool cubemap,bool /*near_pass*/,const void *depth_alpha_tex,bool default_fog,bool write_alpha,int viewport_id,const simul::sky::float4&,const simul::sky::float4& )
{
	if(rebuild_shaders)
		RecompileShaders();
	EnsureTexturesAreUpToDate(context);
	depth_alpha_tex;
	default_fog;
	if(!write_alpha)
		m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,7);
	PIXBeginNamedEvent(0xFF00FFFF,"SimulCloudRenderer::Render");
	if(wrap!=GetCloudInterface()->GetWrap())
	{
		rebuild_shaders=true;
	}
	if(rebuild_shaders)
		RecompileShaders();
	HRESULT hr=S_OK;
	enable_lightning=cloudKeyframer->GetInterpolatedKeyframe().lightning>0;
	// Disable any in-texture gamma-correction that might be lingering from some other bit of rendering:
	/*m_pd3dDevice->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0);*/
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	static D3DTEXTUREADDRESS wrap_u=D3DTADDRESS_WRAP,wrap_v=D3DTADDRESS_WRAP,wrap_w=D3DTADDRESS_CLAMP;

	m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[0]);
	m_pCloudEffect->SetTexture(cloudDensity2				,cloud_textures[1]);
	m_pCloudEffect->SetTexture(noiseTexture					,(LPDIRECT3DTEXTURE9)noise_fb.GetColorTex());

	m_pCloudEffect->SetTexture(skyLossTexture		,sky_loss_texture);
	m_pCloudEffect->SetTexture(skyInscatterTexture	,sky_inscatter_texture);
	m_pCloudEffect->SetTexture(skylightTexture		,skylight_texture);
	m_pCloudEffect->SetTexture(illuminationTexture	,illumination_texture);
	
	// Mess with the proj matrix to extend the far clipping plane? not now
	GetCameraPosVector(view,false,cam_pos);
	simul::math::Vector3 wind_offset=GetCloudInterface()->GetWindOffset();

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);
	
	view_dir.Define(-view._13,-view._23,-view._33);

	float t=skyInterface->GetTime();
	float delta_t=(t-last_time)*cloudKeyframer->GetTimeFactor();
	if(!last_time)
		delta_t=0;
	last_time=t;

	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(viewport_id);
	helper->SetChurn(GetCloudInterface()->GetChurn());
	helper->SetNoFrustumLimit(true);
	helper->Update((const float*)cam_pos,wind_offset,view_dir,up,delta_t,cubemap);

	static float direct_light_mult=0.25f;
	static float indirect_light_mult=0.03f;
	simul::sky::float4 light_response(	exposure*direct_light_mult*GetCloudInterface()->GetLightResponse()
										,exposure*indirect_light_mult*GetCloudInterface()->GetSecondaryLightResponse()
										,0
										,0);
	float base_alt_km=0.001f*(GetCloudInterface()->GetCloudBaseZ());//+.5f*GetCloudInterface()->GetCloudHeight());
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight(base_alt_km);
	
	
	// Get the overall ambient light at this altitude, and multiply it by the cloud's ambient response.
	simul::sky::float4 sky_light_colour=exposure*skyInterface->GetAmbientLight(base_alt_km)*GetCloudInterface()->GetAmbientLightResponse();


	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);

	static bool nofs=false;
	helper->SetNoFrustumLimit(nofs);
	helper->MakeGeometry(GetCloudInterface(),GetCloudGridInterface(),false);
	
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
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
	m_pCloudEffect->SetFloat	(alphaSharpness		,K.edge_sharpness);
	float time=skyInterface->GetTime();
	const simul::clouds::LightningProperties &lightning=cloudKeyframer->GetLightningProperties(time,0);
	
	simul::sky::float4 lightning_colour;
	if(enable_lightning&&lightningRenderInterface)
	{
		static float bb=2.f;
		simul::sky::float4 lightning_multipliers;
		lightning_colour=lightning.colour;
		lightning_colour*=exposure;
		for(int i=0;i<4;i++)
		{
			lightning_multipliers[i]=0;
		}
		static float lightning_effect_on_cloud=20.f;
		lightning_colour.w=lightning_effect_on_cloud;
		m_pCloudEffect->SetVector	(lightningMultipliers	,(D3DXVECTOR4*)(&lightning_multipliers));
		m_pCloudEffect->SetVector	(lightningColour		,(D3DXVECTOR4*)(&lightning_colour));

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

	//InternalRenderHorizontal(viewport_id);
	InternalRenderVolumetric(viewport_id);
	
	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	PIXEndNamedEvent();
	if(!write_alpha)
		m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,15);
	return (hr==S_OK);
}

void SimulCloudRenderer::NumBuffersChanged()
{
}

void SimulCloudRenderer::InternalRenderHorizontal(int )
{
}

bool SimulCloudRenderer::FillRaytraceLayerTexture(int viewport_id)
{
	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(viewport_id);
	D3DLOCKED_RECT lockedRect={0};
	HRESULT hr=S_OK;
	B_RETURN(raytrace_layer_texture->LockRect(0,&lockedRect,NULL,NULL));
	float *float_ptr=(float *)(lockedRect.pBits);
	typedef simul::clouds::SliceVector::const_reverse_iterator iter;
	static float cutoff=100000.f;
	int j=0;
	for(iter i=helper->GetSlices().rbegin();i!=helper->GetSlices().rend()&&j<128;i++,j++)
	{
		float distance=(*i)->layerDistance;
		*float_ptr++=distance;
		*float_ptr++=(*i)->noise_tex_x;
		*float_ptr++=(*i)->noise_tex_y;
		*float_ptr++=0;
	}
	memset(float_ptr,0,4*sizeof(float)*(128-j));
	hr=raytrace_layer_texture->UnlockRect(0);
	return (hr==S_OK);
}

void SimulCloudRenderer::InternalRenderRaytrace(int viewport_id)
{
	FillRaytraceLayerTexture(viewport_id);
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

		hr=m_pCloudEffect->SetTexture(raytraceLayerTexture,raytrace_layer_texture);
		simul::sky::float4 cloud_scales=GetCloudScales();
		simul::sky::float4 cloud_offset=GetCloudOffset();
	
		m_pCloudEffect->SetVector	(cloudScales	,(const D3DXVECTOR4*)&cloud_scales);
		m_pCloudEffect->SetVector	(cloudOffset	,(const D3DXVECTOR4*)&cloud_offset);
		D3DXVECTOR3 d3dcam_pos;
		GetCameraPosVector(view,y_vertical,(float*)&d3dcam_pos);
		float altitude_km=0.001f*(y_vertical?d3dcam_pos.y:d3dcam_pos.z);
		if(skyInterface)
		{
//		hr=m_pCloudEffect->SetFloat(HazeEccentricity,skyInterface->GetMieEccentricity());
			D3DXVECTOR4 mie_rayleigh_ratio(skyInterface->GetMieRayleighRatio());
			D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight(altitude_km));
			D3DXVECTOR4 light_colour(skyInterface->GetLocalIrradiance(altitude_km));
			
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

void SimulCloudRenderer::InternalRenderVolumetric(int viewport_id)
{
	float exposure=1.f;
	CloudConstants cloudConstants;
	CloudPerViewConstants cloudPerViewConstants;
	SetCloudConstants(cloudConstants);
	simul::sky::float4 viewportTextureRegionXYWH(0,0,1.f,1.f);
	simul::sky::float4 mixedResTransformXYWH(0,0,1.f,1.f);
	SetCloudPerViewConstants(cloudPerViewConstants,view,proj,exposure,viewport_id
		,viewportTextureRegionXYWH,mixedResTransformXYWH);
	{
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,viewportToTexRegionScaleBias);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,viewPos);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,invViewProj);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,shadowMatrix);		// Transform from texcoords xy to world viewplane XYZ
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,noiseMatrix);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,depthToLinFadeDistParams);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,exposure);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,tanHalfFov);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,nearZ);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,farZ);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,extentZMetres);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,startZMetres);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,shadowRange);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,shadowTextureSize);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudPerViewConstants,depthMix);
	}
	{
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,inverseScales);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,ambientColour);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,cloud_interp);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,fractalScale);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,cloudEccentricity);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,lightResponse);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,directionToSun);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,earthshadowMultiplier);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,cornerPos);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,hazeEccentricity);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,lightningMultipliers);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,lightningColour);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,lightningSourcePos);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,rain);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,sunlightColour1);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,fractalRepeatLength);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,sunlightColour2);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,maxAltitudeMetres);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,screenCoordOffset);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,mieRayleighRatio);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,alphaSharpness);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,lightningOrigin);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,maxFadeDistanceMetres);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,lightningInvScales);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,noise3DPersistence);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,crossSectionOffset);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,noise3DOctaves);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,noise3DTexcoordScale);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,cloudIrRadiance);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,directionToMoon);
		DX9_STRUCTMEMBER_SET(m_pCloudEffect,cloudConstants,baseNoiseFactor);
	}

	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(viewport_id);

	HRESULT hr=S_OK;
	//set up matrices
	D3DXMATRIX wvp;
//	FixProjectionMatrix(proj,helper->GetMaxCloudDistance()*1.1f,y_vertical);
	//view._41=view._42=view._43=0.f;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);

	D3DXMATRIX tmp1,tmp2,w,ident;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixMultiply(&tmp2,&view,&proj);
	D3DXMatrixTranspose(&wvp,&tmp2);

	m_pCloudEffect->SetMatrix(worldViewProj, &wvp);
	unsigned passes=0;
	hr=m_pCloudEffect->Begin(&passes,0);
	hr=m_pCloudEffect->BeginPass(0);
	int v=0;
	simul::sky::float4 pos;
	float base_alt_km=0.001f*(GetCloudInterface()->GetCloudBaseZ());//+.5f*GetCloudInterface()->GetCloudHeight());
	unsigned grid_x,el_grid;
	helper->GetGrid(el_grid,grid_x);
	simul::sky::float4 view_km=(const float*)cam_pos;
	view_km*=0.001f;
	typedef simul::clouds::SliceVector::const_iterator iter;
	static float cutoff=100000.f;	// Get the sunlight at this altitude:
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(base_alt_km);
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
	float wavelength=cloudKeyframer->GetCloudInterface()->GetFractalRepeatLength(K.fractal_wavelength);
	for(iter i=helper->GetSlices().begin();i!=helper->GetSlices().end();i++)
	{
		float distance=(*i)->layerDistance;
		clouds::SliceInstance s=helper->MakeLayerGeometry(*i,6378000.0f);
		simul::clouds::IntVector &quad_strip_vertices=s.quad_strip_indices;
		size_t qs_vert=0;
		float fade=(*i)->layerFade;
		bool start=true;
		for(simul::clouds::QuadStripVector::const_iterator j=s.quadStrips.begin();j!=s.quadStrips.end();j++)
		{
			bool l=0;
			for(size_t k=0;k<(j)->num_vertices;k++,qs_vert++,l++,v++)
			{
				const simul::clouds::Vertex &V=s.vertices[quad_strip_vertices[qs_vert]];
				pos.Set(V.x,V.y,V.z,0);
				if(v>=MAX_VERTICES)
				{
					break;
				}
				if(start)
				{
					Vertex_t *startvertex=NULL;
					startvertex=&vertices[v];
					startvertex->position=pos;
					v++;
					start=false;
				}
				Vertex_t *vertex=NULL;
				vertex=&vertices[v];
				vertex->position=pos*distance;
				vertex->layerFade=fade;
				vertex->layerNoiseOffset=vec2((*i)->noise_tex_x/wavelength,(*i)->noise_tex_y/wavelength);
				vertex->layerDistance=distance;
			}
		}
		if(v>=MAX_VERTICES)
		{
			break;
		}
		Vertex_t *vertex=NULL;
		vertex=&vertices[v];
		vertex->position=pos;
		v++;
	}
	if(v>2)
	{
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,v-2,&(vertices[0]),sizeof(Vertex_t));
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

bool SimulCloudRenderer::MakeCubemap(void *)
{
	return (false);
}
float SimulCloudRenderer::GetTiming() const
{
	return timing;
}

void *SimulCloudRenderer::GetIlluminationTexture()
{
	return (void *)NULL;
}

void SimulCloudRenderer::SaveCloudTexture(const char *filename)
{
	std::wstring fn=simul::base::StringToWString(filename);
	std::wstring ext=L".png";
	simul::dx9::Framebuffer fb;
	static D3DFORMAT f=D3DFMT_A8R8G8B8;
	fb.SetFormat(f);
	fb.SetWidthAndHeight(cloud_tex_width_x*2,cloud_tex_length_y*cloud_tex_depth_z);
	fb.RestoreDeviceObjects(m_pd3dDevice);
	fb.Activate(NULL);
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

	fb.Deactivate(NULL);
	fn+=ext;
	//hr=D3DXSaveSurfaceToFile((std::wstring(fn)+L".bmp").c_str(),D3DXIFF_BMP,fb.m_pHDRRenderTarget,NULL,NULL);
	SaveTexture((LPDIRECT3DTEXTURE9)fb.GetColorTex(),(std::string(filename)+".png").c_str());
	fb.InvalidateDeviceObjects();
}

void SimulCloudRenderer::RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	static int u=4;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
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
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		D3DXVECTOR4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		m_pCloudEffect->SetVector	(lightResponse		,(D3DXVECTOR4*)(&light_response));
		m_pCloudEffect->SetVector(crossSectionOffset,&cross_section_offset);
		m_pCloudEffect->SetTexture(cloudDensity1,cloud_textures[(i)%3]);
		RenderTexture(m_pd3dDevice,i*(w+1)+4,4,w,h,cloud_textures[(i)%3],m_pCloudEffect,m_hTechniqueCrossSectionXZ);
	}
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		D3DXVECTOR4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		m_pCloudEffect->SetVector(lightResponse		,(D3DXVECTOR4*)(&light_response));
		m_pCloudEffect->SetVector(crossSectionOffset,&cross_section_offset);
		//GetCloudInterface()->GetWrap()?0.5f:0.f);
		m_pCloudEffect->SetTexture(cloudDensity1				,cloud_textures[(i)%3]);
		RenderTexture(m_pd3dDevice,x0+i*(w+1)+4,y0+h+8,w,w,
					  cloud_textures[(i)%3],m_pCloudEffect,m_hTechniqueCrossSectionXY);
		
	}
}

void SimulCloudRenderer::RenderAuxiliaryTextures(crossplatform::DeviceContext &context,int x0,int y0,int width,int height)
{
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
	m_pCloudEffect->SetTexture(noiseTexture,(LPDIRECT3DTEXTURE9)noise_fb.GetColorTex());
	D3DXHANDLE tech			=GetDX9Technique(m_pCloudEffect,"show_noise");
	RenderTexture(m_pd3dDevice
		,x0+width-(w+8),y0+height-(w+8),w,w
		,(LPDIRECT3DTEXTURE9)noise_fb.GetColorTex()
		,m_pCloudEffect,tech);
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

	static float vertices[8][3] =
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
		simul::math::Vector3 X1;//=cloudKeyframer->GetCloudInterface()->ConvertLightCoordsToReal(vertices[i1][0],vertices[i1][1],vertices[i1][2]);
		simul::math::Vector3 X2;//=cloudKeyframer->GetCloudInterface()->ConvertLightCoordsToReal(vertices[i2][0],vertices[i2][1],vertices[i2][2]);
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
		simul::math::Vector3 X1=cloudKeyframer->GetCloudInterface()->ConvertTextureCoordsToReal(vertices[i1][0],vertices[i1][1],vertices[i1][2]);
		simul::math::Vector3 X2=cloudKeyframer->GetCloudInterface()->ConvertTextureCoordsToReal(vertices[i2][0],vertices[i2][1],vertices[i2][2]);
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

void SimulCloudRenderer::DrawLines(void*,VertexXyzRgba *verts,int num,bool strip)
{
	HRESULT hr=S_OK;
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3	,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 12, D3DDECLTYPE_FLOAT4	,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0 },
		D3DDECL_END()
	};
	if(!m_pHudVertexDecl)
	{
		m_pd3dDevice->CreateVertexDeclaration(decl,&m_pHudVertexDecl);
	}
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
	hr=m_pd3dDevice->DrawPrimitiveUP(strip?D3DPT_LINESTRIP:D3DPT_LINELIST,num/2,verts,(unsigned)sizeof(VertexXyzRgba));
}

void SimulCloudRenderer::SetLossTexture(void *t1)
{
	sky_loss_texture=(LPDIRECT3DBASETEXTURE9)t1;
}

void SimulCloudRenderer::SetInscatterTextures(void *,void *s,void *o)
{
	sky_inscatter_texture=(LPDIRECT3DBASETEXTURE9)o;
	skylight_texture=(LPDIRECT3DBASETEXTURE9)s;
}
void SimulCloudRenderer::SetIlluminationTexture(void *i)
{
	illumination_texture=(LPDIRECT3DBASETEXTURE9)i;
}

const char *SimulCloudRenderer::GetDebugText() const
{
	static char debug_text[256];
	sprintf_s(debug_text,256,"SimulCloudRenderer");
	return debug_text;
}