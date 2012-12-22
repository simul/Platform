// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulSkyRenderer.cpp A renderer for skies.
#define NOMINMAX
#include "SimulSkyRenderer.h"

#ifdef XBOX
	#include <a>
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <d3d9.h>
	#include <d3dx9.h>
	#include <dxerr.h>
	#include <string>
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif
#include <fstream>

#include "CreateDX9Effect.h"
#include "Macros.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Sky.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Geometry/Orientation.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/Decay.h"
#include "SaveTexture.h"
#include "Resources.h"

SimulSkyRenderer::SimulSkyRenderer(simul::sky::SkyKeyframer *sk)
	:simul::sky::BaseSkyRenderer(sk)
	,m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,m_pSkyEffect(NULL)
	,m_hTechniqueSky(NULL)
	,m_hTechniqueStarrySky(NULL)
	,m_hTechniquePointStars(NULL)
	,m_hTechniqueQuery(NULL)
	,m_hTechniquePlanet(NULL)
	,m_hTechniqueSun(NULL)
	,max_distance_texture(NULL)
	,m_hTechniqueFadeCrossSection(NULL)
	,m_hTechnique3DTo2DFade(NULL)
	,stars_texture(NULL)
	,d3dQuery(NULL)
	,sky_tex_format(D3DFMT_UNKNOWN)
	,y_vertical(false)
	,maxPixelsVisible(200)
	,screen_pixel_height(0)
{
	MoonTexture="Moon.png";
	for(int i=0;i<3;i++)
	{
		loss_textures[i]=NULL;
		inscatter_textures[i]=NULL;
		skylight_textures[i]=NULL;
		sunlight_textures[i]=NULL;
	}
	skyKeyframer->SetTime(0.5f);
	SetCameraPosition(0,0,400.f);
}

void SimulSkyRenderer::SaveTextures(const char *base_filename)
{
	std::string filename_root(base_filename);
	int pos=filename_root.find_last_of('.');
	std::string ext=filename_root.substr(pos,4);
	filename_root=filename_root.substr(0,pos);
	for(int i=0;i<3;i++)
	{
		std::string filename=filename_root;
		char txt[10];
		sprintf_s(txt,10,"_%d",i);
		filename+=txt;
		filename+=ext;
		//SaveTexture(sky_textures[i],filename.c_str());
	}
}

void SimulSkyRenderer::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	defines["USE_ALTITUDE_INTERPOLATION"]="1";
	if(y_vertical)
		defines["Y_VERTICAL"]="1";
	else
		defines["Z_VERTICAL"]="1";
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pSkyEffect,"simul_sky.fx",defines));
	m_hTechniqueSky				=m_pSkyEffect->GetTechniqueByName("simul_sky");
	m_hTechniqueShowSkyTexture	=m_pSkyEffect->GetTechniqueByName("simul_show_sky_texture");
	m_hTechniqueStarrySky		=m_pSkyEffect->GetTechniqueByName("simul_starry_sky");
	m_hTechniquePointStars		=m_pSkyEffect->GetTechniqueByName("simul_point_stars");

	worldViewProj				=m_pSkyEffect->GetParameterByName(NULL,"worldViewProj");
	lightDirection				=m_pSkyEffect->GetParameterByName(NULL,"lightDir");
	mieRayleighRatio			=m_pSkyEffect->GetParameterByName(NULL,"mieRayleighRatio");
	hazeEccentricity			=m_pSkyEffect->GetParameterByName(NULL,"hazeEccentricity");
	skyInterp					=m_pSkyEffect->GetParameterByName(NULL,"skyInterp");
	altitudeTexCoord			=m_pSkyEffect->GetParameterByName(NULL,"altitudeTexCoord");
	flareTexture				=m_pSkyEffect->GetParameterByName(NULL,"flareTexture");
	colour						=m_pSkyEffect->GetParameterByName(NULL,"colour");
	m_hTechniqueSun				=m_pSkyEffect->GetTechniqueByName("simul_sun");
	m_hTechniquePlanet			=m_pSkyEffect->GetTechniqueByName("simul_planet");

	inscTexture					=m_pSkyEffect->GetParameterByName(NULL,"inscTexture");
	skylTexture					=m_pSkyEffect->GetParameterByName(NULL,"skylTexture");
	starsTexture				=m_pSkyEffect->GetParameterByName(NULL,"starsTexture");
	starBrightness				=m_pSkyEffect->GetParameterByName(NULL,"starBrightness");

	m_hTechniqueQuery			=m_pSkyEffect->GetTechniqueByName("simul_query");
	m_hTechniqueFadeCrossSection=m_pSkyEffect->GetTechniqueByName("simul_fade_cross_section_xz");
	m_hTechniqueShowFade		=m_pSkyEffect->GetTechniqueByName("simul_show_fade");
	m_hTechnique3DTo2DFade		=m_pSkyEffect->GetTechniqueByName("simul_fade_3d_to_2d");
	fadeTexture					=m_pSkyEffect->GetParameterByName(NULL,"fadeTexture");
	fadeTexture2				=m_pSkyEffect->GetParameterByName(NULL,"fadeTexture2");
	texelOffset					=m_pSkyEffect->GetParameterByName(NULL,"texelOffset");
	texelScale					=m_pSkyEffect->GetParameterByName(NULL,"texelScale");
	fadeTexture2D				=m_pSkyEffect->GetParameterByName(NULL,"fadeTexture2D");
}

void SimulSkyRenderer::RestoreDeviceObjects(void *dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
#ifndef XBOX
	sky_tex_format=D3DFMT_A16B16G16R16F;
#else
	sky_tex_format=D3DFMT_LIN_A16B16G16R16F;
#endif
	hr=CanUse16BitFloats(m_pd3dDevice);

	//if(hr!=S_OK)
	{
#ifndef XBOX
		sky_tex_format=D3DFMT_A32B32G32R32F;
#else
		sky_tex_format=D3DFMT_LIN_A32B32G32R32F;
#endif
		if(CanUseTexFormat(m_pd3dDevice,sky_tex_format)!=S_OK)
			throw simul::base::RuntimeError("Can't use sky texture format.");
	}
    m_pd3dDevice->CreateQuery( D3DQUERYTYPE_OCCLUSION, &d3dQuery );
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);
	SAFE_RELEASE(m_pSkyEffect);
	RecompileShaders();
	// CreateSkyTexture() will be called back

	// OK to fail with these (more or less):
	SAFE_RELEASE(stars_texture);
	//CreateDX9Texture(m_pd3dDevice,stars_texture,"TychoSkymapII.t5_04096x02048.jpg");

	SAFE_RELEASE((LPDIRECT3DTEXTURE9&)moon_texture);
	CreateDX9Texture(m_pd3dDevice,(LPDIRECT3DTEXTURE9&)moon_texture,MoonTexture.c_str());
	SetPlanetImage(moon_index,moon_texture);
	screen_pixel_height=0;
	ClearIterators();
}

int SimulSkyRenderer::CalcScreenPixelHeight()
{
	LPDIRECT3DSURFACE9 backBuffer;
	m_pd3dDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backBuffer);
	D3DSURFACE_DESC backBufferDesc;
	backBuffer->GetDesc(&backBufferDesc);
	SAFE_RELEASE(backBuffer);
	return backBufferDesc.Height;
}

void SimulSkyRenderer::DrawLines(Vertext *lines,int vertex_count,bool strip)
{
	RT::DrawLines((RT::VertexXyzRgba*)lines,vertex_count,strip);
}

void SimulSkyRenderer::PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	RT::PrintAt3dPos(p,text,colr,offsetx,offsety);
}

void SimulSkyRenderer::InvalidateDeviceObjects()
{
	screen_pixel_height=0;
	loss_2d.InvalidateDeviceObjects();
	inscatter_2d.InvalidateDeviceObjects();
	skylight_2d.InvalidateDeviceObjects();
	
	HRESULT hr=S_OK;
	delete [] star_vertices;
	star_vertices=NULL;
	sky_tex_format=D3DFMT_UNKNOWN;
	if(m_pSkyEffect)
        hr=m_pSkyEffect->OnLostDevice();
	SAFE_RELEASE(max_distance_texture);
	SAFE_RELEASE(m_pSkyEffect);
	SAFE_RELEASE(m_pVtxDecl);
	
	SAFE_RELEASE(stars_texture);
	SAFE_RELEASE((LPDIRECT3DTEXTURE9&)moon_texture);
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(inscatter_textures[i]);
		SAFE_RELEASE(skylight_textures[i]);
		SAFE_RELEASE(sunlight_textures[i]);
	}
	numFadeDistances=numFadeElevations=numAltitudes=0;
	SAFE_RELEASE(d3dQuery);
}

SimulSkyRenderer::~SimulSkyRenderer()
{
	InvalidateDeviceObjects();
}

void SimulSkyRenderer::FillDistanceTexture(int num_elevs_width,int num_alts_height,const float *dist_array)
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(max_distance_texture);
	HRESULT hr=D3DXCreateTexture(m_pd3dDevice,num_elevs_width,num_alts_height,1,0,D3DFMT_R32F,d3d_memory_pool,&max_distance_texture);

	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=max_distance_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return;
	float *float_ptr=(float *)(lockedRect.pBits);
	for(int i=0;i<num_alts_height*num_elevs_width;i++)
		*float_ptr++=(*dist_array++);
	hr=max_distance_texture->UnlockRect(0);
}

void SimulSkyRenderer::FillSunlightTexture(int texture_index,int texel_index,int num_texels,const float *float4_array)
{
	HRESULT hr;
	LPDIRECT3DTEXTURE9 tex=NULL;
	tex=sunlight_textures[texture_index];
	if(!tex)
		return;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=tex->LockRect(0,&lockedRect,NULL,NULL)))
		return;
	if(sky_tex_format==D3DFMT_A16B16G16R16F)
	{
		// Convert the array of floats into float16 values for the texture.
		short *short_ptr=(short *)(lockedRect.pBits);
		short_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*float4_array++);
	}
	else
	{
		// Convert the array of floats into float16 values for the texture.
		float *float_ptr=(float *)(lockedRect.pBits);
		float_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*float_ptr++=(*float4_array++);
	}
	hr=tex->UnlockRect(0);
}

void SimulSkyRenderer::Get2DLossAndInscatterTextures(void* *l1,void* *i1,void* *s1)
{
	*l1=(void*)loss_2d.GetColorTex();
	*i1=(void*)inscatter_2d.GetColorTex();
	*s1=(void*)skylight_2d.GetColorTex();
}

float SimulSkyRenderer::GetFadeInterp() const
{
	return skyKeyframer->GetInterpolation();
}
void SimulSkyRenderer::SetStepsPerDay(int s)
{
	skyKeyframer->SetUniformKeyframes(s);
}

void SimulSkyRenderer::CreateFadeTextures()
{
	HRESULT hr;
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(inscatter_textures[i]);
		SAFE_RELEASE(skylight_textures[i]);
	}
	for(int i=0;i<3;i++)
	{
		LPDIRECT3DVOLUMETEXTURE9 tex1,tex2,tex3;
		hr=D3DXCreateVolumeTexture(m_pd3dDevice,numAltitudes,numFadeElevations,numFadeDistances,1,0,sky_tex_format,d3d_memory_pool,&tex1);
		hr=D3DXCreateVolumeTexture(m_pd3dDevice,numAltitudes,numFadeElevations,numFadeDistances,1,0,sky_tex_format,d3d_memory_pool,&tex2);
		hr=D3DXCreateVolumeTexture(m_pd3dDevice,numAltitudes,numFadeElevations,numFadeDistances,1,0,sky_tex_format,d3d_memory_pool,&tex3);
		loss_textures[i]=tex1;
		inscatter_textures[i]=tex2;
		skylight_textures[i]=tex3;
	}
	loss_2d.InvalidateDeviceObjects();
	inscatter_2d.InvalidateDeviceObjects();
	skylight_2d.InvalidateDeviceObjects();
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.RestoreDeviceObjects(m_pd3dDevice);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.RestoreDeviceObjects(m_pd3dDevice);
	skylight_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	skylight_2d.RestoreDeviceObjects(m_pd3dDevice);
}

void SimulSkyRenderer::FillFadeTexturesSequentially(int texture_index,int texel_index,int num_texels,
						const float *loss_float4_array,
						const float *inscatter_float4_array,
						const float *skyl_float4_array
						)
{
	HRESULT hr=S_OK;
	D3DLOCKED_BOX lockedBox={0};
	D3DVOLUME_DESC desc3d;
	LPDIRECT3DVOLUMETEXTURE9 tex3d=NULL;
	void *tex_ptr=NULL;
	{
		tex3d=(LPDIRECT3DVOLUMETEXTURE9)loss_textures[texture_index];
		tex3d->GetLevelDesc(0,&desc3d);
		if(!tex3d)
			return;
		if(FAILED(hr=tex3d->LockBox(0,&lockedBox,NULL,NULL)))
			return;
		tex_ptr=lockedBox.pBits;
	}
	// Convert the array of floats into float16 values for the texture.
	if(sky_tex_format==D3DFMT_A16B16G16R16F)
	{
		short *short_ptr=(short *)(tex_ptr);
		short_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*loss_float4_array++);
	}
	else
	{
		// Copy the array of floats into the texture.
		float *float_ptr=(float *)(tex_ptr);
		float_ptr+=4*texel_index;
		memcpy(float_ptr,loss_float4_array,4*num_texels*sizeof(float));
	}
	{
		hr=tex3d->UnlockBox(0);
		tex3d=(LPDIRECT3DVOLUMETEXTURE9)inscatter_textures[texture_index];
		tex3d->GetLevelDesc(0,&desc3d);
		if(!tex3d)
			return;
		if(FAILED(hr=tex3d->LockBox(0,&lockedBox,NULL,NULL)))
			return;
		tex_ptr=lockedBox.pBits;
	}
	// Convert the array of floats into float16 values for the texture.
	if(sky_tex_format==D3DFMT_A16B16G16R16F)
	{
		short *short_ptr=(short *)(tex_ptr);
		short_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*inscatter_float4_array++);
	}
	else
	{
		// Convert the array of floats into float16 values for the texture.
		float *float_ptr=(float *)(tex_ptr);
		float_ptr+=4*texel_index;
		memcpy(float_ptr,inscatter_float4_array,4*num_texels*sizeof(float));
	}
	{
		hr=tex3d->UnlockBox(0);
		tex3d=(LPDIRECT3DVOLUMETEXTURE9)skylight_textures[texture_index];
		tex3d->GetLevelDesc(0,&desc3d);
		if(!tex3d)
			return;
		if(FAILED(hr=tex3d->LockBox(0,&lockedBox,NULL,NULL)))
			return;
		tex_ptr=lockedBox.pBits;
	}
	// Convert the array of floats into float16 values for the texture.
	if(sky_tex_format==D3DFMT_A16B16G16R16F)
	{
		short *short_ptr=(short *)(tex_ptr);
		short_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*skyl_float4_array++);
	}
	else
	{
		// Convert the array of floats into float16 values for the texture.
		float *float_ptr=(float *)(tex_ptr);
		float_ptr+=4*texel_index;
		memcpy(float_ptr,skyl_float4_array,4*num_texels*sizeof(float));
	}
	hr=tex3d->UnlockBox(0);

}

void SimulSkyRenderer::CycleTexturesForward()
{
/*	std::swap(loss_textures[0],loss_textures[1]);
	std::swap(loss_textures[1],loss_textures[2]);
	std::swap(inscatter_textures[0],inscatter_textures[1]);
	std::swap(inscatter_textures[1],inscatter_textures[2]);
	std::swap(sunlight_textures[0],sunlight_textures[1]);
	std::swap(sunlight_textures[1],sunlight_textures[2]);*/
}


void SimulSkyRenderer::CreateSunlightTextures()
{
	HRESULT hr=S_OK;
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sunlight_textures[i]);
	}
	for(int i=0;i<3;i++)
	{
		if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,numAltitudes,1,1,0,sky_tex_format,d3d_memory_pool,&sunlight_textures[i])))
			return;
	}
}

struct Vertex_t
{
	float x,y,z;
};
	static const float size=250.f;
	static Vertex_t vertices[36] =
	{
		{-size,		-size,	size},
		{size,		-size,	size},
		{size,		size,	size},
		{size,		size,	size},
		{-size,		size,	size},
		{-size,		-size,	size},
		
		{-size,		-size,	-size},
		{size,		-size,	-size},
		{size,		size,	-size},
		{size,		size,	-size},
		{-size,		size,	-size},
		{-size,		-size,	-size},
		
		{-size,		size,	-size},
		{size,		size,	-size},
		{size,		size,	size},
		{size,		size,	size},
		{-size,		size,	size},
		{-size,		size,	-size},
					
		{-size,		-size,  -size},
		{size,		-size,	-size},
		{size,		-size,	size},
		{size,		-size,	size},
		{-size,		-size,	size},
		{-size,		-size,  -size},
		
		{size,		-size,	-size},
		{size,		size,	-size},
		{size,		size,	size},
		{size,		size,	size},
		{size,		-size,	size},
		{size,		-size,	-size},
					
		{-size,		-size,	-size},
		{-size,		size,	-size},
		{-size,		size,	size},
		{-size,		size,	size},
		{-size,		-size,	size},
		{-size,		-size,	-size},
	};

bool SimulSkyRenderer::RenderAngledQuad(D3DXVECTOR4 dir,float half_angle_radians)
{
	// If y is vertical, we have LEFT-HANDED rotations, otherwise right.
	// But D3DXMatrixRotationYawPitchRoll uses only left-handed, hence the change of sign below.
	float Yaw=atan2(dir.x,y_vertical?dir.z:dir.y);
	float Pitch=-asin(y_vertical?dir.y:dir.z);
 	HRESULT hr=S_OK;
	D3DXMATRIX tmp1,tmp2;
	D3DXMatrixIdentity(&world);
	static D3DXMATRIX flip(1.f,0,0,0,0,0,1.f,0,0,1.f,0,0,0,0,0,1.f);
	if(y_vertical)
	{
		D3DXMatrixRotationYawPitchRoll(
			  &world,
			  Yaw,
			  Pitch,
			  0
			);
	}
	else
	{
		simul::geometry::SimulOrientation or;
		or.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
		or.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
		world=*((const D3DXMATRIX*)(or.T4.RowPointer(0)));
	}
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToSun());
	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	D3DXVECTOR4 sun2;
	D3DXMATRIX inv_world;
	D3DXMatrixInverse(&inv_world,NULL,&world);
	D3DXVec4Transform(  &sun2,
						  &sun_dir,
						  &inv_world);
	m_pSkyEffect->SetVector(lightDirection,&sun2);

	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));
	struct Vertext
	{
		float x,y,z;
		float tx,ty;
	};
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	static float w=1.f;
	float d=w/tan(half_angle_radians);
	Vertext vertices[4] =
	{
		{ w,-w,	d, 1.f	,0.f},
		{ w, w,	d, 1.f	,1.f},
		{-w, w,	d, 0.f	,1.f},
		{-w,-w,	d, 0.f	,0.f},
	};
	m_pd3dDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX0);
	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    hr=m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
     
	UINT passes=1;
	hr=m_pSkyEffect->Begin(&passes,0);
	for(unsigned i=0;i<passes;++i)
	{
		hr=m_pSkyEffect->BeginPass(i);
		m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
		hr=m_pSkyEffect->EndPass();
	}
	hr=m_pSkyEffect->End();
	return (hr==S_OK);
}
int query_issued=0;
static float query_occlusion=0.f;
float SimulSkyRenderer::CalcSunOcclusion(float cloud_occlusion)
{
	if(!screen_pixel_height)
		screen_pixel_height=CalcScreenPixelHeight();
	if(!screen_pixel_height)
		return cloud_occlusion;
	sun_occlusion=1.f-(1.f-cloud_occlusion)*(1.f-query_occlusion);
	static float uu=1000.f;
	float retain=1.f/(1.f+uu*0.01f);
	float introduce=1.f-retain;
	if(!m_hTechniqueQuery)
		return sun_occlusion;
	m_pSkyEffect->SetTechnique(m_hTechniqueQuery);
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToLight(cam_pos.z*.001f));
	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	// fix the projection matrix so this quad is far away:
	D3DXMATRIX tmp=proj;
	static float ff=0.0001f;
	float zFar=(1.f+ff)/tan(sun_angular_size);
	FixProjectionMatrix(proj,zFar*ff,zFar,IsYVertical());
	HRESULT hr;
	// if the whole quad was visible, how many pixels would it be?
	float screen_angular_size=2.f*atan((float)proj._22);
	float screen_pixel_size=(float)screen_pixel_height;
	float pixel_diameter=screen_pixel_size*sun_angular_size/screen_angular_size;
	maxPixelsVisible=3.14159f*pixel_diameter*pixel_diameter;
	// Start the query
	if(!query_issued)
	{
		hr=d3dQuery->Issue(D3DISSUE_BEGIN);
		hr=RenderAngledQuad(sun_dir,sun_angular_size);
		query_issued=1;
		// End the query, get the data
		if(query_issued==1)
    		hr=d3dQuery->Issue(D3DISSUE_END);
	}
	if(query_issued==1)
	{
		query_issued=2;
    	// Loop until the data becomes available
    	DWORD pixelsVisible=0;
		HRESULT hr=d3dQuery->GetData((void *)&pixelsVisible,sizeof(DWORD),0);
		if(hr==S_OK)//D3DGETDATA_FLUSH
		{
			
			float q=(1.f-(float)pixelsVisible/maxPixelsVisible);
			if(q<0)
				q=0;
			query_occlusion*=retain;
			query_occlusion+=introduce*q;
			query_issued=0;
		}
	}
	proj=tmp;
	return sun_occlusion;
}
float sun_angular_size=3.14159f/180.f/2.f;

void SimulSkyRenderer::RenderSun(float exposure_hint)
{
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	simul::sky::float4 sunlight=skyKeyframer->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=pow(1.f-sun_occlusion,0.25f)*2700.f;
	// But to avoid artifacts like aliasing at the edges, we will rescale the colour itself
	// to the range [0,1], and store a brightness multiplier in the alpha channel!
	sunlight.w=1.f;
	float max_bright=std::max(std::max(sunlight.x,sunlight.y),sunlight.z);
	float maxout_brightness=2.f/exposure_hint;
	if(maxout_brightness>1e6f)
		maxout_brightness=1e6f;
	if(maxout_brightness<1e-6f)
		maxout_brightness=1e-6f;
	if(max_bright>maxout_brightness)
	{
		sunlight*=maxout_brightness/max_bright;
		sunlight.w=max_bright;
	}
	m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&sunlight));
	m_pSkyEffect->SetTechnique(m_hTechniqueSun);
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToLight(cam_pos.z*0.001f));
	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	RenderAngledQuad(sun_dir,sun_angular_size);
}

void SimulSkyRenderer::SetYVertical(bool y)
{
	if(y!=y_vertical)
	{
		y_vertical=y;
		RecompileShaders();
	}
}

void SimulSkyRenderer::EnsureCorrectTextureSizes()
{
	if(!m_pd3dDevice)
		return;
	simul::sky::BaseSkyRenderer::EnsureCorrectTextureSizes();
}

void SimulSkyRenderer::EnsureTexturesAreUpToDate()
{
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		simul::sky::BaseKeyframer::seq_texture_fill texture_fill=skyKeyframer->GetSequentialFadeTextureFill(i,fade_texture_iterator[i]);
			if(texture_fill.num_texels)
			{
			FillFadeTexturesSequentially(i,texture_fill.texel_index,texture_fill.num_texels
									,(const float*)texture_fill.float_array_1
									,(const float*)texture_fill.float_array_2
									,(const float*)texture_fill.float_array_3);
			}
		}
}

void SimulSkyRenderer::EnsureTextureCycle()
{
	int cyc=(skyKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(loss_textures[0],loss_textures[1]);
		std::swap(loss_textures[1],loss_textures[2]);
		std::swap(inscatter_textures[0],inscatter_textures[1]);
		std::swap(inscatter_textures[1],inscatter_textures[2]);
		std::swap(skylight_textures[0],skylight_textures[1]);
		std::swap(skylight_textures[1],skylight_textures[2]);
		std::swap(sunlight_textures[0],sunlight_textures[1]);
		std::swap(sunlight_textures[1],sunlight_textures[2]);
		std::swap(fade_texture_iterator[0],fade_texture_iterator[1]);
		std::swap(fade_texture_iterator[1],fade_texture_iterator[2]);
		for(int i=0;i<3;i++)
		{
			fade_texture_iterator[i].texture_index=i;
		}
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}

bool SimulSkyRenderer::RenderPlanet(void* tex,float rad,const float *dir,const float *colr,bool do_lighting)
{
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	if(do_lighting)
		m_pSkyEffect->SetTechnique(m_hTechniquePlanet);
	else
		m_pSkyEffect->SetTechnique(m_hTechniqueSun);
	m_pSkyEffect->SetTexture(flareTexture,(LPDIRECT3DTEXTURE9)tex);
	simul::sky::float4 planet_dir4=dir;
	planet_dir4/=simul::sky::length(planet_dir4);

	simul::sky::float4 planet_colour(colr[0],colr[1],colr[2],1.f);
	float planet_elevation=asin(y_vertical?planet_dir4.y:planet_dir4.z);
	planet_colour*=skyKeyframer->GetIsotropicColourLossFactor(alt_km,planet_elevation,0,1e10f);
	D3DXVECTOR4 planet_dir(dir);

	// Make it bigger than it should be?
	static float size_mult=1.f;
	float planet_angular_size=size_mult*rad;
	// Start the query
	m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&planet_colour));
	HRESULT hr=RenderAngledQuad(planet_dir,planet_angular_size);
	return hr==S_OK;
}
bool SimulSkyRenderer::RenderFades(int w,int h)
{
	HRESULT hr=S_OK;
	int size=w/4;
	if(h/(numAltitudes+2)<size)
		size=h/(numAltitudes+2);

	m_pSkyEffect->SetTexture(fadeTexture2D,(LPDIRECT3DBASETEXTURE9)inscatter_2d.GetColorTex());
	RenderTexture(m_pd3dDevice,8	,8		,size,size,(LPDIRECT3DBASETEXTURE9)inscatter_2d.GetColorTex(),m_pSkyEffect,m_hTechniqueShowFade);
	m_pSkyEffect->SetTexture(fadeTexture2D,(LPDIRECT3DBASETEXTURE9)loss_2d.GetColorTex());
	RenderTexture(m_pd3dDevice,8	,16+size,size,size,(LPDIRECT3DBASETEXTURE9)inscatter_2d.GetColorTex(),m_pSkyEffect,m_hTechniqueShowFade);
	m_pSkyEffect->SetTexture(fadeTexture2D,(LPDIRECT3DBASETEXTURE9)skylight_2d.GetColorTex());
	RenderTexture(m_pd3dDevice,8	,24+2*size,size,size,(LPDIRECT3DBASETEXTURE9)inscatter_2d.GetColorTex(),m_pSkyEffect,m_hTechniqueShowFade);

	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		m_pSkyEffect->SetFloat	(altitudeTexCoord	,atc);
		m_pSkyEffect->SetTexture(fadeTexture, inscatter_textures[0]);
		RenderTexture(m_pd3dDevice,8+2*(size+8)	,(i)*(size+8)+8,size,size,inscatter_textures[0],m_pSkyEffect,m_hTechniqueFadeCrossSection);
		m_pSkyEffect->SetTexture(fadeTexture, inscatter_textures[1]);
		RenderTexture(m_pd3dDevice,8+3*(size+8)	,(i)*(size+8)+8,size,size,inscatter_textures[1],m_pSkyEffect,m_hTechniqueFadeCrossSection);
		m_pSkyEffect->SetTexture(fadeTexture, skylight_textures[0]);
		RenderTexture(m_pd3dDevice,8+4*(size+8)	,(i)*(size+8)+8,size,size,inscatter_textures[0],m_pSkyEffect,m_hTechniqueFadeCrossSection);
		m_pSkyEffect->SetTexture(fadeTexture, skylight_textures[1]);
		RenderTexture(m_pd3dDevice,8+5*(size+8)	,(i)*(size+8)+8,size,size,inscatter_textures[1],m_pSkyEffect,m_hTechniqueFadeCrossSection);

	}

	return (hr==S_OK);
}
/*
bool SimulSkyRenderer::GetSiderealTransform(D3DXMATRIX *world)
{
	HRESULT hr=S_OK;
	if(!GetSiderealSkyInterface())
	{
		D3DXMatrixIdentity(world);
		//D3DXMatrixRotationX(world,3.14159f*2.f*skyKeyframer->GetTime());
	}
	else
	{
		D3DXMATRIX tra(GetSiderealSkyInterface()->SiderealToEarthMatrix());
		D3DXMATRIX flip_y_z;
		static float rr=3.1415926536f/2.f;
		D3DXMatrixRotationX(&flip_y_z,rr);
		D3DXMATRIX tmp1, tmp2;
		D3DXMatrixMultiply(&tmp1,&flip_y_z,&tra);
		D3DXMatrixMultiply(&tmp2,&tmp1,&flip_y_z);
		D3DXMatrixTranspose(world,&tmp2);
	}
	world->_41=cam_pos.x;
	world->_42=cam_pos.y;
	world->_43=cam_pos.z;
	return (hr==S_OK);
}*/

bool SimulSkyRenderer::RenderPointStars()
{
	PIXBeginNamedEvent(0xFF000FFF,"SimulSkyRenderer::RenderPointStars");
	HRESULT hr=S_OK;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	SetCameraPosition(tmp1._41,tmp1._42,tmp1._43);

	cam_dir.x=tmp1._31;
	cam_dir.y=tmp1._32;
	cam_dir.z=tmp1._33;
	if(!y_vertical)
		cam_dir*=-1.f;

	GetSiderealTransform((float*)&world);
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	D3DXMatrixInverse(&tmp2,NULL,&world);
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));
	//hr=m_pd3dDevice->SetVertexDeclaration(NULL);
	hr=m_pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_TEX0);

	m_pSkyEffect->SetTechnique(m_hTechniquePointStars);

	float sb=skyKeyframer->GetSkyInterface()->GetStarlight().x;
	float star_brightness=sb*skyKeyframer->GetStarBrightness();
	m_pSkyEffect->SetFloat(starBrightness,star_brightness);
	
	int current_num_stars=skyKeyframer->stars.GetNumStars();
	if(!star_vertices||current_num_stars!=num_stars)
	{
		num_stars=current_num_stars;
		delete [] star_vertices;
		star_vertices=new StarVertext[num_stars];
		static float d=100.f;
		{
			for(int i=0;i<num_stars;i++)
			{
				float ra=(float)skyKeyframer->stars.GetStar(i).ascension;
				float de=(float)skyKeyframer->stars.GetStar(i).declination;
				star_vertices[i].x= d*cos(de)*sin(ra);
				star_vertices[i].y= d*cos(de)*cos(ra);
				star_vertices[i].z= d*sin(de);
				star_vertices[i].b=(float)exp(-skyKeyframer->stars.GetStar(i).magnitude);
				star_vertices[i].c=1.f;
			}
		}
	}

	UINT passes=1;
	hr=m_pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_TEX1 );
	hr=m_pSkyEffect->Begin(&passes,0);
	for(unsigned i=0;i<passes;i++)
	{
		hr=m_pSkyEffect->BeginPass(i);
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_POINTLIST,num_stars,star_vertices,sizeof(StarVertext));
		hr=m_pSkyEffect->EndPass();
	}
	hr=m_pSkyEffect->End();
	D3DXMatrixIdentity(&world);
	PIXEndNamedEvent();
	return (hr==S_OK);
}

bool SimulSkyRenderer::RenderTextureStars()
{
	HRESULT hr=S_OK;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	SetCameraPosition(tmp1._41,tmp1._42,tmp1._43);

	cam_dir.x=tmp1._31;
	cam_dir.y=tmp1._32;
	cam_dir.z=tmp1._33;
	if(!y_vertical)
		cam_dir*=-1.f;

	GetSiderealTransform((float*)&world);
	FixProjectionMatrix(proj,size*4.f,y_vertical);
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));

	hr=m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl);

	m_pSkyEffect->SetTechnique(m_hTechniqueStarrySky);
	m_pSkyEffect->SetTexture(starsTexture,stars_texture);

	float sb=skyKeyframer->GetSkyInterface()->GetStarlight().x;
	m_pSkyEffect->SetFloat(starBrightness,sb*skyKeyframer->GetStarBrightness());
	
	UINT passes=1;
	hr=m_pSkyEffect->Begin(&passes,0);
	for(unsigned i=0;i<passes;i++)
	{
		hr=m_pSkyEffect->BeginPass(i);
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,12,vertices,sizeof(Vertex_t));
		hr=m_pSkyEffect->EndPass();
	}
	hr=m_pSkyEffect->End();
	D3DXMatrixIdentity(&world);
	return (hr==S_OK);
}

bool SimulSkyRenderer::Render2DFades()
{
	// Not needed if called from Render():
	//m_pSkyEffect->SetFloat	(altitudeTexCoord	,GetAltitudeTextureCoordinate());
	//m_pSkyEffect->SetFloat	(skyInterp		,skyKeyframer->GetInterpolation());
	m_pSkyEffect->SetTechnique(m_hTechnique3DTo2DFade);
	m_pSkyEffect->SetTexture(fadeTexture,loss_textures[0]);
	m_pSkyEffect->SetTexture(fadeTexture2,loss_textures[1]);
	static float ff=0.5f;
	D3DXVECTOR4 offs(ff/(float)numFadeDistances,ff/(float)numFadeElevations,0.f,1.f);
	m_pSkyEffect->SetVector(texelOffset,&offs);
	static float uu=0.f;
	D3DXVECTOR4 sc(1.f-uu/(float)numFadeDistances,1.f-uu/(float)numFadeElevations,1.f,1.f);
	m_pSkyEffect->SetVector(texelScale,&sc);
	float atc=GetAltitudeTextureCoordinate();
	m_pSkyEffect->SetFloat	(altitudeTexCoord	,atc);
	loss_2d.Activate();
	DrawFullScreenQuad(m_pd3dDevice,m_pSkyEffect);
	loss_2d.Deactivate();
	m_pSkyEffect->SetTexture(fadeTexture,inscatter_textures[0]);
	m_pSkyEffect->SetTexture(fadeTexture2,inscatter_textures[1]);
	inscatter_2d.Activate();
	DrawFullScreenQuad(m_pd3dDevice,m_pSkyEffect);
	inscatter_2d.Deactivate();
	
	m_pSkyEffect->SetTexture(fadeTexture,skylight_textures[0]);
	m_pSkyEffect->SetTexture(fadeTexture2,skylight_textures[1]);
	skylight_2d.Activate();
	DrawFullScreenQuad(m_pd3dDevice,m_pSkyEffect);
	skylight_2d.Deactivate();
	return true;
}

bool SimulSkyRenderer::Render(bool blend)
{
	interp_at_last_render=skyKeyframer->GetInterpolation();
	m_pSkyEffect->SetFloat	(altitudeTexCoord	,GetAltitudeTextureCoordinate());
	m_pSkyEffect->SetFloat	(skyInterp			,skyKeyframer->GetInterpolation());
	EnsureTexturesAreUpToDate();
	Render2DFades();
	PIXBeginNamedEvent(0xFF00FFFF,"SimulSkyRenderer::Render");
	HRESULT hr=S_OK;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	SetCameraPosition(tmp1._41,tmp1._42,tmp1._43);

	cam_dir.x=tmp1._31;
	cam_dir.y=tmp1._32;
	cam_dir.z=tmp1._33;

	D3DXMatrixIdentity(&world);
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));

	m_pSkyEffect->SetTexture(inscTexture,(LPDIRECT3DBASETEXTURE9)inscatter_2d.GetColorTex());
	m_pSkyEffect->SetTexture(skylTexture,(LPDIRECT3DBASETEXTURE9)skylight_2d.GetColorTex());

	hr=m_pd3dDevice->SetVertexDeclaration( m_pVtxDecl );

	m_pSkyEffect->SetTechnique(m_hTechniqueSky);
	if(blend)
	{
		m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
		m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
		m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
	}
	else
	{
		m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
	}
	simul::sky::float4 mie_rayleigh_ratio=skyKeyframer->GetMieRayleighRatio();
	D3DXVECTOR4 ratio(mie_rayleigh_ratio);
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToLight(cam_pos.z*0.001f));
	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);

	m_pSkyEffect->SetVector	(lightDirection		,&sun_dir);
	m_pSkyEffect->SetVector	(mieRayleighRatio	,&ratio);
	m_pSkyEffect->SetFloat	(hazeEccentricity	,skyKeyframer->GetMieEccentricity());
	UINT passes=1;
	hr=m_pSkyEffect->Begin( &passes, 0 );
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		hr=m_pSkyEffect->BeginPass(i);
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,12,vertices,sizeof(Vertex_t));
		hr=m_pSkyEffect->EndPass();
	}
	hr=m_pSkyEffect->End();
	D3DXMatrixIdentity(&world);
	PIXEndNamedEvent();
	return (hr==S_OK);
}
#ifdef XBOX
void SimulSkyRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	SetCameraPosition(tmp1._41,tmp1._42,tmp1._43);
}
#endif

float SimulSkyRenderer::GetTiming() const
{
	return timing;
}

simul::sky::float4 SimulSkyRenderer::GetAmbient() const
{
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	return GetBaseSkyInterface()->GetAmbientLight(alt_km);
}

simul::sky::float4 SimulSkyRenderer::GetLightColour() const
{
	float alt_km=0.001f*(y_vertical?cam_pos.y:cam_pos.z);
	return GetBaseSkyInterface()->GetLocalIrradiance(alt_km);
}

const char *SimulSkyRenderer::GetDebugText() const
{
	static char txt[200];
	sprintf_s(txt,200,"interp %3.3g",interp_at_last_render);
	return txt;
}

void SimulSkyRenderer::SetTime(float hour)
{
	skyKeyframer->SetTime(hour/24.f);
	skyKeyframer->Reset();
}
