// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d10.h>
	#include <d3dx10.h>
#endif
typedef long HRESULT;
#include <vector>
#include "Simul/Math/float3.h"
#include "Simul/Math/Vector3.h"
namespace simul
{
	namespace sky
	{
		class SkyInterface;
	}
	namespace terrain
	{
		class HeightMapInterface;
		struct Cutout;
	}
}
class SimulTerrainRenderer
{
public:
	SimulTerrainRenderer();
	//standard d3d object interface functions
	HRESULT Create( ID3D10Device* pd3dDevice);
	HRESULT RestoreDeviceObjects( ID3D10Device* pd3dDevice);
	HRESULT InvalidateDeviceObjects();
	HRESULT Destroy();

	virtual ~SimulTerrainRenderer();
	HRESULT Render();
	void Update(float dt);
	void SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetLossTextures(ID3D10Texture2D* t1,ID3D10Texture2D* t2){sky_loss_texture_1=t1;sky_loss_texture_2=t2;}
	void SetInscatterTextures(ID3D10Texture2D* t1,ID3D10Texture2D* t2){sky_inscatter_texture_1=t1;sky_inscatter_texture_2=t2;}
	void SetCloudTextures(ID3D10Texture3D* *t){cloud_textures=t;}
	void SetSkyInterface(simul::sky::SkyInterface *si){skyInterface=si;}
	simul::terrain::HeightMapInterface *GetHeightMapInterface();
	void SetOvercastFactor(float of)
	{
		overcast_factor=of;
	}
	void SetCloudScales(const float *s)
	{
		cloud_scales[0]=s[0];
		cloud_scales[1]=s[1];
		cloud_scales[2]=s[2];
	}
	void SetCloudOffset(const float *s)
	{
		cloud_offset[0]=s[0];
		cloud_offset[1]=s[1];
		cloud_offset[2]=s[2];
	}
	void setCloudInterpolation(float s)
	{
		cloud_interp=s;
	}
	void setFadeInterpolation(float s)
	{
		fade_interp=s;
	}
	void SetExposure(float e)
	{
		exposure=e;
	}
protected:
	simul::sky::SkyInterface *skyInterface;
	ID3D10Device*		m_pd3dDevice;
	ID3D10InputLayout* m_pVtxDecl;
	ID3D10Effect*			m_pTerrainEffect;		// The fx file for the sky
	ID3D10Texture2D*		terrain_texture;
	ID3D10Texture2D*		grass_texture;
	ID3D10Texture2D*		sky_loss_texture_1;
	ID3D10Texture2D*		sky_loss_texture_2;
	ID3D10Texture2D*		sky_inscatter_texture_1;
	ID3D10Texture2D*		sky_inscatter_texture_2;
	ID3D10Texture2D*		road_texture;
	ID3D10Texture3D* *cloud_textures;
	ID3D10EffectVariable*				worldViewProj;
	ID3D10EffectVariable*				param_world;
	ID3D10EffectTechnique*	m_hTechniqueTerrain;	// Handle to technique in the effect 
	ID3D10EffectTechnique*	techniqueRoad;			// Handle to technique in the effect 
	ID3D10EffectVariable*				eyePosition;
	ID3D10EffectVariable*				lightDirection;
	ID3D10EffectVariable*				MieRayleighRatio;
	ID3D10EffectVariable*				hazeEccentricity;
	ID3D10EffectVariable*				overcastFactor;
	ID3D10EffectVariable*				cloudScales;
	ID3D10EffectVariable*				cloudOffset;
	ID3D10EffectVariable*				cloudInterp;
	ID3D10EffectVariable*				fadeInterp;
	ID3D10EffectVariable*				exposureParam;

	ID3D10EffectVariable*				lightColour;
	ID3D10EffectVariable*				ambientColour;

	ID3D10EffectVariable*				g_mainTexture;
	ID3D10EffectVariable*				grassTexture;
	ID3D10EffectVariable*				roadTexture;
	ID3D10EffectVariable*				skyLossTexture1;
	ID3D10EffectVariable*				skyLossTexture2;
	ID3D10EffectVariable*				skyInscatterTexture1;
	ID3D10EffectVariable*				skyInscatterTexture2;
	ID3D10EffectVariable*				cloudTexture1;
	ID3D10EffectVariable*				cloudTexture2;
	
	D3DXMATRIX				world,view,proj;
	D3DXVECTOR3				cam_pos;
	ID3D10Buffer			*vertexBuffer;

	simul::math::Vector3 dir_to_sun;
	struct TerrainTileMIP
	{
		unsigned num_prims;
		unsigned num_verts;
		ID3D10Buffer* indexBuffer;
		ID3D10Buffer* extraVertexBuffer;
		ID3D10Buffer* extraIndexBuffer;
		typedef std::vector<bool> BitMask;
		typedef std::vector<BitMask> BitMap;
		std::vector<simul::math::float3> extra_vertices;
		std::vector<int> extra_triangles;
		BitMap unimpeded_squares;
		int num_squares;
		bool tri_strip;
		TerrainTileMIP(int tilesize);

		int tile_size;
		void Reset();
		void ApplyCutouts(simul::terrain::HeightMapInterface *hmi,int i,int j,int step,const simul::terrain::Cutout *cutout);
	};
	struct RoadRenderable
	{
		LPDIRECT3DVERTEXBUFFER10 roadVertexBuffer;
		LPDIRECT3DINDEXBUFFER10 roadIndexBuffer;
		int num_verts;
		void Reset();
	};
	std::vector < RoadRenderable  > roads;
	struct TerrainTile
	{
		std::vector<TerrainTileMIP> mips;
		TerrainTile(int tilesize,int num_mips);
		void Reset();
		float pos[3];
	};
	typedef std::vector < TerrainTile  > TerrainRow;
	typedef std::vector < TerrainRow > Terrain2D;

	HRESULT BuildRoad();
	HRESULT BuildTile(TerrainTile *tile,int i,int j,int mip_level);
	void ReleaseIndexBuffers();

	Terrain2D tiles;
	int TilePosToIndex(int i,int j,int x,int y) const;
	float overcast_factor;

	float cloud_scales[3];
	float cloud_offset[3];
	float cloud_interp;
	float fade_interp;
	void GetVertex(int i,int j,struct TerrainVertex_t *V);
	void GetVertex(float x,float y,struct TerrainVertex_t *V);

	float exposure;
};