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
	#include <D3D11.h>
	#include <d3DX11.h>
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
	HRESULT Create( ID3D11Device* pd3dDevice);
	HRESULT RestoreDeviceObjects( ID3D11Device* pd3dDevice);
	HRESULT InvalidateDeviceObjects();
	HRESULT Destroy();

	virtual ~SimulTerrainRenderer();
	HRESULT Render();
	void Update(float dt);
	void SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetLossTextures(ID3D11Texture2D* t1,ID3D11Texture2D* t2){sky_loss_texture_1=t1;sky_loss_texture_2=t2;}
	void SetInscatterTextures(ID3D11Texture2D* t1,ID3D11Texture2D* t2){sky_inscatter_texture_1=t1;sky_inscatter_texture_2=t2;}
	void SetCloudTextures(ID3D11Texture3D* *t){cloud_textures=t;}
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
	ID3D11Device*		m_pd3dDevice;
	ID3D11InputLayout* m_pVtxDecl;
	ID3DX11Effect*			m_pTerrainEffect;		// The fx file for the sky
	ID3D11Texture2D*		terrain_texture;
	ID3D11Texture2D*		grass_texture;
	ID3D11Texture2D*		sky_loss_texture_1;
	ID3D11Texture2D*		sky_loss_texture_2;
	ID3D11Texture2D*		sky_inscatter_texture_1;
	ID3D11Texture2D*		sky_inscatter_texture_2;
	ID3D11Texture2D*		road_texture;
	ID3D11Texture3D* *cloud_textures;
	ID3DX11EffectVariable*				worldViewProj;
	ID3DX11EffectVariable*				param_world;
	ID3DX11EffectTechnique*	m_hTechniqueTerrain;	// Handle to technique in the effect 
	ID3DX11EffectTechnique*	techniqueRoad;			// Handle to technique in the effect 
	ID3DX11EffectVariable*				eyePosition;
	ID3DX11EffectVariable*				lightDirection;
	ID3DX11EffectVariable*				MieRayleighRatio;
	ID3DX11EffectVariable*				hazeEccentricity;
	ID3DX11EffectVariable*				overcastFactor;
	ID3DX11EffectVariable*				cloudScales;
	ID3DX11EffectVariable*				cloudOffset;
	ID3DX11EffectVariable*				cloudInterp;
	ID3DX11EffectVariable*				fadeInterp;
	ID3DX11EffectVariable*				exposureParam;

	ID3DX11EffectVariable*				lightColour;
	ID3DX11EffectVariable*				ambientColour;

	ID3DX11EffectVariable*				g_mainTexture;
	ID3DX11EffectVariable*				grassTexture;
	ID3DX11EffectVariable*				roadTexture;
	ID3DX11EffectVariable*				skyLossTexture1;
	ID3DX11EffectVariable*				skyLossTexture2;
	ID3DX11EffectVariable*				skyInscatterTexture1;
	ID3DX11EffectVariable*				skyInscatterTexture2;
	ID3DX11EffectVariable*				cloudTexture1;
	ID3DX11EffectVariable*				cloudTexture2;
	
	D3DXMATRIX				world,view,proj;
	D3DXVECTOR3				cam_pos;
	ID3D11Buffer			*vertexBuffer;

	simul::math::Vector3 dir_to_sun;
	struct TerrainTileMIP
	{
		unsigned num_prims;
		unsigned num_verts;
		ID3D11Buffer* indexBuffer;
		ID3D11Buffer* extraVertexBuffer;
		ID3D11Buffer* extraIndexBuffer;
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
		LPDIRECT3DVERTEXBUFFER9	roadVertexBuffer;
		LPDIRECT3DINDEXBUFFER9 roadIndexBuffer;
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