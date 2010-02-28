// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulSkyRenderer.h A renderer for skies.

#pragma once
#include "Simul/Base/SmartPtr.h"
#include "Simul/Sky/FadeTableCallback.h"
#include "Simul/Sky/Float4.h"
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif

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
		class SkyNode;
		class InterpolatedFadeTable;
		class FadeTableInterface;
	}
}

typedef long HRESULT;

//! A renderer for skies, this class will manage an instance of simul::sky::SkyNode and use it to calculate sky colours
//! in real time for the simul_sky.fx shader.

class SimulSkyRenderer:public simul::sky::FadeTableCallback
{
public:
	SimulSkyRenderer(bool UseColourSky);
	virtual ~SimulSkyRenderer();
	//! Get the interface to the sky object so that other classes can use it for lighting, distance fades etc.
	simul::sky::SkyInterface *GetSkyInterface();
	simul::sky::FadeTableInterface *GetFadeTableInterface();
	simul::sky::InterpolatedFadeTable *GetFadeTable();

	//standard d3d object interface functions
	//! Call this when the D3D device has been created or reset.
	HRESULT RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the D3D device has been shut down.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	//! Call this once per frame to update the sky.
	void Update(float dt);
	//! Call this to draw the sky, usually to the SimulWeatherRenderer's render target.
	HRESULT Render();
	//! Call this to draw the sun flare, usually drawn last, on the main render target.
	HRESULT RenderFlare(float exposure);
	//! Get a value, from zero to one, which represents how much of the sun is visible.
	//! Call this when the current rendering surface is the one that has obscuring
	//! objects like mountains etc. in it, and make sure these have already been drawn.
	//! GetSunOcclusion executes a pseudo-render of an invisible billboard, then
	//! uses a hardware occlusion query to see how many pixels have passed the z-test.
	void CalcSunOcclusion(float cloud_occlusion=0.f);
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
	//! Set a multiplier for time steps - default is 1.0
	void SetTimeMultiplier(float tm);
	//! Set the overcast factor for the horizon. Zero is clear skies, one is cloudy.
	//! This factor then goes into sky brightness and fade calculations.
	void SetOvercastFactor(float of);
	//! A timing measurement.
	float GetTiming() const;
	//! This sets the base altitude and range for the overcast effect - the base should be the cloudbase,
	//! while the range should be the height of the clouds.
	void SetOvercastBaseAndRange(float base_alt_km,float range_km);
	void GetLossAndInscatterTextures(LPDIRECT3DBASETEXTURE9 *l1,LPDIRECT3DBASETEXTURE9 *l2,
		LPDIRECT3DBASETEXTURE9 *i1,LPDIRECT3DBASETEXTURE9 *i2);
	float GetAltitudeTextureCoordinate() const;
	bool Use3DFadeTextures() const{return true;}
	float GetFadeInterp() const;
	void SetStepsPerDay(unsigned steps);
//! Implement the FadeTableCallback
	void SetSkyTextureSize(unsigned size);
	void SetFadeTextureSize(unsigned width,unsigned height,unsigned num_altitudes);
	void FillSkyTexture(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array);
	void FillFadeTextures(int alt_index,int texture_index,int texel_index,int num_texels,
						const float *loss_float4_array,
						const float *inscatter_float4_array);
	void FillSunlightTexture(int texture_index,int texel_index,int num_texels,const float *float4_array);
	void CycleTexturesForward();
	const char *GetDebugText() const;
protected:
	float timing;
	D3DFORMAT sky_tex_format;
	float sun_occlusion;
	simul::sky::float4 sunlight;
	simul::base::SmartPtr<simul::graph::meta::Node> skyNode;
	simul::base::SmartPtr<simul::sky::InterpolatedFadeTable> fadeTable;

	simul::sky::SkyInterface *skyInterface;
	simul::sky::FadeTableInterface *fadeTableInterface;
	unsigned skyTexSize;
	unsigned skyTexIndex;
	unsigned numAltitudes;
	float interp;
	float interp_step_time;
	float interp_time_1;
	void CreateFadeTextures();
	unsigned fadeTexWidth,fadeTexHeight;
	LPDIRECT3DDEVICE9			m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9 m_pVtxDecl;
	LPD3DXEFFECT				m_pSkyEffect;		// The fx file for the sky
	D3DXHANDLE					worldViewProj;
	D3DXHANDLE                  m_hTechniqueSky;	// Handle to technique in the effect 
	D3DXHANDLE					m_hTechniqueSun;
	D3DXHANDLE					m_hTechniqueQuery;	// A technique that uses the z-test for occlusion queries
	D3DXHANDLE					m_hTechniqueFlare;
	D3DXHANDLE					m_hTechniquePlanet;
	D3DXHANDLE					altitudeTexCoord;
	D3DXHANDLE					lightDirection;
	D3DXHANDLE					MieRayleighRatio;
	D3DXHANDLE					hazeEccentricity;
	D3DXHANDLE					skyInterp;
	D3DXHANDLE					colour;
	D3DXHANDLE					flareTexture;
	D3DXHANDLE					skyTexture1;
	D3DXHANDLE					skyTexture2;
	LPDIRECT3DTEXTURE9			flare_texture;
	LPDIRECT3DTEXTURE9			moon_texture;
	// Three sky textures - 2 to interpolate and one to fill
	LPDIRECT3DTEXTURE9			sky_textures[3];
	// Three sunlight textures.
	LPDIRECT3DTEXTURE9			sunlight_textures[3];
	// If using 1D sky textures and 2D fade textures:
	LPDIRECT3DTEXTURE9			loss_textures[3];
	LPDIRECT3DTEXTURE9			inscatter_textures[3];
	// If using 2D sky textures and 3D fade textures (i.e. altitude is an extra dimension):
	LPDIRECT3DVOLUMETEXTURE9	loss_textures_3d[3];
	LPDIRECT3DVOLUMETEXTURE9	inscatter_textures_3d[3];
	D3DXVECTOR3					cam_pos;
	D3DXMATRIX					world,view,proj;
	LPDIRECT3DQUERY9			d3dQuery;
	HRESULT						UpdateSkyTexture(float proportion);
	HRESULT						CreateSkyTextures();
	HRESULT						CreateSunlightTextures();
	HRESULT						CreateSkyEffect();
	HRESULT						RenderAngledQuad(D3DXVECTOR4 dir,float half_angle_radians);
	HRESULT						RenderMoon();
	HRESULT						RenderSun();
};