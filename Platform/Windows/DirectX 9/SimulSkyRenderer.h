// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulSkyRenderer.h A renderer for skies.

#pragma once
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Sky/Float4.h"
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include <map>

namespace simul
{
	namespace base
	{
		class Referenced;
	}
	namespace graph
	{
		namespace meta
		{
			class Node;
		}
	}
	namespace sky
	{
		class SkyInterface;
		class SiderealSkyInterface;
		class SkyNode;
		class SkyKeyframer;
		class FadeTableInterface;
		class OvercastCallback;
	}
}

typedef long HRESULT;

//! A renderer for skies, this class will manage an instance of simul::sky::SkyNode and use it to calculate sky colours
//! in real time for the simul_sky.fx shader.

class SimulSkyRenderer : public simul::sky::BaseSkyRenderer
{
public:
	SimulSkyRenderer(bool UseColourSky=false);

	virtual ~SimulSkyRenderer();

	//standard d3d object interface functions
	//! Call this when the D3D device has been created or reset.
	HRESULT RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the D3D device has been shut down.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	bool						RenderPlanet(void* tex,float rad,const float *dir,const float *colr,bool do_lighting);
	HRESULT						RenderSun();
	//! Get the transform that goes from declination/right-ascension to azimuth and elevation.
	HRESULT GetSiderealTransform(D3DXMATRIX *world);
	//! Render the stars, as points.
	HRESULT						RenderPointStars();
	//! Render the stars, as a background.
	HRESULT						RenderStars();
	//! Call this to draw the sky, usually to the SimulWeatherRenderer's render target.
	HRESULT						Render();
	//! Call this to draw the sun flare, usually drawn last, on the main render target.
	HRESULT						RenderFlare(float exposure);
	//! Draw the fade textures to screen
	HRESULT						RenderFades(int width);
	//! Draw sidereal and geographic information to screen
	HRESULT						RenderCelestialDisplay(int screen_width,int screen_height);
	//! Get a value, from zero to one, which represents how much of the sun is visible.
	//! Call this when the current rendering surface is the one that has obscuring
	//! objects like mountains etc. in it, and make sure these have already been drawn.
	//! GetSunOcclusion executes a pseudo-render of an invisible billboard, then
	//! uses a hardware occlusion query to see how many pixels have passed the z-test.
	void CalcSunOcclusion(float cloud_occlusion=0.f);
#ifdef XBOX
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
#endif
	void SetTime(float hour);
	//! Set the overcast factor for the horizon. Zero is clear skies, one is cloudy.
	//! This factor then goes into sky brightness and fade calculations.
	void SetOvercastFactor(float of);
	//! A timing measurement.
	float GetTiming() const;
	simul::sky::float4 GetAmbient() const;
	simul::sky::float4 GetLightColour() const;
	//! This sets the base altitude and range for the overcast effect - the base should be the cloudbase,
	//! while the range should be the height of the clouds.
	void SetOvercastBaseAndRange(float base_alt_km,float range_km);
	void GetLossAndInscatterTextures(LPDIRECT3DBASETEXTURE9 *l1,LPDIRECT3DBASETEXTURE9 *l2,
		LPDIRECT3DBASETEXTURE9 *i1,LPDIRECT3DBASETEXTURE9 *i2);
	void GetSkyTextures(LPDIRECT3DBASETEXTURE9 *s1,LPDIRECT3DBASETEXTURE9 *s2);
	bool Use3DFadeTextures() const{return true;}
	float GetFadeInterp() const;
	void SetStepsPerDay(int s);
//! Implement the SkyTexturesCallback
	void SetSkyTextureSize(unsigned size);
	void SetFadeTextureSize(unsigned width,unsigned height,unsigned num_altitudes);
	void FillSkyTexture(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array);
	void FillFadeTexturesSequentially(int texture_index,int texel_index,int num_texels,
						const float *loss_float4_array,
						const float *inscatter_float4_array);
	void FillFadeTextureBlocks(int,int,int,int,int,int,int,
						const float *,
						const float *)
	{
	}

	void FillSunlightTexture(int texture_index,int texel_index,int num_texels,const float *float4_array);
	void CycleTexturesForward();
	const char *GetDebugText() const;
	void SetFlare(LPDIRECT3DTEXTURE9 tex,float rad);
protected:
	struct StarVertext
	{
		float x,y,z;
		float b,c;
	};
	StarVertext *star_vertices;
	HRESULT PrintAt(const float *p,const wchar_t *text,int screen_width,int screen_height);
	bool external_flare_texture;
	float timing;
	D3DFORMAT sky_tex_format;
	void CreateFadeTextures();

	LPDIRECT3DDEVICE9			m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pHudVertexDecl;

	LPDIRECT3DVERTEXDECLARATION9 m_pVtxDecl;
	LPD3DXEFFECT				m_pSkyEffect;		// The fx file for the sky
	D3DXHANDLE					worldViewProj;
	D3DXHANDLE                  m_hTechniqueSky;	// Handle to technique in the effect 
	D3DXHANDLE					m_hTechniqueStarrySky;
	D3DXHANDLE					m_hTechniquePointStars;
	D3DXHANDLE					m_hTechniquePlainColour;
	D3DXHANDLE					m_hTechniqueSun;
	D3DXHANDLE					m_hTechniqueQuery;	// A technique that uses the z-test for occlusion queries
	D3DXHANDLE					m_hTechniqueFlare;
	D3DXHANDLE					m_hTechniquePlanet;
	D3DXHANDLE					m_hTechniqueFadeCrossSection;
	D3DXHANDLE					altitudeTexCoord;
	D3DXHANDLE					lightDirection;
	D3DXHANDLE					mieRayleighRatio;
	D3DXHANDLE					hazeEccentricity;
	D3DXHANDLE					skyInterp;
	D3DXHANDLE					colour;
	D3DXHANDLE					flareTexture;
	D3DXHANDLE					fadeTexture;
	D3DXHANDLE					skyTexture1;
	D3DXHANDLE					skyTexture2;
	D3DXHANDLE					starsTexture;
	D3DXHANDLE					starBrightness;
	
	LPDIRECT3DTEXTURE9			flare_texture;
	LPDIRECT3DTEXTURE9			stars_texture;
	std::map<int,LPDIRECT3DTEXTURE9> planet_textures;
	std::vector<LPDIRECT3DTEXTURE9> halo_textures;
	// Three sky textures - 2 to interpolate and one to fill
	LPDIRECT3DTEXTURE9			sky_textures[3];
	// Three sunlight textures.
	LPDIRECT3DTEXTURE9			sunlight_textures[3];
	// If using 1D sky textures and 2D fade textures:
	LPDIRECT3DBASETEXTURE9		loss_textures[3];
	LPDIRECT3DBASETEXTURE9		inscatter_textures[3];
	
    ID3DXFont*					m_pFont;
	D3DXVECTOR3					cam_dir;
	D3DXMATRIX					world,view,proj;
	LPDIRECT3DQUERY9			d3dQuery;
	HRESULT						UpdateSkyTexture(float proportion);
	HRESULT						CreateSkyTextures();
	HRESULT						CreateSunlightTextures();
	HRESULT						CreateSkyEffect();
	HRESULT						RenderAngledQuad(D3DXVECTOR4 dir,float half_angle_radians);
	virtual bool IsYVertical()
	{
		return true;
	}
};