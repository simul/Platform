// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with that agreement.

#include "SimulTerrainRenderer.h"
#include "Simul/Terrain/Cutout.h"
#include "Simul/Terrain/HeightMapNode.h"
#include "Simul/Clouds/BaseCloudRenderer.h"

#ifdef XBOX
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <d3d9.h>
	#include <d3dx9.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif

#include <stack>
#include "Simul/Base/SmartPtr.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Terrain/Road.h"
#include "CreateDX9Effect.h"
#include "Macros.h"
#include "Resources.h"
#undef min
#undef max
typedef std::basic_string<TCHAR> tstring;

SimulTerrainRenderer::SimulTerrainRenderer() :
	m_pVtxDecl(NULL)
	,m_pd3dDevice(NULL)
	,m_pTerrainEffect(NULL)
	,pRenderHeightmapEffect(NULL)
	,terrain_texture(NULL)
	,detail_texture(NULL)
	,road_texture(NULL)
	,colourkey_texture(NULL)
	,mainVertexBuffer(NULL)
	,cloud_textures(NULL)
	,skyInterface(NULL)
	,height_texture(NULL)
	,rock_height_texture(NULL)
	,soil_depth_texture(NULL)
	,water_texture(NULL)
	,flux_texture(NULL)
	,show_wireframe(false)
	,rebuild_effect(true)
	,y_vertical(true)
	,max_fade_distance_metres(300000.f)
	,last_overall_checksum(0)
	,last_buffer_checksum(0)
{
}

struct TerrainVertex_t
{
	float x,y,z;
	float tex_x,tex_y;
	float morph,anymorph;
    float texCoordHx,texCoordHy;
    float texCoordH1x,texCoordH1y;
    float texCoordH2x,texCoordH2y;
};

bool SimulTerrainRenderer::Create( LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	return (hr==S_OK);
}

bool SimulTerrainRenderer::MakeMapTexture()
{
	last_overall_checksum=heightmap->GetChecksum();
	int size=heightmap->GetPageSize()-1;
	SAFE_RELEASE(rock_height_texture);
	SAFE_RELEASE(height_texture);
	SAFE_RELEASE(soil_depth_texture);
	SAFE_RELEASE(water_texture);
	SAFE_RELEASE(flux_texture);
	int mips=heightmap->GetNumMipMapLevels();
	V_CHECK(D3DXCreateTexture(m_pd3dDevice,size,size,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,&height_texture));
	V_CHECK(D3DXCreateTexture(m_pd3dDevice,size,size,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,&rock_height_texture));
	V_CHECK(D3DXCreateTexture(m_pd3dDevice,size,size,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,&soil_depth_texture));
	V_CHECK(D3DXCreateTexture(m_pd3dDevice,size,size,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,&water_texture));
	V_CHECK(D3DXCreateTexture(m_pd3dDevice,size,size,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,D3DPOOL_DEFAULT,&flux_texture));

	LPDIRECT3DSURFACE9						pOldRenderTarget=NULL;
	V_CHECK(m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget));
	{
		int terrain_size=heightmap->GetPageSize()-1;
		LPDIRECT3DSURFACE9						pRenderTarget=NULL;
		water_texture->GetSurfaceLevel(0,&pRenderTarget);
		V_CHECK(m_pd3dDevice->SetRenderTarget(0,pRenderTarget));
		m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0x00000000,1.0f,0L);
		SAFE_RELEASE(pRenderTarget);
	
		flux_texture->GetSurfaceLevel(0,&pRenderTarget);
		V_CHECK(m_pd3dDevice->SetRenderTarget(0,pRenderTarget));
		m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0x00000000,1.0f,0L);
		SAFE_RELEASE(pRenderTarget);
	}
	V_CHECK(m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget));
	SAFE_RELEASE(pOldRenderTarget);
	/*
	int skip=1;
	D3DLOCKED_RECT lockedRect={0};
	for(int m=0;m<mips;m++)
	{
		V_CHECK(height_texture->LockRect(m,&lockedRect,NULL,NULL));
		float *ptr=(float *)(lockedRect.pBits);
		for(int i=0;i<size;i+=skip)
		{
			for(int j=0;j<size;j+=skip)
			{
				float h=heightmap->GetHeightAt(j,i);
				const float *n=heightmap->GetNormalAt(j,i);
				*(ptr++)=h;
				*(ptr++)=n[0];
				*(ptr++)=n[1];
				*(ptr++)=n[2];
			}
		}
		V_CHECK(height_texture->UnlockRect(m));
		skip*=2;
	}*/
	//D3DUSAGE_AUTOGENMIPMAP height_texture->GenerateMipSubLevels();
	return true;
}
#include "simul/clouds/TextureGenerator.h"

#ifdef XBOX
	const bool big_endian=true;
#else
	const bool big_endian=false;
#endif

void SimulTerrainRenderer::GPUGenerateHeightmap()
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return;
	LPDIRECT3DTEXTURE9				temp_noise_texture=NULL;
	int noise_size=heightmap->GetFractalFrequency();
	// Make the input texture:
	{
		if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,noise_size,noise_size,0,D3DUSAGE_AUTOGENMIPMAP,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&temp_noise_texture)))
			return;
		D3DLOCKED_RECT lockedRect={0};
		if(FAILED(hr=temp_noise_texture->LockRect(0,&lockedRect,NULL,NULL)))
			return;
		static unsigned bits8[4]={	(unsigned)0x00FF0000,
									(unsigned)0x000000FF,
									(unsigned)0x0000FF00,
									(unsigned)0xFF000000};
		simul::clouds::TextureGenerator::SetBits(bits8[0],bits8[1],bits8[2],bits8[3],(unsigned)4,big_endian);
		static int seed=5;
		simul::clouds::TextureGenerator::Make2DRandomTexture((unsigned char *)(lockedRect.pBits),noise_size,seed);
		hr=temp_noise_texture->UnlockRect(0);
		temp_noise_texture->GenerateMipSubLevels();
	}

	LPDIRECT3DSURFACE9				pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9				pRenderTarget=NULL;

	D3DXHANDLE fractalHeightTechnique		=pRenderHeightmapEffect->GetTechniqueByName("fractal_height");
	D3DXHANDLE inputTexture					=pRenderHeightmapEffect->GetParameterByName(NULL,"noiseTexture");
	D3DXHANDLE octaves						=pRenderHeightmapEffect->GetParameterByName(NULL,"octaves");
	D3DXHANDLE persistence					=pRenderHeightmapEffect->GetParameterByName(NULL,"persistence");
	D3DXHANDLE heightRange					=pRenderHeightmapEffect->GetParameterByName(NULL,"heightRange");
	D3DXVECTOR4 height_range(heightmap->GetBaseAltitude(),heightmap->GetMaxHeight(),0,0);
	pRenderHeightmapEffect->SetVector(heightRange,&height_range);
	D3DXHANDLE texcoordScale				=pRenderHeightmapEffect->GetParameterByName(NULL,"texcoordScale");
	float texcoord_scale=heightmap->GetInitialFrequency()/(float)heightmap->GetFractalFrequency();
	pRenderHeightmapEffect->SetFloat(texcoordScale,texcoord_scale);
	D3DXHANDLE worldSize					=pRenderHeightmapEffect->GetParameterByName(NULL,"worldSize");
	float world_size=heightmap->GetPageWorldX();
	int terrain_size=heightmap->GetPageSize()-1;
	pRenderHeightmapEffect->SetFloat(worldSize,world_size);

	pRenderHeightmapEffect->SetTexture(inputTexture,temp_noise_texture);
	pRenderHeightmapEffect->SetFloat(persistence,heightmap->GetPersistence());
	int texture_octaves=heightmap->GetFractalOctaves();
	pRenderHeightmapEffect->SetInt(octaves,texture_octaves);
	
	hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);
	{
		rock_height_texture->GetSurfaceLevel(0,&pRenderTarget);
		hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		RenderTexture(m_pd3dDevice,0,0,terrain_size,terrain_size,temp_noise_texture,pRenderHeightmapEffect,fractalHeightTechnique);
		rock_height_texture->GenerateMipSubLevels();
	}
	D3DXHANDLE heightTexture				=pRenderHeightmapEffect->GetParameterByName(NULL,"heightTexture");
	pRenderHeightmapEffect->SetTexture(heightTexture,rock_height_texture);

	for(int i=0;i<heightmap->GetNumFilters();i++)
	{
		simul::terrain::Filter *filter=heightmap->GetFilter(i);
		D3DXHANDLE tech=pRenderHeightmapEffect->GetTechniqueByName(filter->GetType());
		if(!tech)
		{
			std::cerr<<"Missing technique: "<<filter->GetType()<<std::endl;
			continue;
		}
		D3DXHANDLE remapHeightsTexture=pRenderHeightmapEffect->GetParameterByName(NULL,"remapHeightsTexture");

		LPDIRECT3DTEXTURE9 remap_heights_texture=NULL;
		int remap_size=filter->remap_heights.size();
		if(remap_size)
		{
			if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,remap_size,1,1,D3DUSAGE_AUTOGENMIPMAP,D3DFMT_R32F,D3DPOOL_MANAGED,&remap_heights_texture)))
				return;
			D3DLOCKED_RECT lockedRect={0};
			if(FAILED(hr=remap_heights_texture->LockRect(0,&lockedRect,NULL,NULL)))
				return;
			float *f=(float *)(lockedRect.pBits);
			for(int i=0;i<remap_size;i++)
			{
				*f++=filter->remap_heights[i];
			}
			remap_heights_texture->UnlockRect(0);
		}

		pRenderHeightmapEffect->SetTexture(remapHeightsTexture,remap_heights_texture);
		LPDIRECT3DTEXTURE9		alternate_map_texture;
		hr=(m_pd3dDevice->CreateTexture(	terrain_size,
											terrain_size,
											1,
											D3DUSAGE_RENDERTARGET,
											D3DFMT_A32B32G32R32F,
											D3DPOOL_DEFAULT,
											&alternate_map_texture,
											NULL
										));
		SAFE_RELEASE(pRenderTarget);
		alternate_map_texture->GetSurfaceLevel(0,&pRenderTarget);
		hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		RenderTexture(m_pd3dDevice,0,0,terrain_size,terrain_size,temp_noise_texture,pRenderHeightmapEffect,tech);
		alternate_map_texture->GenerateMipSubLevels();
		std::swap(alternate_map_texture,rock_height_texture);
		SAFE_RELEASE(alternate_map_texture);
		SAFE_RELEASE(remap_heights_texture);
		pRenderHeightmapEffect->SetTexture(heightTexture,rock_height_texture);
	}
	SAFE_RELEASE(pRenderTarget);
	pRenderHeightmapEffect->SetTexture(heightTexture,rock_height_texture);
	GpuMakeNormals();

	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	SAFE_RELEASE(pOldRenderTarget);
	SAFE_RELEASE(temp_noise_texture);
	SAFE_RELEASE(pRenderTarget);
}

void SimulTerrainRenderer::GpuMakeNormals()
{
	LPDIRECT3DSURFACE9				pRenderTarget=NULL;
	int terrain_size=heightmap->GetPageSize()-1;
	float world_size=heightmap->GetPageWorldX();
	
	D3DXHANDLE gridSize						=pRenderHeightmapEffect->GetParameterByName(NULL,"gridSize");
	pRenderHeightmapEffect->SetFloat(gridSize,(float)(terrain_size));
	D3DXHANDLE worldSize					=pRenderHeightmapEffect->GetParameterByName(NULL,"worldSize");
	pRenderHeightmapEffect->SetFloat(worldSize,world_size);
	D3DXHANDLE heightTexture				=pRenderHeightmapEffect->GetParameterByName(NULL,"heightTexture");
	D3DXHANDLE addTexture				=pRenderHeightmapEffect->GetParameterByName(NULL,"addTexture");
	pRenderHeightmapEffect->SetTexture(addTexture,water_texture);
	D3DXHANDLE soilTexture					=pRenderHeightmapEffect->GetParameterByName(NULL,"soilTexture");
	D3DXHANDLE makeNormalsTechnique			=pRenderHeightmapEffect->GetTechniqueByName("height_to_normals");
	{
	/*	SAFE_RELEASE(height_texture);
		HRESULT hr=(m_pd3dDevice->CreateTexture(	terrain_size,
													terrain_size,
													1,
													D3DUSAGE_RENDERTARGET,
													D3DFMT_A32B32G32R32F,
													D3DPOOL_DEFAULT,
													&height_texture,
													NULL	));*/
		height_texture->GetSurfaceLevel(0,&pRenderTarget);
		m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		RenderTexture(m_pd3dDevice,0,0,terrain_size,terrain_size,NULL,pRenderHeightmapEffect,makeNormalsTechnique);
		SAFE_RELEASE(pRenderTarget);
	}
}

void SimulTerrainRenderer::GpuThermalErosion(float time_step)
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return;
	int terrain_size=heightmap->GetPageSize()-1;
	LPDIRECT3DTEXTURE9				out_texture=NULL;
	// input: height_texture, including normals.
	// output: eroded heightmap.
	// post-process: convert to new height_texture, including normals
	LPDIRECT3DSURFACE9						pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9						pRenderTarget=NULL;
	D3DXHANDLE gridSize						=pRenderHeightmapEffect->GetParameterByName(NULL,"gridSize");
	pRenderHeightmapEffect->SetFloat(gridSize,(float)(terrain_size));
	D3DXHANDLE thermalErosionTechnique1		=pRenderHeightmapEffect->GetTechniqueByName("thermal_erosion_out");
	D3DXHANDLE thermalErosionTechnique2		=pRenderHeightmapEffect->GetTechniqueByName("thermal_erosion_in");
	D3DXHANDLE heightTexture				=pRenderHeightmapEffect->GetParameterByName(NULL,"heightTexture");
	D3DXHANDLE addTexture					=pRenderHeightmapEffect->GetParameterByName(NULL,"addTexture");
	D3DXHANDLE talusTangent					=pRenderHeightmapEffect->GetParameterByName(NULL,"talusTangent");
	pRenderHeightmapEffect->SetFloat(talusTangent,tan(heightmap->GetTalusAngleDegrees()*3.13159f/180.f));
	pRenderHeightmapEffect->SetTexture(heightTexture,height_texture);
	hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);
	{
		hr=(m_pd3dDevice->CreateTexture(	terrain_size,
											terrain_size,
											1,
											D3DUSAGE_RENDERTARGET,
											D3DFMT_A32B32G32R32F,
											D3DPOOL_DEFAULT,
											&out_texture,
											NULL	));
		out_texture->GetSurfaceLevel(0,&pRenderTarget);
		hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		RenderTexture(m_pd3dDevice,0,0,terrain_size,terrain_size,NULL,pRenderHeightmapEffect,thermalErosionTechnique1);
	}
	{
		pRenderHeightmapEffect->SetTexture(heightTexture,height_texture);
		pRenderHeightmapEffect->SetTexture(addTexture,out_texture);
		SAFE_RELEASE(pRenderTarget);
		soil_depth_texture->GetSurfaceLevel(0,&pRenderTarget);
		hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		RenderTexture(m_pd3dDevice,0,0,terrain_size,terrain_size,NULL,pRenderHeightmapEffect,thermalErosionTechnique2);
	}
	pRenderHeightmapEffect->SetTexture(heightTexture,soil_depth_texture);
	GpuMakeNormals();
	hr=m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	SAFE_RELEASE(pRenderTarget);
	SAFE_RELEASE(pOldRenderTarget);
	SAFE_RELEASE(out_texture);
}

void SimulTerrainRenderer::GpuWaterErosion(float time_step)
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return;
	if(!pRenderHeightmapEffect)
		return;
	int terrain_size=heightmap->GetPageSize()-1;
	LPDIRECT3DSURFACE9						pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9						pRenderTarget=NULL;
	D3DXHANDLE texcoordScale				=pRenderHeightmapEffect->GetParameterByName(NULL,"texcoordScale");
	float texcoord_scale=heightmap->GetInitialFrequency()/(float)heightmap->GetFractalFrequency();
	pRenderHeightmapEffect->SetFloat(texcoordScale,texcoord_scale);
	D3DXHANDLE worldSize					=pRenderHeightmapEffect->GetParameterByName(NULL,"worldSize");
	float world_size=heightMapInterface->GetPageWorldX();
	pRenderHeightmapEffect->SetFloat(worldSize,world_size);
	D3DXHANDLE gridSize						=pRenderHeightmapEffect->GetParameterByName(NULL,"gridSize");
	pRenderHeightmapEffect->SetFloat(gridSize,(float)(terrain_size));
	D3DXHANDLE hydraulicErosionTechnique1	=pRenderHeightmapEffect->GetTechniqueByName("hydraulic_erosion_out");
	D3DXHANDLE hydraulicErosionTechnique2	=pRenderHeightmapEffect->GetTechniqueByName("hydraulic_erosion_in");
	D3DXHANDLE hydraulicErosionTechnique3	=pRenderHeightmapEffect->GetTechniqueByName("hydraulic_erosion_apply");
	D3DXHANDLE heightTexture				=pRenderHeightmapEffect->GetParameterByName(NULL,"heightTexture");
	D3DXHANDLE addTexture					=pRenderHeightmapEffect->GetParameterByName(NULL,"addTexture");
	D3DXHANDLE waterTexture					=pRenderHeightmapEffect->GetParameterByName(NULL,"waterTexture");
	D3DXHANDLE rainfall						=pRenderHeightmapEffect->GetParameterByName(NULL,"rainfall");
	D3DXHANDLE evaporation					=pRenderHeightmapEffect->GetParameterByName(NULL,"evaporation");
	pRenderHeightmapEffect->SetTexture(addTexture,water_texture);
	pRenderHeightmapEffect->SetTexture(heightTexture,height_texture);
	pRenderHeightmapEffect->SetFloat(rainfall,0.05f*heightmap->GetRainfall());
	pRenderHeightmapEffect->SetFloat(evaporation,0.01f*heightmap->GetEvaporation());
	hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);
	{
		soil_depth_texture->GetSurfaceLevel(0,&pRenderTarget);
		hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		RenderTexture(m_pd3dDevice,0,0,terrain_size,terrain_size,NULL,pRenderHeightmapEffect,hydraulicErosionTechnique1);
	}
	{
		pRenderHeightmapEffect->SetTexture(addTexture,soil_depth_texture);
		SAFE_RELEASE(pRenderTarget);
		water_texture->GetSurfaceLevel(0,&pRenderTarget);
		hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		RenderTexture(m_pd3dDevice,0,0,terrain_size,terrain_size,NULL,pRenderHeightmapEffect,hydraulicErosionTechnique2);
	}
	{
		pRenderHeightmapEffect->SetTexture(addTexture,water_texture);
		SAFE_RELEASE(pRenderTarget);
		soil_depth_texture->GetSurfaceLevel(0,&pRenderTarget);
		hr=m_pd3dDevice->SetRenderTarget(0,pRenderTarget);
		RenderTexture(m_pd3dDevice,0,0,terrain_size,terrain_size,NULL,pRenderHeightmapEffect,hydraulicErosionTechnique3);
	}
	pRenderHeightmapEffect->SetTexture(heightTexture,soil_depth_texture);
	GpuMakeNormals();
	hr=m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	SAFE_RELEASE(pOldRenderTarget);
	SAFE_RELEASE(pRenderTarget);
}

void SimulTerrainRenderer::GetVertex(int i,int j,TerrainVertex_t *V)
{
	int grid=heightMapInterface->GetPageSize();
	float X=(float)i/((float)grid-1.f);
	float Y=(float)j/((float)grid-1.f);
	
	V->x=(X)*heightMapInterface->GetPageWorldX();
	V->y=(Y)*heightMapInterface->GetPageWorldY();
	V->z=heightMapInterface->GetHeightAt(i,j);
	if(y_vertical)
		std::swap(V->y,V->z);
	static simul::math::Vector3 n;
	n.DefineValues(heightMapInterface->GetNormalAt(i,j));
	if(y_vertical)
		std::swap(n.y,n.z);
	static float tex_scale=20.f;
	V->tex_x=tex_scale*X;
	V->tex_y=tex_scale*Y;
	V->morph=0.f;
}

void SimulTerrainRenderer::GetVertex(float x,float y,TerrainVertex_t *V)
{
	float X=x/heightMapInterface->GetPageWorldX()+0.5f;
	float Y=y/heightMapInterface->GetPageWorldY()+0.5f;
	
	V->x=x;
	V->y=y;
	V->z=heightMapInterface->GetHeightAt(x,y);
	if(y_vertical)
		std::swap(V->y,V->z);
	simul::math::Vector3 n=heightMapInterface->GetNormalAt(x,y);
	V->tex_x=130.f*X;
	V->tex_y=130.f*Y;
	V->anymorph=1.f;
	V->morph=0.f;
    V->texCoordHx=0;
	V->texCoordHy=0;
    V->texCoordH1x=0;
	V->texCoordH1y=0;
    V->texCoordH2x=0;
	V->texCoordH2y=0;
}

void SimulTerrainRenderer::TerrainModified()
{
	heightmap->Rebuild();
}

void SimulTerrainRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	last_overall_checksum=heightmap->GetChecksum();
	enabled=false;
	HRESULT hr;
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,12,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0,20,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		{0,28,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,3},
		{0,36,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,4},
		{0,44,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,5},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);

	SAFE_RELEASE(terrain_texture);
	V_CHECK(D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/MudGrass01.dds"),&terrain_texture));

	SAFE_RELEASE(detail_texture);
	V_CHECK(D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/grass01.dds"),&detail_texture));

	SAFE_RELEASE(road_texture);
	V_CHECK(D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/road.dds"),&road_texture));

	SAFE_RELEASE(colourkey_texture);
	D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/colourkey.png"),&colourkey_texture);

	rebuild_effect=true;

	RebuildBuffers();
	RecompileShaders();
	MakeMapTexture();
	GPUGenerateHeightmap();
}

unsigned SimulTerrainRenderer::GetBufferChecksum()
{
	unsigned checksum=0;
	int tile_size=heightMapInterface->GetTileSize();
	float world_size=heightMapInterface->GetPageWorldX();
	int page_size=heightMapInterface->GetPageSize();
	checksum+=*(reinterpret_cast<unsigned*>(&tile_size));
	checksum+=*(reinterpret_cast<unsigned*>(&world_size));
	checksum+=*(reinterpret_cast<unsigned*>(&page_size));
	return checksum;
}

void SimulTerrainRenderer::RebuildBuffers()
{
	MakeMapTexture();
	last_buffer_checksum=GetBufferChecksum();
	int mips=(int)(log((float)heightMapInterface->GetTileSize()-1.f)/log(2.f));
	heightMapInterface->SetNumMipMapLevels(mips-1);
	// e.g. (16+1) * (16+1)
	int num_vertices=0;
	for(int m=0;m<heightMapInterface->GetNumMipMapLevels();m++)
	{
		int tile_size=heightMapInterface->GetTileSize()-1;
		tile_size=tile_size>>m;
		tile_size=tile_size+1;
		num_vertices+=tile_size*tile_size;
	}

	SAFE_RELEASE(mainVertexBuffer);
	V_CHECK(m_pd3dDevice->CreateVertexBuffer(num_vertices*sizeof(TerrainVertex_t),D3DUSAGE_WRITEONLY,0,
									  D3DPOOL_DEFAULT, &mainVertexBuffer,
									  NULL ));
	TerrainVertex_t *vertices;
	V_CHECK(mainVertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
	TerrainVertex_t *V=vertices;
	int grid=heightMapInterface->GetTileSize();
	float tile_texsize=(grid-1)/(float)(heightMapInterface->GetPageSize()-1);
	float wx=heightMapInterface->GetPageWorldX()*tile_texsize;
	float wy=heightMapInterface->GetPageWorldY()*tile_texsize;
	for(int m=0;m<heightMapInterface->GetNumMipMapLevels();m++)
	{
		for(int i=0;i<grid;i++)
		{
			for(int j=0;j<grid;j++)
			{
				float X=(float)(j)/((float)grid-1.f);
				float Y=(float)(i)/((float)grid-1.f);
				V->tex_x=X;
				V->tex_y=Y;
				V->x=X*wx;
				V->y=Y*wy;
				bool edge_x=(i==0||i==grid-1);
				bool edge_y=(j==0||j==grid-1);
				V->morph=(edge_x^edge_y)?1.f:0.f;
				V->anymorph=1.f;

				V->texCoordHx=X*tile_texsize;
				V->texCoordHy=Y*tile_texsize;
				
				int I=(i>>1)*2;
				int J=(j>>1)*2;

				float X1=(float)(J)/((float)grid-1.f);
				float Y1=(float)(I)/((float)grid-1.f);

				float X2=(float)(J+2)/((float)grid-1.f);
				float Y2=(float)(I+2)/((float)grid-1.f);

				if(J==j)
					X2=X1=X;
				if(I==i)
					Y2=Y1=Y;
				if((i>=grid/2)^(j>=grid/2))
					std::swap(Y1,Y2);

				V->texCoordH1x=X1*tile_texsize;
				V->texCoordH1y=Y1*tile_texsize;

				V->texCoordH2x=X2*tile_texsize;
				V->texCoordH2y=Y2*tile_texsize;


				if(edge_x&&edge_y)
					V->anymorph=0.f;
				if(y_vertical)
					std::swap(V->y,V->z);
				V++;
			}
		}
		grid=(grid>>1)+1;
	}
    V_CHECK(mainVertexBuffer->Unlock());
	ReleaseIndexBuffers();
//	BuildRoad();
	int start_index=0;
	for(int i=0;i<heightMapInterface->GetNumMipMapLevels();i++)
	{
		generic_mips.push_back(TileMipGeneric());
		TileMipGeneric &mip=generic_mips.back();
		int tile_size=heightMapInterface->GetTileSize();
		tile_size-=1;
		tile_size>>=i;
		grid=tile_size;
		tile_size+=1;
		int num_prims=0;
		if(grid>=1)
			num_prims=(2*grid+1)*(grid-2);//+2;
		
		V_CHECK(m_pd3dDevice->CreateIndexBuffer(num_prims*sizeof(unsigned)
												,D3DUSAGE_WRITEONLY
												,D3DFMT_INDEX32
												,D3DPOOL_DEFAULT
												,&mip.indexBuffer,NULL));
		unsigned *indexData;
		V_CHECK(mip.indexBuffer->Lock(0,num_prims,(void**)&indexData,0));

		unsigned *index=indexData;
		int A=0,B=1,C=1,D=2;
		// grid-2 times:
		for(int x=1;x<grid-1;x+=2)
		{
			if(x==grid/2)
			{
				A=1;B=0;C=2;D=1;
				*(index++)	=start_index+TilePosToIndex((x+B),1,tile_size);
			}
			// (grid/2-1)*2  = grid-2
			for(int y=1;y<grid/2;y++)
			{
				*(index++)	=start_index+TilePosToIndex((x+B),y,tile_size);
				*(index++)	=start_index+TilePosToIndex((x+A),y,tile_size);
			}
			// +1 =grid-1
			*(index++)	=start_index+TilePosToIndex((x+B),grid/2,tile_size);
			// + (grid/2) * 2 = 2*grid-1
			for(int y=grid/2;y<grid;y++)
			{
				*(index++)	=start_index+TilePosToIndex((x+A),y,tile_size);
				*(index++)	=start_index+TilePosToIndex((x+B),y,tile_size);
			}
			// + 2 = 2*grid +1
			*(index++)		=start_index+TilePosToIndex((x+1),(grid-1),tile_size);
			*(index++)		=start_index+TilePosToIndex((x+1),(grid-1),tile_size);
			//
			if(x+1==grid/2)
			{
				A=1;B=0;C=2;D=1;
				*(index++)	=start_index+TilePosToIndex((x+1),(grid-1),tile_size);
			}

			for(int y=1;y<grid/2;y++)
			{
				*(index++)	=start_index+TilePosToIndex((x+D),(grid-y),tile_size);
				*(index++)	=start_index+TilePosToIndex((x+C),(grid-y),tile_size);
			}
			*(index++)	=start_index+TilePosToIndex((x+D),grid/2,tile_size);
			for(int y=grid/2;y<grid;y++)
			{
				*(index++)	=start_index+TilePosToIndex((x+C),(grid-y),tile_size);
				*(index++)	=start_index+TilePosToIndex((x+D),(grid-y),tile_size);
			}
			*(index++)		=start_index+TilePosToIndex((x+2),1,tile_size);
			*(index++)		=start_index+TilePosToIndex((x+2),1,tile_size);
			if(x+2==grid/2)
				*(index++)		=start_index+TilePosToIndex((x+2),1,tile_size);
		}
		V_CHECK(mip.indexBuffer->Unlock());
		mip.num_prims=num_prims;
		mip.num_verts=heightMapInterface->GetTileSize()*heightMapInterface->GetTileSize();
		for(int lower_level=i;lower_level<heightMapInterface->GetNumMipMapLevels();lower_level++)
		{
			for(int nsew=0;nsew<4;nsew++)
			{
				FourMipEdges *edges=NULL;
				int idx=lower_level-i;
				if(idx>=(int)mip.edges.size())
				{
					mip.edges.push_back(FourMipEdges());
				}
				edges=&(mip.edges[idx]);
				MIPEdge *edge=&edges->edge[nsew];
				BuildMIPEdge(edge,i,lower_level,nsew);
			}
		}
		start_index+=tile_size*tile_size;
	}
	ClearTiles();
	int squares=heightMapInterface->GetTileSize()-1;
	size_t tile_grid=(heightMapInterface->GetPageSize())/squares;
	tiles.resize(tile_grid);
	for(size_t i=0;i<tile_grid;i++)
	{
		for(size_t j=0;j<tile_grid;j++)
		{
			TerrainTile *tile=new TerrainTile(squares,heightMapInterface->GetNumMipMapLevels());
			tiles[i].push_back(tile);
			BuildTile(tiles[i][j],(int)i,(int)j);
		}
	}
	enabled=true;
}
void SimulTerrainRenderer::ClearTiles()
{
	for(size_t i=0;i<tiles.size();i++)
	{
		for(size_t j=0;j<tiles[i].size();j++)
		{
			delete tiles[i][j];
		}
		tiles[i].clear();
	}
	tiles.clear();
}
void SimulTerrainRenderer::RecompileShaders()
{
	HRESULT hr=S_OK;
	std::map<std::string,std::string> defines;
	if(wrap_clouds)
		defines["WRAP_CLOUDS"]="1";
	char max_fade_distance_str[25];
	sprintf_s(max_fade_distance_str,25,"%g",max_fade_distance_metres);
	defines["MAX_FADE_DISTANCE_METRES"]=max_fade_distance_str;
	if(!y_vertical)
		defines["Z_VERTICAL"]='1';
	else
		defines["Y_VERTICAL"]='1';
	SAFE_RELEASE(pRenderHeightmapEffect);
	CreateDX9Effect(m_pd3dDevice,pRenderHeightmapEffect,"simul_renderheightmap.fx");
	hr=CreateDX9Effect(m_pd3dDevice,m_pTerrainEffect,"simul_terrain.fx",defines);

	m_hTechniqueTerrain		=m_pTerrainEffect->GetTechniqueByName("simul_terrain");
	m_hTechniqueDepthOnly	=m_pTerrainEffect->GetTechniqueByName("simul_depth_only");
	m_hTechniqueMap			=m_pTerrainEffect->GetTechniqueByName("simul_map");
	techniqueRoad			=m_pTerrainEffect->GetTechniqueByName("simul_road");
	worldViewProj			=m_pTerrainEffect->GetParameterByName(NULL,"worldViewProj");
	world					=m_pTerrainEffect->GetParameterByName(NULL,"world");
	mipLevels				=m_pTerrainEffect->GetParameterByName(NULL,"mipLevels");
	elevationMapTexture		=m_pTerrainEffect->GetParameterByName(NULL,"heightTexture");
	eyePosition				=m_pTerrainEffect->GetParameterByName(NULL,"eyePosition");
	morphFactor				=m_pTerrainEffect->GetParameterByName(NULL,"morphFactor");
	texOffsetH				=m_pTerrainEffect->GetParameterByName(NULL,"texOffsetH");
	lightDirection			=m_pTerrainEffect->GetParameterByName(NULL,"lightDir");

	cloudScales				=m_pTerrainEffect->GetParameterByName(NULL,"cloudScales");
	cloudOffset				=m_pTerrainEffect->GetParameterByName(NULL,"cloudOffset");
	cloudInterp				=m_pTerrainEffect->GetParameterByName(NULL,"cloudInterp");

	lightColour				=m_pTerrainEffect->GetParameterByName(NULL,"lightColour");
	ambientColour			=m_pTerrainEffect->GetParameterByName(NULL,"ambientColour");

	g_mainTexture			=m_pTerrainEffect->GetParameterByName(NULL,"g_mainTexture");
	detailTexture			=m_pTerrainEffect->GetParameterByName(NULL,"detailTexture");
	roadTexture				=m_pTerrainEffect->GetParameterByName(NULL,"roadTexture");
	waterTexture			=m_pTerrainEffect->GetParameterByName(NULL,"waterTexture");
	colourkeyTexture		=m_pTerrainEffect->GetParameterByName(NULL,"colourkeyTexture");

	cloudTexture1			=m_pTerrainEffect->GetParameterByName(NULL,"cloudTexture1");
	cloudTexture2			=m_pTerrainEffect->GetParameterByName(NULL,"cloudTexture2");

	rebuild_effect=false;

	LIGHTING_PASS=2;
	LIGHTING_PASS_WITH_SHADOWS=3;
	WIREFRAME_PASS=3;
	D3DXTECHNIQUE_DESC desc;
	m_pTerrainEffect->GetTechniqueDesc(m_hTechniqueTerrain,&desc);
	for(unsigned i=0;i<desc.Passes;i++)
	{
		D3DXHANDLE h=m_pTerrainEffect->GetPass(m_hTechniqueTerrain,i);
		D3DXPASS_DESC pass_desc;
		m_pTerrainEffect->GetPassDesc(h,&pass_desc);
		const char *name=pass_desc.Name;
		if(strcmp(name,"cloud_shadow")==0)
			LIGHTING_PASS_WITH_SHADOWS=i;
		if(strcmp(name,"shadow")==0)
			LIGHTING_PASS=i;
		if(strcmp(name,"outline")==0)
			WIREFRAME_PASS=i;
	}
}

void SimulTerrainRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	ClearTiles();
	if(m_pTerrainEffect)
        hr=m_pTerrainEffect->OnLostDevice();
	SAFE_RELEASE(m_pTerrainEffect);
	SAFE_RELEASE(pRenderHeightmapEffect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(mainVertexBuffer);
	ReleaseIndexBuffers();
	SAFE_RELEASE(terrain_texture);
	SAFE_RELEASE(detail_texture);
	SAFE_RELEASE(colourkey_texture);
	SAFE_RELEASE(road_texture);
	cloud_textures=NULL;
	SAFE_RELEASE(height_texture);
	SAFE_RELEASE(rock_height_texture);
	SAFE_RELEASE(soil_depth_texture);
	SAFE_RELEASE(water_texture);
	
	SAFE_RELEASE(flux_texture);
}

SimulTerrainRenderer::~SimulTerrainRenderer()
{
	InvalidateDeviceObjects();
}
	static const float radius=50.f;
	static const float height=150.f;

void SimulTerrainRenderer::RenderOnlyDepth()
{
	PIXBeginNamedEvent(0xFF006600,"SimulTerrainRenderer::RenderOnlyDepth");
	InternalRender(true);
	PIXEndNamedEvent();
}

void SimulTerrainRenderer::Render()
{
	PIXBeginNamedEvent(0xFF00FF00,"SimulTerrainRenderer::Render");
	HRESULT hr=InternalRender(false);
	PIXEndNamedEvent();

	if(highlight_pos.x+highlight_pos.y+highlight_pos.z!=0)
	{
		static float cc=100.f;
		float pos[13*3];
		for(int i=0;i<13;i++)
		{
			float angle=2.f*3.14159f*(float)i/12.f;
			float dx=cc*cos(angle);
			float dy=cc*sin(angle);
			pos[i*3+0]=highlight_pos.x+dx;
			pos[i*3+1]=highlight_pos.z+dy;
			pos[i*3+2]=highlight_pos.y;//z;
			if(y_vertical)
			{
				std::swap(pos[i*3+1],pos[i*3+2]);
			}
		}
		//RenderLines(m_pd3dDevice,12,pos);
	}
}

float SimulTerrainRenderer::GetMip(int i,int j) const
{
	if(i<0||j<0||i>=(int)tiles.size()||j>=(int)tiles.size())
		return 0;
	simul::math::Vector3 pos=tiles[i][j]->pos;
	pos-=simul::math::Vector3((const float*)(&cam_pos));
	float dist=pos.Magnitude();
	static float dist_mult=2.f;
	float mip_dist=dist_mult*heightMapInterface->GetPageWorldX()*heightMapInterface->GetTileSize()/(float)heightMapInterface->GetPageSize();
	float mip_level=(dist/mip_dist);
	if(mip_level>heightMapInterface->GetNumMipMapLevels()-1)
		mip_level=(float)(heightMapInterface->GetNumMipMapLevels()-1);
	return mip_level;
}

bool SimulTerrainRenderer::InternalRender(bool depth_only)
{
	HRESULT hr=S_OK;
	if(!enabled)
		return (hr==S_OK);
	if(last_buffer_checksum!=GetBufferChecksum())
		RebuildBuffers();
	if(rebuild_effect)
		RecompileShaders();
	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
	if(depth_only)
		m_pTerrainEffect->SetTechnique( m_hTechniqueDepthOnly );
	else
		m_pTerrainEffect->SetTechnique( m_hTechniqueTerrain );
	D3DXMATRIX tmp1,tmp2,wvp,w,ident;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixMultiply(&tmp2,&view,&proj);
	D3DXMatrixTranspose(&wvp,&tmp2);
	D3DXMatrixIdentity(&ident);
	m_pTerrainEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&wvp));
	m_pTerrainEffect->SetMatrix(world,(const D3DXMATRIX *)(&ident));
	m_pTerrainEffect->SetVector(eyePosition,(const D3DXVECTOR4 *)(&cam_pos));
	float mip_scale=heightMapInterface->GetNumMipMapLevels()-1.f;
	simul::sky::float4 light_colour(1.f,1.f,1.f,1.f);
	if(skyInterface)
	{
		D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight());
		if(y_vertical)
			std::swap(sun_dir.y,sun_dir.z);
		m_pTerrainEffect->SetVector	(lightDirection		,&sun_dir);
		float alt_km=cam_pos.z*0.001f;
		if(y_vertical)
			alt_km=cam_pos.y*0.001f;
		static float light_mult=0.08f;
		light_colour=light_mult*skyInterface->GetLocalIrradiance(alt_km);
		simul::sky::float4 ambient_colour=skyInterface->GetAmbientLight(alt_km);
		m_pTerrainEffect->SetVector	(lightColour	,(const D3DXVECTOR4 *)(&light_colour));
		m_pTerrainEffect->SetVector	(ambientColour	,(const D3DXVECTOR4 *)(&ambient_colour));
	}
	m_pTerrainEffect->SetVector	(cloudScales		,(const D3DXVECTOR4 *)(cloud_scales));
	m_pTerrainEffect->SetVector	(cloudOffset		,(const D3DXVECTOR4 *)(cloud_offset));
	m_pTerrainEffect->SetFloat	(cloudInterp		,cloud_interp);
	m_pTerrainEffect->SetTexture(elevationMapTexture,height_texture);
	m_pTerrainEffect->SetTexture(g_mainTexture		,terrain_texture);
	m_pTerrainEffect->SetTexture(waterTexture			,water_texture);
	m_pTerrainEffect->SetTexture(detailTexture		,detail_texture);
	m_pTerrainEffect->SetFloat(mipLevels,mip_scale);
	if(cloud_textures)
	{
		m_pTerrainEffect->SetTexture(cloudTexture1		,cloud_textures[0]);
		m_pTerrainEffect->SetTexture(cloudTexture2		,cloud_textures[1]);
	}
	else
	{
		m_pTerrainEffect->SetTexture(cloudTexture1		,NULL);
		m_pTerrainEffect->SetTexture(cloudTexture2		,NULL);
	}
	UINT passes=1;
	hr=m_pTerrainEffect->Begin( &passes, 0 );
	B_RETURN(m_pd3dDevice->SetStreamSource(	0,
											mainVertexBuffer,
											0,
											sizeof(TerrainVertex_t)
											));
	B_RETURN(m_pd3dDevice->SetVertexDeclaration(m_pVtxDecl));
	float tile_texsize=(float)(heightMapInterface->GetTileSize()-1)/(float)(heightMapInterface->GetPageSize()-1);
	static bool only=false;
	for(int p=0;p<passes;++p)
	{
		//D3DXHANDLE h=m_pTerrainEffect->GetPass(m_hTechniqueTerrain,p);
		if((p!=LIGHTING_PASS_WITH_SHADOWS&&p!=LIGHTING_PASS&&p!=WIREFRAME_PASS)||
			(cloud_textures&&p==LIGHTING_PASS_WITH_SHADOWS)||
			(!cloud_textures&&p==LIGHTING_PASS)||
			(show_wireframe&&p==WIREFRAME_PASS))
		for(size_t i=0;i<tiles.size();i++)
		{
			for(size_t j=0;j<tiles.size();j++)
			{
				float float_mip=GetMip(i,j);
					if(only&&(i!=0||j!=0))
						continue;
				simul::sky::float4 morph(float_mip/mip_scale,float_mip/mip_scale,0,0);
				m_pTerrainEffect->SetVector(morphFactor,(const D3DXVECTOR4 *)(&morph));
				simul::sky::float4 tex_offset_heightmap(i*tile_texsize,j*tile_texsize,0,0);
				m_pTerrainEffect->SetVector(texOffsetH,(const D3DXVECTOR4 *)(&tex_offset_heightmap));
				
				if(p==4)
				{
					int r=(i)%3;
					int g=(j)%4;
					int b=(i+j)%5;
					float c[]={1.0,0.0,0.5,0.25,0.75};
					simul::sky::float4 line_colour(c[r],c[g],c[b],0.5f);
					m_pTerrainEffect->SetVector(lightColour,(const D3DXVECTOR4 *)(&line_colour));
				}
				else
					m_pTerrainEffect->SetVector(lightColour,(const D3DXVECTOR4 *)(&light_colour));
				int mip_level=(int)float_mip;
				static int hh=4;
				for(int k=0;k<hh;k++)
				{
					float other_float_mip_level=GetMip(i+(k==0)-(k==1),j+(k==2)-(k==3));
					if(other_float_mip_level<float_mip)
						other_float_mip_level=float_mip;
					int other_mip_level=(int)other_float_mip_level;
					if(other_mip_level<0)
						continue;
					int idx=other_mip_level-mip_level;
					if(idx<0)
						idx=0;
					B_RETURN(m_pd3dDevice->SetIndices(generic_mips[mip_level].edges[idx].edge[k].indexBuffer));
					int num_tris=generic_mips[mip_level].edges[idx].edge[k].num_tris;
					int num_verts=generic_mips[mip_level].edges[idx].edge[k].num_verts;
					simul::sky::float4 morph(float_mip/mip_scale,other_float_mip_level/mip_scale,0,0);
					m_pTerrainEffect->SetVector(morphFactor,(const D3DXVECTOR4 *)(&morph));

					const float *x=tiles[i][j]->offset;
					D3DXMatrixTranslation(&w,x[0],x[1],x[2]);
					m_pTerrainEffect->SetMatrix(world,(const D3DXMATRIX *)(&w));

					if(p==4)
					{
						simul::sky::float4 line_colour(0.f,0.f,0.f,0.35f);
						m_pTerrainEffect->SetVector(lightColour,(const D3DXVECTOR4 *)(&line_colour));
					}
					hr=m_pTerrainEffect->BeginPass(p);
					B_RETURN(m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,num_verts,0,num_tris))
					hr=m_pTerrainEffect->EndPass();
				}

				//	if(mip->tri_strip)
				{
					B_RETURN(m_pd3dDevice->SetIndices(generic_mips[mip_level].indexBuffer));
					const float *x=tiles[i][j]->offset;
					D3DXMatrixTranslation(&w,x[0],x[1],x[2]);
					m_pTerrainEffect->SetMatrix(world,(const D3DXMATRIX *)(&w));
					hr=m_pTerrainEffect->BeginPass(p);
					B_RETURN(m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,0,generic_mips[mip_level].num_verts,0,generic_mips[mip_level].num_prims-2))
					hr=m_pTerrainEffect->EndPass();
					m_pTerrainEffect->SetMatrix(world,(const D3DXMATRIX *)(&ident));
				}
				if(tiles[i][j]->mips.size())
				{
					TileMipInstance *mip=&tiles[i][j]->mips[mip_level];
					if(mip->num_squares)
					{
						B_RETURN(m_pd3dDevice->SetIndices(mip->indexBuffer));
						hr=m_pTerrainEffect->BeginPass(p);
						B_RETURN(m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,mip->num_verts,0,mip->num_prims/3))
						hr=m_pTerrainEffect->EndPass();
					}
					if(mip->extraIndexBuffer)
					{
						B_RETURN(m_pd3dDevice->SetStreamSource(0,mip->extraVertexBuffer,0,sizeof(TerrainVertex_t)));
						B_RETURN(m_pd3dDevice->SetIndices(mip->extraIndexBuffer));
						hr=m_pTerrainEffect->BeginPass(p);
						B_RETURN(m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,(unsigned)mip->extra_vertices.size(),0,(unsigned)mip->extra_triangles.size()/3))
						hr=m_pTerrainEffect->EndPass();
						B_RETURN(m_pd3dDevice->SetStreamSource(0,mainVertexBuffer,0,sizeof(TerrainVertex_t)));
					}
				}
			}
		}

		hr=m_pTerrainEffect->EndPass();
	}
	hr=m_pTerrainEffect->End();
	m_pTerrainEffect->SetTechnique( techniqueRoad );
	m_pTerrainEffect->SetTexture(g_mainTexture,road_texture);
	hr=m_pTerrainEffect->Begin( &passes, 0 );
	for(unsigned p = 0 ; p < passes ; ++p )
	{
		hr=m_pTerrainEffect->BeginPass(p);
		for(size_t i=0;i<roads.size();i++)
		{
			if(roads[i].roadVertexBuffer)
			{
				B_RETURN(m_pd3dDevice->SetStreamSource(0,roads[i].roadVertexBuffer,0,sizeof(TerrainVertex_t)));
				B_RETURN(m_pd3dDevice->SetIndices(roads[i].roadIndexBuffer));
				B_RETURN(m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,0,roads[i].num_verts,0,roads[i].num_verts-2))
				B_RETURN(m_pd3dDevice->SetStreamSource(0,mainVertexBuffer,0,sizeof(TerrainVertex_t)));
			}
		}
		hr=m_pTerrainEffect->EndPass();
	}
	hr=m_pTerrainEffect->End();

	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,TRUE);
	return (hr==S_OK);
}

bool SimulTerrainRenderer::RenderMap(int width)
{
	int w=(width-16)/3;
	int h=w;
	
	D3DXHANDLE param_altitudeBase	=m_pTerrainEffect->GetParameterByName(NULL,"altitudeBase");
	D3DXHANDLE param_altitudeRange	=m_pTerrainEffect->GetParameterByName(NULL,"altitudeRange");
	float altitudeBase=heightMapInterface->GetBaseAltitude();
	float altitudeRange=heightMapInterface->GetMaxHeight()-altitudeBase;
	m_pTerrainEffect->SetFloat	(param_altitudeBase		,altitudeBase);
	m_pTerrainEffect->SetFloat	(param_altitudeRange	,altitudeRange);
	m_pTerrainEffect->SetTexture(elevationMapTexture	,height_texture);
	m_pTerrainEffect->SetTexture(colourkeyTexture		,colourkey_texture);
	m_pTerrainEffect->SetTexture(waterTexture			,water_texture);
	HRESULT hr=RenderTexture(m_pd3dDevice,8,8,w,h,height_texture,m_pTerrainEffect,m_hTechniqueMap);
	return(hr==S_OK);
}

void SimulTerrainRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

void SimulTerrainRenderer::SetCloudTextures(void **t,bool wrap)
{
	cloud_textures=(LPDIRECT3DVOLUMETEXTURE9*)t;
	if(wrap_clouds!=wrap)
	{
		wrap_clouds=wrap;
		rebuild_effect=true;
	}
}

simul::terrain::HeightMapInterface *SimulTerrainRenderer::GetHeightMapInterface()
{
	return heightMapInterface;
}

simul::terrain::HeightMapNode *SimulTerrainRenderer::GetHeightMap()
{
	return heightmap.get();
}

void SimulTerrainRenderer::Highlight(const float *x,const float *d)
{
	simul::math::Vector3 X(x);
	simul::math::Vector3 D(d);
	D.Normalize();
	simul::math::Vector3 I;
	if(y_vertical)
	{
		std::swap(X.y,X.z);
		std::swap(D.y,D.z);
	}
	highlight_pos.x=highlight_pos.y=highlight_pos.z=0;
	if(heightMapInterface->Collide(X,D,I))
	{
		if((I-X).Magnitude()<100000.f)
		{
			highlight_pos.x=I.x;
			highlight_pos.y=I.y;
			highlight_pos.z=I.z;
			if(y_vertical)
				std::swap(highlight_pos.y,highlight_pos.z);
		}
	}
}

void SimulTerrainRenderer::Update(float dt)
{
	if(heightmap->GetChecksum()!=last_overall_checksum)
	{
		//RestoreDeviceObjects(m_pd3dDevice);
	}
	if(ThermalErosion)
		GpuThermalErosion(dt);
	if(WaterErosion)
		GpuWaterErosion(dt);
}

void SimulTerrainRenderer::SetCloudShadowCallback(simul::clouds::CloudShadowCallback *cb)
{
	if(cb)
	{
		SetCloudTextures	(cb->GetCloudTextures(),cb->GetWrap());
		SetCloudScales		(cb->GetCloudScales());
		SetCloudOffset		(cb->GetCloudOffset());
		setCloudInterpolation(cb->GetInterpolation());
	}
}

void SimulTerrainRenderer::ReleaseIndexBuffers()
{
	for(size_t i=0;i<generic_mips.size();i++)
		generic_mips[i].Reset();
	generic_mips.clear();
	for(size_t i=0;i<tiles.size();i++)
	{
		for(size_t j=0;j<tiles.size();j++)
		{
			tiles[i][j]->Reset();
		}
	}
	for(size_t i=0;i<roads.size();i++)
	{
		roads[i].Reset();
	}
}

unsigned SimulTerrainRenderer::TilePosToIndex(int x,int y,int tile_size) const
{
	return y*(tile_size)+x;
}
simul::base::SmartPtr<simul::terrain::Road> road;
	
bool SimulTerrainRenderer::BuildRoad()
{
	HRESULT hr=S_OK;
	road=new simul::terrain::Road(heightMapInterface);

	roads.push_back(RoadRenderable());
	RoadRenderable *rr=&(roads.back());
	rr->num_verts=2*road->GetNumSegments()*12;

	
	SAFE_RELEASE(rr->roadVertexBuffer);
	B_RETURN(m_pd3dDevice->CreateVertexBuffer( rr->num_verts*sizeof(TerrainVertex_t),D3DUSAGE_WRITEONLY,0,
									  D3DPOOL_DEFAULT, &rr->roadVertexBuffer,
									  NULL ));
	TerrainVertex_t *vertices;
	B_RETURN(rr->roadVertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
	TerrainVertex_t *V=vertices;

	//RHS of road
	float along=0.f;
	float tex_repeat_length=50.f;
	for(int a=0;a<rr->num_verts/2;a++)
	{
		simul::math::float3 x=road->GetCentreLine ((float)a/(float)(rr->num_verts/2-1));
		simul::math::float3 nextx
							=road->GetCentreLine ((float)(a+1)/(float)(rr->num_verts/2-1));
		simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(rr->num_verts/2-1));
		simul::math::float2 left ((const float*)(x+dx));
		simul::math::float2 right((const float*)(x-dx));
		GetVertex(right.x,right.y,V);
		V->z=x.z;
		V->tex_x=0.f;
		V->tex_y=along/tex_repeat_length;
		V->anymorph=1.f;

				
				V->texCoordHx=0;
				V->texCoordHy=0;
				V->texCoordH1x=0;
				V->texCoordH1y=0;
				V->texCoordH2x=0;
				V->texCoordH2y=0;
		V++;
		GetVertex( left.x, left.y,V);
		V->z=x.z;
		if(y_vertical)
			std::swap(V->y,V->z);
		V->tex_x=1.f;
		V->tex_y=along/tex_repeat_length;
		V->anymorph=1.f;
				V->texCoordHx=0;
				V->texCoordHy=0;
				V->texCoordH1x=0;
				V->texCoordH1y=0;
				V->texCoordH2x=0;
				V->texCoordH2y=0;
		V++;
		along+=simul::math::float3::length(nextx-x);
	}
    B_RETURN(rr->roadVertexBuffer->Unlock());
	
	B_RETURN(m_pd3dDevice->CreateIndexBuffer(rr->num_verts*sizeof(unsigned),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
		D3DPOOL_DEFAULT, &rr->roadIndexBuffer, NULL ));
	unsigned *indexData;
	B_RETURN(rr->roadIndexBuffer->Lock(0, rr->num_verts, (void**)&indexData, 0 ));

	unsigned *index=indexData;
	for(int x=0;x<rr->num_verts/2;x++)
	{
		*(index++)	=2*x;
		*(index++)	=2*x+1;
	}
	B_RETURN(rr->roadIndexBuffer->Unlock());
	return (hr==S_OK);
}

bool SimulTerrainRenderer::BuildMIPEdge(MIPEdge *edge,int mip_level,int lower_level,int nsew)
{
	int diff=lower_level-mip_level;
	HRESULT hr=S_OK;

	int start_index=0,lower_start_index=0;
	for(int m=0;m<mip_level;m++)
	{
		int T=heightMapInterface->GetTileSize()-1;
		T=T>>m;
		T=T+1;
		start_index+=T*T;
	}
	for(int m=0;m<lower_level;m++)
	{
		int T=heightMapInterface->GetTileSize()-1;
		T=T>>m;
		T=T+1;
		lower_start_index+=T*T;
	}

	int original_tile_size=heightMapInterface->GetTileSize();
	int tile_size=original_tile_size;
	tile_size-=1;
	tile_size>>=mip_level;
	tile_size+=1;
	int skip1=1;//<<mip_level;

	int lower_tile_size=heightMapInterface->GetTileSize();
	lower_tile_size-=1;
	lower_tile_size>>=lower_level;
	int skip2=1<<diff;

	// now have two tile sizes, e.g. 9 and 3.
	
	// for each of the lower sizes, e.g. 3, we have one triangle, plus a fan of size 2^(diff)
	int U=1<<diff;
	edge->num_verts=lower_tile_size*(1+U);
	edge->num_tris=lower_tile_size*(1+U)-2;

	if(edge->num_tris>0)
	{
		B_RETURN(m_pd3dDevice->CreateIndexBuffer(edge->num_tris*3*sizeof(unsigned),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
												  D3DPOOL_DEFAULT, &edge->indexBuffer,NULL));
		unsigned *indexData;
		B_RETURN(edge->indexBuffer->Lock(0,edge->num_tris,(void**)&indexData,0));

		unsigned *index=indexData;
		// For each of the larger steps,
		for(int x=0;x<lower_tile_size;x++)
		{
			// For the big central triangles, offs is how many gaps between the two outer vertices.
			// If skip1==skip2 (same mips), and we're at the start, it's 1.
			//										 not at the start, it's ZERO (no big triangle).

			// But for skip1!=skip2 (different mip levels), it's skip2/2.
			int offs=((skip1==skip2)?(x<lower_tile_size/2?skip2:0):skip2/2);
			int same=(skip1==skip2&&x<lower_tile_size/2)?1:0;
			int a1,a2,a3;
			int b1,b2,b3;
			int i1,i2,i3;
			int s1,s2,s3;
			for(int u=0;u<1+U;u++)
			{
				if(x==0&&u==1)
					continue;
				if(x==lower_tile_size-1&&u==U)
					continue;
				// The big central triangles, u=0:
				if(!u)
				{
					s1=lower_tile_size;
					s2=lower_tile_size;
					s3=tile_size-1;
					i1=lower_start_index;
					i2=lower_start_index;
					i3=start_index;
					a1=0;
					a2=0;
					a3=skip1;
					b1=(x+1);
					b2=(x  );
					b3=(x  )*skip2+offs;
				}
				// The fan, first half and second half:
				else
				{
					s1=lower_tile_size;
					s2=tile_size-1;
					s3=tile_size-1;
					i1=lower_start_index;
					i2=start_index;
					i3=start_index;
					int v=(u-1);
					int w=(U==1||v>=U/2)?1:0;
					a1=0;
					a2=skip1;
					a3=skip1;
					b1=(x+w-same);
					b2=x*skip2+v*skip1;
					b3=b2+skip1;
				}
				if(nsew==0)
				{
					*(index++)	=i1+TilePosToIndex((s1-a1)	,b1			,s1+1);
					*(index++)	=i2+TilePosToIndex((s2-a2)	,b2			,s2+1);
					*(index++)	=i3+TilePosToIndex((s3-a3)	,b3			,s3+1);
				}												
				if(nsew==1)										
				{												
					*(index++)	=i2+TilePosToIndex((a2)		,b2			,s2+1);
					*(index++)	=i1+TilePosToIndex((a1)		,b1			,s1+1);
					*(index++)	=i3+TilePosToIndex((a3)		,b3			,s3+1);
				}
				if(nsew==2)
				{
					*(index++)	=i2+TilePosToIndex(b2		,(s2-a2)	,s2+1);
					*(index++)	=i1+TilePosToIndex(b1		,(s1-a1)	,s1+1);
					*(index++)	=i3+TilePosToIndex(b3		,(s3-a3)	,s3+1);
				}
				if(nsew==3)
				{
					*(index++)	=i1+TilePosToIndex(b1		,(a1)		,s1+1);
					*(index++)	=i2+TilePosToIndex(b2		,(a2)		,s2+1);
					*(index++)	=i3+TilePosToIndex(b3		,(a3)		,s3+1);
				}
			}
		}
		B_RETURN(edge->indexBuffer->Unlock());
	}
	else
	{
		edge->indexBuffer=0;
	}

	return (hr==S_OK);
}

bool SimulTerrainRenderer::BuildTile(TerrainTile *tile,int i,int j)
{
	int tile_size=heightMapInterface->GetTileSize();

	float X=((float)i)/((float)tiles.size());
	float Y=((float)j)/((float)tiles.size());
	simul::math::Vector3 pos;
	tile->offset[0]=(X)*heightMapInterface->GetPageWorldX();
	tile->offset[2]=0;
	tile->offset[1]=(Y)*heightMapInterface->GetPageWorldY();
	X=((float)i+0.5f)/((float)tiles.size());
	Y=((float)j+0.5f)/((float)tiles.size());
	tile->pos[0]=(X)*heightMapInterface->GetPageWorldX();
	tile->pos[2]=heightMapInterface->GetHeightAt(i*(tile_size-1)+tile_size/2,j*(tile_size-1)+tile_size/2);
	tile->pos[1]=(Y)*heightMapInterface->GetPageWorldY();

	if(y_vertical)
	{
		std::swap(tile->pos[1],tile->pos[2]);
		std::swap(tile->offset[1],tile->offset[2]);
	}
//	for(int m=0;m<heightMapInterface->GetNumMipMapLevels();m++)
//		BuildTileMip(tile,(int)i,(int)j,m);
	return true;
}

bool SimulTerrainRenderer::BuildTileMip(TerrainTile *tile,int i,int j,int mip_level)
{
	int tile_size=heightMapInterface->GetTileSize();
	int skip=1<<mip_level;
	TileMipInstance *mip=&(tile->mips[mip_level]);
	mip->num_verts=heightMapInterface->GetPageSize()*heightMapInterface->GetPageSize();
	simul::terrain::Cutout cutout;
#if 1
	if(road)
	{
		int s=12*road->GetNumSegments();
		simul::math::float2 dx;

		//RHS of road
		for(int a=0;a<s;a++)
		{
			simul::math::float3 x=road->GetCentreLine((float)a/(float)(s-1));
			simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(s-1));
			cutout.addvertex((const float*)(x+dx),x.z);
		}
		//LHS of road:
		for(int a=s-1;a>=0;a--)
		{
			simul::math::float3 x=road->GetCentreLine((float)a/(float)(s-1));
			simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(s-1));
			cutout.addvertex((const float*)(x-dx),x.z);
		}
	}
#else
	float rad=4500.f;
	for(int a=0;a<16;a++)
	{
		float angle=2.f*3.14159f*(float)a/(float)16;
		cutout.addvertex(simul::math::float2(rad*cos(angle),-rad*sin(angle)),0);
	}
#endif

	mip->ApplyCutouts(heightMapInterface,i,j,skip,&cutout);

	HRESULT hr=S_OK;

	tile_size-=1;
	tile_size>>=mip_level;
	tile_size+=1;
	if(mip->tri_strip)
	{
		if(tile_size>=2)
		{
			mip->num_prims=(tile_size-1)*2*(tile_size-3);
			mip->indexBuffer=NULL;
			for(int lower_level=mip_level;lower_level<heightMapInterface->GetNumMipMapLevels();lower_level++)
			{
				for(int nsew=0;nsew<4;nsew++)
				{
					
					TileMipInstance *mip=&(tile->mips[mip_level]);
					FourMipEdges *edges=NULL;
					int idx=lower_level-mip_level;
					if(idx>=(int)mip->edges.size())
					{
						mip->edges.push_back(FourMipEdges());
					}
					edges=&(mip->edges[idx]);
					MIPEdge *edge=&edges->edge[nsew];

					B_RETURN(BuildMIPEdge(edge,mip_level,lower_level,nsew));
				}
			}
		}
		else
		{
			mip->num_prims=0;
		}
	}
	else
	{
		if(mip->num_squares)
		{
			// Must create 
			mip->num_prims=6*mip->num_squares;
			B_RETURN(m_pd3dDevice->CreateIndexBuffer((unsigned)(mip->num_prims*sizeof(unsigned)),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
											  D3DPOOL_DEFAULT, &mip->indexBuffer,
											  NULL ));
			unsigned *indexData;
			B_RETURN(mip->indexBuffer->Lock(0, mip->num_prims, (void**)&indexData, 0 ));
			unsigned *index=indexData;
			for(int x=0;x<tile_size-1;x++)
			{
				for(int y=0;y<tile_size-1;y++)
				{
					if(!mip->unimpeded_squares[x][y])
						continue;
					*(index++)	=TilePosToIndex((x  )*skip,y*skip		,tile_size);
					*(index++)	=TilePosToIndex((x+1)*skip,(y+1)*skip	,tile_size);
					*(index++)	=TilePosToIndex((x+1)*skip,y*skip		,tile_size);

					*(index++)	=TilePosToIndex((x+1)*skip,(y+1)*skip	,tile_size);
					*(index++)	=TilePosToIndex((x  )*skip,y*skip		,tile_size);
					*(index++)	=TilePosToIndex((x  )*skip,(y+1)*skip	,tile_size);
				}
			}
			B_RETURN(mip->indexBuffer->Unlock());
		}

		if(mip->extra_vertices.size())
		{
			// Must create own vertex buffer:
			SAFE_RELEASE(mip->extraVertexBuffer);
			B_RETURN(m_pd3dDevice->CreateVertexBuffer((unsigned)(mip->extra_vertices.size()*sizeof(TerrainVertex_t)),
											D3DUSAGE_WRITEONLY,0,
										  D3DPOOL_DEFAULT, &mip->extraVertexBuffer,
										  NULL ));
			TerrainVertex_t *vertices;
			B_RETURN(mip->extraVertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
			TerrainVertex_t *V=vertices;
			for(size_t i=0;i<mip->extra_vertices.size();i++)
			{
				GetVertex(mip->extra_vertices[i].x,mip->extra_vertices[i].y,V);
				V->y=mip->extra_vertices[i].z;
				V++;
			}
			B_RETURN(mip->extraVertexBuffer->Unlock());
		}
		if(mip->extra_triangles.size())
		{
			// and the index buffer for these extra vertices:
			B_RETURN(m_pd3dDevice->CreateIndexBuffer((unsigned)(mip->extra_triangles.size()*sizeof(unsigned)),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
											  D3DPOOL_DEFAULT, &mip->extraIndexBuffer,
											  NULL ));
			unsigned *indexData;
			B_RETURN(mip->extraIndexBuffer->Lock(0,(unsigned)mip->extra_triangles.size(), (void**)&indexData, 0 ));
			unsigned *index=indexData;
			for(size_t i=0;i<mip->extra_triangles.size();i++)
			{
				*(index++)	=mip->extra_triangles[i];
			}
			B_RETURN(mip->extraIndexBuffer->Unlock());
		}
	}
	return (hr==S_OK);
}

SimulTerrainRenderer::TerrainTile::TerrainTile(int tilesize,int num_mips)
{
	for(int i=0;i<num_mips;i++)
	{
		mips.push_back(TileMipInstance(tilesize));
		tilesize>>=1;
	}
}

SimulTerrainRenderer::TileMipInstance::TileMipInstance(int tilesize)
	:indexBuffer(NULL)
	,extraVertexBuffer(NULL)
	,extraIndexBuffer(NULL)
	,num_prims(0)
{
	tile_size=tilesize;
	unimpeded_squares.resize(tilesize);
	num_squares=0;
	tri_strip=false;
	for(int i=0;i<tilesize;i++)
	{
		unimpeded_squares[i].resize(tilesize);
	}
}

void SimulTerrainRenderer::TileMipGeneric::Reset()
{
	SAFE_RELEASE(indexBuffer);
	num_prims=0;
	num_verts=0;
	for(size_t i=0;i<edges.size();i++)
		edges[i].Reset();
}

void SimulTerrainRenderer::TileMipInstance::Reset()
{
	if(!tri_strip)
		SAFE_RELEASE(indexBuffer);
	SAFE_RELEASE(extraVertexBuffer);
	SAFE_RELEASE(extraIndexBuffer);
	indexBuffer=NULL;
	num_prims=0;
	num_squares=0;
	num_verts=0;
	tri_strip=false;
	for(size_t i=0;i<edges.size();i++)
		edges[i].Reset();
}

void SimulTerrainRenderer::MIPEdge::Reset()
{
	SAFE_RELEASE(indexBuffer);
	num_tris=0;
}
void SimulTerrainRenderer::FourMipEdges::Reset()
{
	for(int i=0;i<4;i++)
		edge[i].Reset();
}

void SimulTerrainRenderer::RoadRenderable::Reset()
{
	SAFE_RELEASE(roadVertexBuffer);
	SAFE_RELEASE(roadIndexBuffer);
	roadVertexBuffer=NULL;
	roadIndexBuffer=NULL;
	num_verts=0;
}

static size_t loopindex(int idx,size_t length)
{
	while(idx<0)
		idx+=(int)length;
	return (size_t)(idx%(length));
}
// which squares are affected?
void SimulTerrainRenderer::TileMipInstance::ApplyCutouts(
	simul::terrain::HeightMapInterface *hmi,int i,int j,int step,const simul::terrain::Cutout *cutout)
{
	simul::math::float2 min_corner,max_corner;
	cutout->GetCorners(min_corner,max_corner);
	
	simul::math::float2 block_min,block_max;

	hmi->GetBlockCorners(i,j,block_min,block_max);

	if(!(block_min<max_corner&&block_max>min_corner)
		)
	{
		num_squares=0;
		tri_strip=true;
		for(int x=0;x<tile_size;x++)
		{
			for(int y=0;y<tile_size;y++)
			{
				this->unimpeded_squares[x][y]=true;
			}
		}
		return;
	}
	tri_strip=false;
	num_squares=0;
	for(int x=0;x<tile_size;x++)
	{
		for(int y=0;y<tile_size;y++)
		{
			this->unimpeded_squares[x][y]=true;
			simul::math::float2 square_min,square_max;
			hmi->GetVertexPos((i*tile_size+x)*step,(j*tile_size+y)*step,square_min);
			hmi->GetVertexPos((i*tile_size+x+1)*step,(j*tile_size+y+1)*step,square_max);
			if(square_min.x>max_corner.x||square_min.y>max_corner.y
				||square_max.x<min_corner.x||square_max.y<min_corner.y
				)
			{
				num_squares++;
				continue;
			}
			this->unimpeded_squares[x][y]=false;
			struct State
			{
				simul::math::float2 this_point,next_point;
				int Case;
				size_t corner;
				size_t index;
				int counter;
				simul::terrain::Cutout::Corner start_corner;
			};
			std::stack<State> remaining;
			std::stack<simul::terrain::Cutout> polygons;
			State state;
			simul::terrain::Cutout square;
			square.addvertex(square_min,hmi->GetHeightAt(square_min.x,square_min.y));
			square.addvertex(simul::math::float2(square_min.x,square_max.y),hmi->GetHeightAt(square_min.x,square_max.y));
			square.addvertex(square_max,hmi->GetHeightAt(square_max.x,square_max.y));
			square.addvertex(simul::math::float2(square_max.x,square_min.y),hmi->GetHeightAt(square_max.x,square_min.y));
			simul::terrain::Cutout building;
			state.Case=0;		// going along the edge of the square outside the cutout
			// firstly is the bottom-left corner inside or outside of the cutout?
			state.counter=0;
			state.start_corner=NULL;
			if(cutout->IsInside(square_min))
			{	
				this->unimpeded_squares[x][y]=false;
				state.counter=1;
				state.Case=1;		// going along the edge of the square inside the cutout
			}
			else	// add a vertex:
			{
				building.addvertex(square_min,hmi->GetHeightAt(square_min.x,square_min.y));
				state.start_corner=simul::terrain::Cutout::Corner(&square.vertex(0),&square.vertex(-1));
			}
			state.corner=0;
			state.index=0;
			state.this_point=square.vertex((int)state.corner).pos;
			state.next_point=square.vertex((int)state.corner+1).pos;
			int direction=1;// anticlockwise.
			cutout->unuse();
			while(state.corner<4)
			{
				simul::math::float2 intersect;
				// go round the square clockwise, looking for intersections.
				// crossover=1 means entering - use the anti-clockwise direction, 1 means clockwise.
				int edge=-1;
				int crossover=0;
				bool finished=false;
				if(state.Case==0)
				{
					square.unuse();
					square.vertex((int)state.index).used=true;
					crossover=cutout->GetCrossover(state.this_point,state.next_point,intersect,edge);
					if(crossover==0)
					{
						cutout->unuse();
						state.corner++;
						state.index++;
						if(state.corner<4)
							building.addvertex(state.next_point,hmi->GetHeightAt(state.next_point.x,state.next_point.y));
						state.this_point=state.next_point;
						state.next_point=square.vertex((int)state.corner+1).pos;
					}
					// we've entered the polygon. 
					else if(!state.counter&&crossover==1)
					{
						simul::terrain::Cutout::Corner this_corner(&cutout->vertex(edge),&square.vertex((int)state.corner));
						// was this where we started?
						if(state.start_corner==this_corner)
						{
							finished=true;
						}
						else
						{
							// The state branches here. The first branch moves along the outside of the cutout.
							// The second branch moves on the square in State 1.
							{
								remaining.push(State());
								State &branch=remaining.top();
								branch.this_point=intersect;
								branch.next_point=state.next_point;
								branch.Case=1;
								branch.counter=1;
								branch.corner=state.corner;
								branch.index=branch.corner;
								branch.start_corner=this_corner;

							}
							{
								building.addvertex(intersect,cutout->GetHeightAt(edge,intersect));
								state.index=edge;
								if(crossover>0)
									state.index++;
								direction=-crossover;
								cutout->unuse();
								cutout->vertex((int)state.index-((direction<0)?1:0)).used=true;
								state.this_point=intersect;
								if(!state.counter)
								{
									state.Case=2;		// going along the edge of the cutout
									// next point along cutout.
									state.next_point=cutout->vertex((int)state.index+direction).pos;
								}
							}
						}
						unimpeded_squares[x][y]=false;
						state.counter++;
					}
					else
					{
						//ignore this edge:
						cutout->vertex(edge).used=true;
					}
				}
				else if(state.Case==2)
				{
					// do we hit an edge of the square?
					crossover=square.GetCrossover(state.this_point,state.next_point,intersect,edge);
					square.unuse();
					if(crossover==0)
					{
						building.addvertex(state.next_point,cutout->GetHeightAt((int)state.index-(direction<0),state.next_point));
						state.index+=direction;
						cutout->unuse();
						cutout->vertex((int)state.index-((direction<0)?1:0)).used=true;
						state.this_point=state.next_point;
						state.next_point=cutout->vertex((int)state.index+direction).pos;
					}
					else if(state.counter&&crossover==-1)
					{
						if(edge<(int)state.corner)
							edge+=4;
						state.this_point=intersect;
						state.counter--;
						if(!state.counter)
						{
							simul::terrain::Cutout::Corner this_corner(&cutout->vertex((int)state.index-(direction<0)),&square.vertex(edge));
							// was this where we started?
							if(state.start_corner==this_corner)
							{
								finished=true;
							}
							state.corner=edge;
							if(state.corner<4)
								building.addvertex(intersect,cutout->GetHeightAt((int)state.index-(direction<0),intersect));
							state.Case=0;		// going along the edge of the square
							state.next_point=square.vertex(edge-crossover).pos;	// next point along cutout.
						}
						else
							building.addvertex(intersect,cutout->GetHeightAt((int)state.index,intersect));
						state.index=edge;
						direction=1;
					}
					else
						DebugBreak();
				}
				else //Case=1
				{
					square.vertex((int)state.index).used=true;
					crossover=cutout->GetCrossover(state.this_point,state.next_point,intersect,edge);
					square.unuse();
					if(crossover==0)
					{
						cutout->unuse();
						state.corner++;
						state.index++;
						state.this_point=state.next_point;
						state.next_point=square.vertex((int)state.corner+1).pos;
					}
					// we've exited the polygon. 
					else if(state.counter&&crossover==-1)
					{
						simul::terrain::Cutout::Corner this_corner(&cutout->vertex(edge),&square.vertex((int)state.index));
						building.addvertex(intersect,cutout->GetHeightAt(edge,intersect));
						cutout->unuse();
						cutout->vertex(edge).used=true;
						direction=1;
						state.this_point=intersect;
						state.counter--;
						if(!state.counter)
						{
							state.Case=0;		// going along the edge of the square
							state.start_corner=this_corner;
							
						}
						this->unimpeded_squares[x][y]=false;
					}
					else
					{
						//ignore this edge:
						cutout->vertex(edge).used=true;
					}
				}
				if(state.corner>=4||finished)
				{
					polygons.push(building);
					building.Clear();
					if(remaining.size())
					{
						state=remaining.top();
						remaining.pop();
					}
				}
			}
			if(unimpeded_squares[x][y])
				num_squares++;
			else
			{
				while(polygons.size())
				{
					simul::terrain::Cutout& building=polygons.top();
					// eliminate close duplicates
					for(size_t i=0;i<building.NumVertices();i++)
					{
						simul::math::float2 diff=building.vertex((int)i).pos-building.vertex((int)i+1).pos;
						// merge values that are less than 1mm apart:
						if(simul::math::float2::length(diff)<0.1f)
						{
						//	building.vertices.erase(building.vertices.begin()+i);
						//	i--;
						}
					}
					std::vector<int> indices;
					for(size_t i=0;i<building.NumVertices();i++)
					{
						indices.push_back((int)extra_vertices.size());
						extra_vertices.push_back(simul::math::float3(
							building.vertex((int)i).pos.x,building.vertex((int)i).pos.y,
							building.vertex((int)i).height));
					}
					//triangulate the "building" polygon:
					while(building.NumVertices()>=3)
					{
						int vert_ear=(int)building.FindEar();
						if(vert_ear<0)
						{
							building.FindEar();
							break;
						}
						// the new triangle is between vert_ear and the two adjacent vertices.
						extra_triangles.push_back(indices[vert_ear]);
						extra_triangles.push_back(indices[loopindex(vert_ear+1,indices.size())]);
						extra_triangles.push_back(indices[loopindex(vert_ear-1,indices.size())]);
						building.RemoveVertex(vert_ear);
						indices.erase(indices.begin()+vert_ear);
						
					}
					if(building.NumVertices()==3)
					{
						for(int i=0;i<(int)building.NumVertices();i++)
						{
							extra_triangles.push_back(indices[i]);
						}
					}
					else
					{
					}
					polygons.pop();
				}
			}
		}
	}
}

void SimulTerrainRenderer::TerrainTile::Reset()
{
	for(size_t i=0;i<mips.size();i++)
	{
		mips[i].Reset();
	}
}