// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulSkyRenderer.cpp A renderer for skies.

#include "SimulSkyRenderer.h"

#ifdef XBOX
	#include <dxerr.h>
	#include <string>
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
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/SkyNode.h"
#include "Simul/Sky/ColourSkyNode.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Sky/ColourSkyKeyframer.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/Decay.h"
#include "Resources.h"

SimulSkyRenderer::SimulSkyRenderer(bool UseColourSky)
	:m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,m_pHudVertexDecl(NULL)
	,m_pSkyEffect(NULL)
	,m_hTechniqueSky(NULL)
	,m_hTechniquePlainColour(NULL)
	,m_hTechniqueStarrySky(NULL)
	,m_hTechniquePointStars(NULL)
	,m_hTechniqueSun(NULL)
	,m_hTechniqueQuery(NULL)
	,m_hTechniqueFlare(NULL)
	,m_hTechniquePlanet(NULL)
	,m_hTechniqueFadeCrossSection(NULL)
	,flare_texture(NULL)
	,stars_texture(NULL)
	,d3dQuery(NULL)
	,sky_tex_format(D3DFMT_UNKNOWN)
	,external_flare_texture(false)
	,m_pFont(NULL)
	,star_vertices(NULL)
{
	MoonTexture="Moon.png";
	SunTexture="SunFlare.png";
	for(int i=0;i<3;i++)
	{
		sky_textures[i]=NULL;
		loss_textures[i]=NULL;
		inscatter_textures[i]=NULL;
		sunlight_textures[i]=NULL;
	}
	EnableColourSky(UseColourSky);
	fadeTableInterface->SetEarthTest(false);
	skyInterface->SetTime(0.5f);

	cam_pos.x=cam_pos.z=0;
	cam_pos.y=400.f;
}

HRESULT SimulSkyRenderer::RestoreDeviceObjects(LPDIRECT3DDEVICE9 dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=dev;
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
		V_RETURN(CanUseTexFormat(m_pd3dDevice,sky_tex_format));
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
	hr=CreateSkyEffect();
	m_hTechniqueSky				=m_pSkyEffect->GetTechniqueByName("simul_sky");
	m_hTechniqueStarrySky		=m_pSkyEffect->GetTechniqueByName("simul_starry_sky");
	m_hTechniquePointStars		=m_pSkyEffect->GetTechniqueByName("simul_point_stars");
	m_hTechniquePlainColour		=m_pSkyEffect->GetTechniqueByName("simul_plain_colour");
	
	worldViewProj				=m_pSkyEffect->GetParameterByName(NULL,"worldViewProj");
	lightDirection				=m_pSkyEffect->GetParameterByName(NULL,"lightDir");
	mieRayleighRatio			=m_pSkyEffect->GetParameterByName(NULL,"mieRayleighRatio");
	hazeEccentricity			=m_pSkyEffect->GetParameterByName(NULL,"hazeEccentricity");
	skyInterp					=m_pSkyEffect->GetParameterByName(NULL,"skyInterp");
	altitudeTexCoord			=m_pSkyEffect->GetParameterByName(NULL,"altitudeTexCoord");

	colour						=m_pSkyEffect->GetParameterByName(NULL,"colour");
	m_hTechniqueSun				=m_pSkyEffect->GetTechniqueByName("simul_sun");
	m_hTechniqueFlare			=m_pSkyEffect->GetTechniqueByName("simul_flare");
	m_hTechniquePlanet			=m_pSkyEffect->GetTechniqueByName("simul_planet");
	flareTexture				=m_pSkyEffect->GetParameterByName(NULL,"flareTexture");

	skyTexture1					=m_pSkyEffect->GetParameterByName(NULL,"skyTexture1");
	skyTexture2					=m_pSkyEffect->GetParameterByName(NULL,"skyTexture2");
	starsTexture				=m_pSkyEffect->GetParameterByName(NULL,"starsTexture");
	starBrightness				=m_pSkyEffect->GetParameterByName(NULL,"starBrightness");

	m_hTechniqueQuery			=m_pSkyEffect->GetTechniqueByName("simul_query");
	m_hTechniqueFadeCrossSection=m_pSkyEffect->GetTechniqueByName("simul_fade_cross_section");
	fadeTexture					=m_pSkyEffect->GetParameterByName(NULL,"fadeTexture");

	fadeTableInterface->SetCallback(NULL);
	fadeTableInterface->SetCallback(this);
	// CreateSkyTexture() will be called back

	// OK to fail with these (more or less):
	if(!external_flare_texture)
	{
		SAFE_RELEASE(flare_texture);
		CreateDX9Texture(m_pd3dDevice,flare_texture,SunTexture.c_str());
	}
	SAFE_RELEASE(stars_texture);
	CreateDX9Texture(m_pd3dDevice,stars_texture,"TychoSkymapII.t5_04096x02048.jpg");

	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	halo_textures.clear();
	int num_halo_textures=0;
	for(int i=0;i<lensFlare.GetNumArtifacts();i++)
	{
		int t=lensFlare.GetArtifactType(i);
		if(t+1>num_halo_textures)
			num_halo_textures=t+1;
	}
	halo_textures.resize(num_halo_textures);
	for(int i=0;i<num_halo_textures;i++)
	{
		std::string tn=lensFlare.GetTypeName(i);
		LPDIRECT3DTEXTURE9 &tex=halo_textures[i];
		CreateDX9Texture(m_pd3dDevice,tex,(tn+".png").c_str());
	}
	SAFE_RELEASE((LPDIRECT3DTEXTURE9&)moon_texture);
	CreateDX9Texture(m_pd3dDevice,(LPDIRECT3DTEXTURE9&)moon_texture,MoonTexture.c_str());
	SetPlanetImage(moon_index,moon_texture);
//	stars.LoadAsText("UrsaMajor.txt"); used for calibration
	stars.UseDefaultStars();
	return hr;
}

HRESULT SimulSkyRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	delete [] star_vertices;
	star_vertices=NULL;
	sky_tex_format=D3DFMT_UNKNOWN;
	if(m_pSkyEffect)
        hr=m_pSkyEffect->OnLostDevice();
	SAFE_RELEASE(m_pSkyEffect);
	SAFE_RELEASE(m_pFont);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pHudVertexDecl);
	

	SAFE_RELEASE(stars_texture);
	if(!external_flare_texture)
	{
		SAFE_RELEASE(flare_texture);
	}
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	halo_textures.clear();
	SAFE_RELEASE((LPDIRECT3DTEXTURE9&)moon_texture);
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sky_textures[i]);
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(inscatter_textures[i]);
	//	SAFE_RELEASE(loss_textures_3d[i]);
	//	SAFE_RELEASE(inscatter_textures_3d[i]);
		SAFE_RELEASE(sunlight_textures[i]);
	}
	fadeTexWidth=fadeTexHeight=numAltitudes=0;
	SAFE_RELEASE(d3dQuery);
	fadeTableInterface->SetCallback(NULL);
	return hr;
}

HRESULT SimulSkyRenderer::Destroy()
{
	HRESULT hr=S_OK;
	InvalidateDeviceObjects();
	fadeTableInterface->SetCallback(NULL);
	return hr;
}

SimulSkyRenderer::~SimulSkyRenderer()
{
	Destroy();
}

void SimulSkyRenderer::FillSkyTexture(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array)
{
	HRESULT hr;
	LPDIRECT3DTEXTURE9 tex=NULL;
	tex=sky_textures[texture_index];
	if(!tex)
		return;
	texel_index+=alt_index*skyTexSize;
	if(texel_index<0)
		return;
	if(num_texels<=0)
		return;
	if(texel_index+num_texels>skyTexSize*numAltitudes)
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
		{
			*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*float4_array++);
		}
	}
	else
	{
		// Convert the array of floats into float16 values for the texture.
		float *float_ptr=(float *)(lockedRect.pBits);
		float_ptr+=4*texel_index;
		//memcpy(float_ptr,float4_array,sizeof(float)*4*num_texels);
		for(int i=0;i<num_texels*4;i++)
			*float_ptr++=(*float4_array++);
	}
	hr=tex->UnlockRect(0);
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

void SimulSkyRenderer::SetOvercastFactor(float of)
{
	skyInterface->SetOvercast(of);
}

void SimulSkyRenderer::SetOvercastBaseAndRange(float base_alt_km,float range_km)
{
	skyInterface->SetOvercastBaseKm(base_alt_km);
	skyInterface->SetOvercastRangeKm(range_km);
}

void SimulSkyRenderer::GetLossAndInscatterTextures(LPDIRECT3DBASETEXTURE9 *l1,LPDIRECT3DBASETEXTURE9 *l2,
		LPDIRECT3DBASETEXTURE9 *i1,LPDIRECT3DBASETEXTURE9 *i2)
{
	//if(numAltitudes<=1)
	{
		*l1=loss_textures[0];
		*l2=loss_textures[1];
		*i1=inscatter_textures[0];
		*i2=inscatter_textures[1];
	}
/*	else
	{
		*l1=loss_textures_3d[0];
		*l2=loss_textures_3d[1];
		*i1=inscatter_textures_3d[0];
		*i2=inscatter_textures_3d[1];
	}*/
}

void SimulSkyRenderer::GetSkyTextures(LPDIRECT3DBASETEXTURE9 *s1,LPDIRECT3DBASETEXTURE9 *s2)
{
	*s1=sky_textures[0];
	*s2=sky_textures[1];
}

float SimulSkyRenderer::GetFadeInterp() const
{
	return skyKeyframer->GetInterpolation();
}
void SimulSkyRenderer::SetStepsPerDay(int s)
{
	skyKeyframer->SetUniformKeyframes(s);
}

void SimulSkyRenderer::SetSkyTextureSize(unsigned size)
{
	skyTexSize=size;
	CreateSkyTextures();
}
void SimulSkyRenderer::SetFadeTextureSize(unsigned width,unsigned height,unsigned num_alts)
{
	if(fadeTexWidth==width&&fadeTexHeight==height&&numAltitudes==num_alts)
		return;
	if(!m_pd3dDevice)
		return;
	fadeTexWidth=width;
	fadeTexHeight=height;
	numAltitudes=num_alts;
	CreateFadeTextures();
	CreateSunlightTextures();
}

void SimulSkyRenderer::CreateFadeTextures()
{
	HRESULT hr;
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(inscatter_textures[i]);
	}
	for(int i=0;i<3;i++)
	{
		if(numAltitudes<=1)
		{
			LPDIRECT3DTEXTURE9 tex1,tex2;
			hr=D3DXCreateTexture(m_pd3dDevice,fadeTexWidth,fadeTexHeight,1,0,sky_tex_format,d3d_memory_pool,&tex1);
			hr=D3DXCreateTexture(m_pd3dDevice,fadeTexWidth,fadeTexHeight,1,0,sky_tex_format,d3d_memory_pool,&tex2);
			loss_textures[i]=tex1;
			inscatter_textures[i]=tex2;
		}
		else
		{
			LPDIRECT3DVOLUMETEXTURE9 tex1,tex2;
			hr=D3DXCreateVolumeTexture(m_pd3dDevice,fadeTexWidth,fadeTexHeight,numAltitudes,1,0,sky_tex_format,d3d_memory_pool,&tex1);
			hr=D3DXCreateVolumeTexture(m_pd3dDevice,fadeTexWidth,fadeTexHeight,numAltitudes,1,0,sky_tex_format,d3d_memory_pool,&tex2);
			loss_textures[i]=tex1;
			inscatter_textures[i]=tex2;
		}
	}
}

void SimulSkyRenderer::FillFadeTexturesSequentially(int texture_index,int texel_index,int num_texels,
						const float *loss_float4_array,
						const float *inscatter_float4_array)
{
	HRESULT hr=S_OK;
	D3DLOCKED_RECT lockedRect={0};
	D3DLOCKED_BOX lockedBox={0};
	D3DSURFACE_DESC desc;
	D3DVOLUME_DESC desc3d;
	LPDIRECT3DTEXTURE9 tex=NULL;
	LPDIRECT3DVOLUMETEXTURE9 tex3d=NULL;
	void *tex_ptr=NULL;
	if(numAltitudes<=1)
	{
		tex=(LPDIRECT3DTEXTURE9)loss_textures[texture_index];
		if(!tex)
			return;
		if(FAILED(hr=tex->LockRect(0,&lockedRect,NULL,NULL)))
			return;
		tex->GetLevelDesc(0,&desc);
		tex_ptr=lockedRect.pBits;
	}
	else
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
		for(int i=0;i<num_texels*4;i++)
			*float_ptr++=(*loss_float4_array++);
	}
	if(numAltitudes<=1)
	{
		hr=tex->UnlockRect(0);
		tex=(LPDIRECT3DTEXTURE9)inscatter_textures[texture_index];
		if(!tex)
			return;
		if(FAILED(hr=tex->LockRect(0,&lockedRect,NULL,NULL)))
			return;
		tex->GetLevelDesc(0,&desc);
		tex_ptr=lockedRect.pBits;
	}
	else
	{
		hr=tex3d->UnlockBox(0);
		tex3d=(LPDIRECT3DVOLUMETEXTURE9)inscatter_textures[texture_index];
		tex3d->GetLevelDesc(0,&desc3d);
		if(!tex3d)
			return;
		if(FAILED(hr=tex3d->LockBox(0,&lockedBox,NULL,NULL)))
			return;
		tex_ptr=lockedBox.pBits;
		// already added:
		//texel_index+=desc3d.Width*desc3d.Height*alt_index;
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
		for(int i=0;i<num_texels*4;i++)
			*float_ptr++=(*inscatter_float4_array++);
	}
	if(numAltitudes<=1)
		hr=tex->UnlockRect(0);
	else
		hr=tex3d->UnlockBox(0);

}

void SimulSkyRenderer::CycleTexturesForward()
{
	std::swap(sky_textures[0],sky_textures[1]);
	std::swap(sky_textures[1],sky_textures[2]);
	std::swap(loss_textures[0],loss_textures[1]);
	std::swap(loss_textures[1],loss_textures[2]);
	std::swap(inscatter_textures[0],inscatter_textures[1]);
	std::swap(inscatter_textures[1],inscatter_textures[2]);
	
//std::swap(loss_textures_3d[0],loss_textures_3d[1]);
//	std::swap(loss_textures_3d[1],loss_textures_3d[2]);
//	std::swap(inscatter_textures_3d[0],inscatter_textures_3d[1]);
//	std::swap(inscatter_textures_3d[1],inscatter_textures_3d[2]);
	
	std::swap(sunlight_textures[0],sunlight_textures[1]);
	std::swap(sunlight_textures[1],sunlight_textures[2]);
}

HRESULT SimulSkyRenderer::CreateSkyTextures()
{
	HRESULT hr=S_OK;
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sky_textures[i]);
	}
	for(int i=0;i<3;i++)
	{
		if(numAltitudes<=1)
		{
			if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,skyTexSize,1,1,0,sky_tex_format,d3d_memory_pool,&sky_textures[i])))
				return hr;
		}
		else
		{
			if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,skyTexSize,numAltitudes,1,0,sky_tex_format,d3d_memory_pool,&sky_textures[i])))
				return hr;
		}
		LPDIRECT3DTEXTURE9 tex=sky_textures[i];
		D3DLOCKED_RECT lockedRect={0};
		if(FAILED(hr=tex->LockRect(0,&lockedRect,NULL,NULL)))
			return hr;
		if(sky_tex_format==D3DFMT_A16B16G16R16F)
		{
			// Convert the array of floats into float16 values for the texture.
			short *short_ptr=(short *)(lockedRect.pBits);
			for(int i=0;i<(int)skyTexSize*4;i++)
			{
				*short_ptr++=simul::sky::TextureGenerator::ToFloat16(0.f);
			}
		}
		else
		{
			// Convert the array of floats into float16 values for the texture.
			float *float_ptr=(float *)(lockedRect.pBits);
			for(unsigned i=0;i<skyTexSize*4;i++)
				*float_ptr++=0.f;
		}
		hr=tex->UnlockRect(0);
	}
	return hr;
}
HRESULT SimulSkyRenderer::CreateSunlightTextures()
{
	HRESULT hr=S_OK;
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sunlight_textures[i]);
	}
	for(int i=0;i<3;i++)
	{
		if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,numAltitudes,1,1,0,sky_tex_format,d3d_memory_pool,&sunlight_textures[i])))
			return hr;
	}
	return hr;
}

HRESULT SimulSkyRenderer::CreateSkyEffect()
{
	std::map<std::string,std::string> defines;
	defines["USE_ALTITUDE_INTERPOLATION"]="";
	HRESULT hr=CreateDX9Effect(m_pd3dDevice,m_pSkyEffect,"simul_sky.fx",defines);
	return hr;
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

HRESULT SimulSkyRenderer::RenderAngledQuad(D3DXVECTOR4 dir,float half_angle_radians)
{
	float Yaw=atan2(dir.x,dir.z);
	float Pitch=-asin(dir.y);
	HRESULT hr=S_OK;
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixIdentity(&world);
	D3DXMatrixRotationYawPitchRoll(
		  &world,
		  Yaw,
		  Pitch,
		  0
		);
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToSun());
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
	return hr;
}
int query_issued=0;
float SimulSkyRenderer::CalcSunOcclusion(float cloud_occlusion)
{
	sun_occlusion=cloud_occlusion;
	//if(!m_hTechniqueQuery)
		return sun_occlusion;
	m_pSkyEffect->SetTechnique(m_hTechniqueQuery);
	D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight());
	std::swap(sun_dir.y,sun_dir.z);
	float sun_angular_size=3.14159f/180.f/2.f;

	// fix the projection matrix so this quad is far away:
	D3DXMATRIX tmp=proj;
	float zNear=-proj._43/proj._33;
	static float ff=0.0001f;
	float zFar=(1.f+ff)/tan(sun_angular_size);
	zNear=zFar*ff;
	proj._33=zFar/(zFar-zNear);
	proj._43=-zNear*zFar/(zFar-zNear);
	HRESULT hr;
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
	else
	{
		query_issued=2;
    	// Loop until the data becomes available
    	DWORD pixelsVisible=0;
		if(d3dQuery->GetData((void *)&pixelsVisible,sizeof(DWORD),0)!=S_FALSE)//D3DGETDATA_FLUSH
		{
			sun_occlusion=1.f-(float)pixelsVisible/500.f;
			if(sun_occlusion<0)
				sun_occlusion=0;
			sun_occlusion=1.f-(1.f-cloud_occlusion)*(1.f-sun_occlusion);
			query_issued=0;
		}
	}
	proj=tmp;
}
	float sun_angular_size=3.14159f/180.f/2.f;

HRESULT SimulSkyRenderer::RenderSun()
{
	float alt_km=0.001f*cam_pos.y;
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=pow(1.f-sun_occlusion,0.25f)*25.f;//2700.f;
	m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&sunlight));
	m_pSkyEffect->SetTechnique(m_hTechniqueSun);
	D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight());
	std::swap(sun_dir.y,sun_dir.z);
	HRESULT hr=RenderAngledQuad(sun_dir,sun_angular_size);
	return hr;
}
void SimulSkyRenderer::SetFlare(LPDIRECT3DTEXTURE9 tex,float rad)
{
	if(!external_flare_texture)
	{
		SAFE_RELEASE(flare_texture);
	}
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	halo_textures.clear();
	flare_angular_size=rad;
	flare_texture=tex;
	external_flare_texture=true;
}


bool SimulSkyRenderer::RenderPlanet(void* tex,float rad,const float *dir,const float *colr,bool do_lighting)
{
	float alt_km=0.001f*cam_pos.y;
	if(do_lighting)
		m_pSkyEffect->SetTechnique(m_hTechniquePlanet);
	else
		m_pSkyEffect->SetTechnique(m_hTechniqueFlare);
	m_pSkyEffect->SetTexture(flareTexture,(LPDIRECT3DTEXTURE9)tex);


	simul::sky::float4 original_irradiance=skyInterface->GetSunIrradiance();

	simul::sky::float4 planet_dir4=dir;
	planet_dir4/=simul::sky::length(planet_dir4);

	simul::sky::float4 planet_colour(colr[0],colr[1],colr[2],1.f);
	float planet_elevation=asin(planet_dir4.y);
	planet_colour*=skyInterface->GetIsotropicColourLossFactor(alt_km,planet_elevation,0,1e10f);
	D3DXVECTOR4 planet_dir(dir);

	// Make it bigger than it should be?
	static float size_mult=1.f;
	float planet_angular_size=size_mult*rad;
	// Start the query
	m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&planet_colour));
	HRESULT hr=RenderAngledQuad(planet_dir,planet_angular_size);
	return hr==S_OK;
}

HRESULT SimulSkyRenderer::RenderFlare(float exposure)
{
	HRESULT hr=S_OK;
	if(!m_pSkyEffect)
		return hr;
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToLight());
	float magnitude=exposure*(1.f-sun_occlusion);
	simul::math::FirstOrderDecay(flare_magnitude,magnitude,5.f,0.1f);
	if(flare_magnitude>1.f)
		flare_magnitude=1.f;
	float alt_km=0.001f*cam_pos.y;
	simul::sky::float4 sunlight=skyKeyframer->GetLocalIrradiance(alt_km)*lensFlare.GetStrength();
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	static float sun_mult=0.05f;
	sunlight*=sun_mult*flare_magnitude;
    m_pd3dDevice->SetVertexShader(NULL);
    m_pd3dDevice->SetPixelShader(NULL);
	m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&sunlight));
	m_pSkyEffect->SetTechnique(m_hTechniqueFlare);
	m_pSkyEffect->SetTexture(flareTexture,flare_texture);
	std::swap(sun_dir.y,sun_dir.z);
	lensFlare.UpdateCamera(cam_dir,sun_dir);
	if(flare_magnitude>0.f)
	{
		hr=RenderAngledQuad(sun_dir,flare_angular_size*flare_magnitude);
		sunlight*=0.25f;
		for(int i=0;i<lensFlare.GetNumArtifacts();i++)
		{
			D3DXVECTOR4 pos=lensFlare.GetArtifactPosition(i);
			float sz=lensFlare.GetArtifactSize(i);
			int t=lensFlare.GetArtifactType(i);
			m_pSkyEffect->SetTexture(flareTexture,halo_textures[t]);
			m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&sunlight));
			hr=RenderAngledQuad(pos,flare_angular_size*sz*flare_magnitude);
		}
	}
	return hr;
}

HRESULT SimulSkyRenderer::RenderFades(int )
{
	HRESULT hr=S_OK;
	static int size=64;
	m_pSkyEffect->SetTexture(fadeTexture, loss_textures[0]);
	RenderTexture(m_pd3dDevice,8			,size+32,size,size,loss_textures[0],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	m_pSkyEffect->SetTexture(fadeTexture, loss_textures[1]);
	RenderTexture(m_pd3dDevice,8+(size+8)	,size+32,size,size,loss_textures[1],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	m_pSkyEffect->SetTexture(fadeTexture, loss_textures[2]);
	RenderTexture(m_pd3dDevice,8+2*(size+8)	,size+32,size,size,loss_textures[2],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	m_pSkyEffect->SetTexture(fadeTexture, inscatter_textures[0]);
	RenderTexture(m_pd3dDevice,8			,size*2+64,size,size,inscatter_textures[0],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	m_pSkyEffect->SetTexture(fadeTexture, inscatter_textures[1]);
	RenderTexture(m_pd3dDevice,8+(size+8)	,size*2+64,size,size,inscatter_textures[1],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	m_pSkyEffect->SetTexture(fadeTexture, inscatter_textures[2]);
	RenderTexture(m_pd3dDevice,8+2*(size+8)	,size*2+64,size,size,inscatter_textures[2],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	m_pSkyEffect->SetTexture(fadeTexture, sky_textures[0]);
	RenderTexture(m_pd3dDevice,8			,size*3+96,size,size,sky_textures[0],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	m_pSkyEffect->SetTexture(fadeTexture, sky_textures[1]);
	RenderTexture(m_pd3dDevice,8+(size+8)	,size*3+96,size,size,sky_textures[1],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	m_pSkyEffect->SetTexture(fadeTexture, sky_textures[2]);
	RenderTexture(m_pd3dDevice,8+2*(size+8)	,size*3+96,size,size,sky_textures[2],m_pSkyEffect,m_hTechniqueFadeCrossSection);
	return hr;
}

HRESULT SimulSkyRenderer::PrintAt(const float *p,const wchar_t *text,int screen_width,int screen_height)
{
	D3DXMatrixTranslation(&world,cam_pos.x,cam_pos.y,cam_pos.z);
#ifndef xbox
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&world);
#endif
	D3DXMATRIX tmp1,tmp2,wvp;
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&wvp,&tmp1,&proj);


	D3DXVECTOR4 pos(p[0],p[1],p[2],1.f);
	D3DXVECTOR4 screen_pos;
	D3DXVec4Transform(&screen_pos,&pos,&wvp);
	float x=0.5f*(screen_pos.x/screen_pos.w+1.f)*screen_width;//screen_pos.z;
	float y=0.5f*(1.f-screen_pos.y/screen_pos.w)*screen_height;//screen_pos.z;
	RECT rcDest;
	rcDest.left=(long)x-32;
	rcDest.bottom=(long)y+32;
	rcDest.right=(long)x+32;
	rcDest.top=(long)y-32;

	DWORD dwTextFormat = DT_CENTER |  DT_NOCLIP ;//DT_CALCRECT;
	if(screen_pos.w<0)
		return S_OK;
	HRESULT hr = m_pFont->DrawText(NULL,text,-1,&rcDest,dwTextFormat,D3DXCOLOR(1.f,1.f,1.f,1.f));
	return hr;
}

HRESULT SimulSkyRenderer::RenderCelestialDisplay(int screen_width,int screen_height)
{
	HRESULT hr=S_OK;
	D3DVERTEXELEMENT9 decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 12, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0 },
		D3DDECL_END()
	};
	if(!m_pHudVertexDecl)
	{
		V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pHudVertexDecl));
	}

	if(!m_pFont)
	{
		V_RETURN(D3DXCreateFont(m_pd3dDevice,32,0,FW_NORMAL,1,FALSE,DEFAULT_CHARSET,
								OUT_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,
								L"Arial",&m_pFont));
	}

	D3DXMatrixTranslation(&world,cam_pos.x,cam_pos.y,cam_pos.z);
#ifndef xbox
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&world);
#endif
	
	D3DXMATRIX tmp1,tmp2,wvp;
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&wvp,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&wvp));

	struct Vertext
	{
		float x,y,z;
		float r,g,b,a;
	};
	m_pd3dDevice->SetVertexDeclaration(m_pHudVertexDecl);


	m_pd3dDevice->SetTexture(0,NULL);
	Vertext *lines=new Vertext[64*2];
	static float d=10000.f;
	float pi=3.1415926f;
	for(int i=0;i<64;i++)
	{
		bool D=((8*(i/8))==i);
		float angle=(float)i/64.f*2.f*pi;
		lines[i*2].x=d*cos(angle); 
		lines[i*2].y=-d*(D?.025f:0.01f); 
		lines[i*2].z=d*sin(angle);
		lines[i*2].r=0.f;
		lines[i*2].g=0.f;
		lines[i*2].b=0.f;
		lines[i*2].a=0.5f;
		lines[i*2+1].x=d*cos(angle);
		lines[i*2+1].y=d*(D?.025f:0.01f); 
		lines[i*2+1].z=d*sin(angle);
		lines[i*2+1].r=1.f;
		lines[i*2+1].g=0.5f;
		lines[i*2+1].b=0.f;
		lines[i*2+1].a=0.5f;
	}

	m_pSkyEffect->SetTechnique(m_hTechniquePlainColour);
	

	UINT passes=1;
	hr=m_pSkyEffect->Begin(&passes,0);
	hr=m_pSkyEffect->BeginPass(0);


	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST,64,lines,(unsigned)sizeof(Vertext));
	delete [] lines;
	lines=new Vertext[65];
	Vertext *moon_lines=new Vertext[65];
	float old_time=skyKeyframer->GetTime();
	for(int i=0;i<65;i++)
	{
		float time=(float)i/32.f;
		float daytime=time-(int)time;
		//skyInterface->SetDaytime(time);
		skyKeyframer->SetDaytime(time);
		simul::sky::float4 dir=d*skyInterface->GetDirectionToSun();
		lines[i].x=dir.x; 
		lines[i].y=dir.z;
		lines[i].z=dir.y;
		lines[i].r=1.f;
		lines[i].g=.8f;
		lines[i].b=0.f;
		lines[i].a=0.1f+0.5f*daytime;
		simul::sky::float4 moon_dir=d*skyInterface->GetDirectionToMoon();
		moon_lines[i].x=moon_dir.x; 
		moon_lines[i].y=moon_dir.z;
		moon_lines[i].z=moon_dir.y;
		moon_lines[i].r=0.f;
		moon_lines[i].g=.5f;
		moon_lines[i].b=1.f;
		moon_lines[i].a=0.1f+0.5f*daytime;
	}
	skyInterface->SetTime(old_time);
	skyKeyframer->SetTime(old_time);
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINESTRIP,64,lines,(unsigned)sizeof(Vertext));
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINESTRIP,64,moon_lines,(unsigned)sizeof(Vertext));

	hr=m_pSkyEffect->EndPass();
	hr=m_pSkyEffect->End();

	PrintAt(D3DXVECTOR4( 0.f, -0.05f, 1.f, 1.f)	,L"N",screen_width,screen_height);
	PrintAt(D3DXVECTOR4( 0.7f,-0.05f, 0.7f,1.f)	,L"NE",screen_width,screen_height);
	PrintAt(D3DXVECTOR4( 1.f, -0.05f, 0.f, 1.f)	,L"E",screen_width,screen_height);
	PrintAt(D3DXVECTOR4( 0.7f,-0.05f,-.7f, 1.f)	,L"SE",screen_width,screen_height);
	PrintAt(D3DXVECTOR4( 0.f, -0.05f,-1.f, 1.f)	,L"S",screen_width,screen_height);
	PrintAt(D3DXVECTOR4(-1.f, -0.05f,-1.f, 1.f)	,L"SW",screen_width,screen_height);
	PrintAt(D3DXVECTOR4(-1.f, -0.05f, 0.f, 1.f)	,L"W",screen_width,screen_height);
	PrintAt(D3DXVECTOR4(-1.f, -0.05f, 1.f, 1.f)	,L"NW",screen_width,screen_height);
	delete [] lines;
	delete [] moon_lines;
	return S_OK;
}

HRESULT SimulSkyRenderer::GetSiderealTransform(D3DXMATRIX *world)
{
	HRESULT hr=S_OK;
	if(!GetSiderealSkyInterface())
	{
		D3DXMatrixIdentity(world);
		D3DXMatrixRotationX(world,3.14159f*2.f*skyInterface->GetTime());
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
	return hr;
}

HRESULT SimulSkyRenderer::RenderPointStars()
{
	PIXBeginNamedEvent(0xFF000FFF,"SimulSkyRenderer::RenderPointStars");
	HRESULT hr=S_OK;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;

	cam_dir.x=tmp1._31;
	cam_dir.y=tmp1._32;
	cam_dir.z=tmp1._33;

	GetSiderealTransform(&world);
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));
	hr=m_pd3dDevice->SetVertexDeclaration(NULL);
	hr=m_pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_TEX0);

	m_pSkyEffect->SetTechnique(m_hTechniquePointStars);
	static float star_brightness=1000.f;

	float sb=skyInterface->GetStarlight().x;
	m_pSkyEffect->SetFloat(starBrightness,sb*star_brightness);
	
	int num_stars=stars.GetNumStars();
	if(!star_vertices)
	{
		star_vertices=new StarVertext[num_stars];
		static float d=100.f;
		{
			for(int i=0;i<num_stars;i++)
			{
				float ra=(float)stars.GetStar(i).ascension;
				float de=(float)stars.GetStar(i).declination;
				star_vertices[i].x= d*cos(de)*sin(ra);
				star_vertices[i].z=-d*cos(de)*cos(ra);
				star_vertices[i].y=-d*sin(de);
				star_vertices[i].b=(float)exp(-stars.GetStar(i).magnitude);
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
	return hr;
}

HRESULT SimulSkyRenderer::RenderStars()
{
return S_OK;
	HRESULT hr=S_OK;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;

	cam_dir.x=tmp1._31;
	cam_dir.y=tmp1._32;
	cam_dir.z=tmp1._33;

	GetSiderealTransform(&world);
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));

	hr=m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl);

	m_pSkyEffect->SetTechnique(m_hTechniqueStarrySky);
	m_pSkyEffect->SetTexture(starsTexture,stars_texture);
	static float star_brightness=1.f;

	float sb=skyInterface->GetStarlight().x;
	m_pSkyEffect->SetFloat(starBrightness,sb*star_brightness);
	
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
	return hr;
}

HRESULT SimulSkyRenderer::Render()
{
	PIXBeginNamedEvent(0xFF00FFFF,"SimulSkyRenderer::Render");
	HRESULT hr=S_OK;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;

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

	m_pSkyEffect->SetTexture(skyTexture1,sky_textures[0]);
	m_pSkyEffect->SetTexture(skyTexture2,sky_textures[1]);

	hr=m_pd3dDevice->SetVertexDeclaration( m_pVtxDecl );

	m_pSkyEffect->SetTechnique(m_hTechniqueSky);

	simul::sky::float4 mie_rayleigh_ratio=skyInterface->GetMieRayleighRatio();
	D3DXVECTOR4 ratio(mie_rayleigh_ratio);
	D3DXVECTOR4 sun_dir(skyKeyframer->GetDirectionToLight());
	std::swap(sun_dir.y,sun_dir.z);

	m_pSkyEffect->SetVector	(lightDirection		,&sun_dir);
	m_pSkyEffect->SetVector	(mieRayleighRatio	,&ratio);
	m_pSkyEffect->SetFloat	(hazeEccentricity	,skyInterface->GetMieEccentricity());
	m_pSkyEffect->SetFloat	(altitudeTexCoord	,GetAltitudeTextureCoordinate());

	m_pSkyEffect->SetFloat	(skyInterp		,skyKeyframer->GetInterpolation());
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
	return hr;
}
#ifdef XBOX
void SimulSkyRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
}
#endif

float SimulSkyRenderer::GetTiming() const
{
	return timing;
}

simul::sky::float4 SimulSkyRenderer::GetAmbient() const
{
	float alt_km=0.001f*cam_pos.y;
	return skyInterface->GetSkyColour(simul::sky::float4(0,0,1,0),alt_km);
}

simul::sky::float4 SimulSkyRenderer::GetLightColour() const
{
	float alt_km=0.001f*cam_pos.y;
	return skyInterface->GetLocalIrradiance(alt_km);
}

const char *SimulSkyRenderer::GetDebugText() const
{
	static char txt[200];
	sprintf_s(txt,200,"interp %3.3g",skyKeyframer->GetInterpolation());
	return txt;
}

void SimulSkyRenderer::SetTime(float hour)
{
	GetSiderealSkyInterface()->SetHourOfTheDay(hour);
	GetFadeTableInterface()->Reset();
}
