// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulObjectRenderer.h A renderer for rain, snow etc.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif

#include <string>
typedef long HRESULT;

namespace simul
{
	namespace sky
	{
		class SkyInterface;
	}
	namespace clouds
	{
		class CloudInterface;
	}
}
//! A renderer for Object geometry loaded from a .x file. This a a simple sample renderer to 
//! show how Simul Sky and Clouds can be integrated with geometry rendering. In practice,
//! distance-fading, exposure and gamma-correction should usually be done as a post-processing step,
//! but in this case they have been integrated into the object shader.
class SimulObjectRenderer
{
public:
	SimulObjectRenderer(const char *filename);
	virtual ~SimulObjectRenderer();
	//! Call this once to set the sky interface that this cloud renderer can use for distance fading.
	void SetSkyInterface(simul::sky::SkyInterface *si);
	//! Call this once to set the cloud interface that casts shadows.
	void SetCloudInterface(simul::clouds::CloudInterface *ci);
	//! Standard d3d object interface functions
	HRESULT RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Standard d3d object interface functions
	HRESULT InvalidateDeviceObjects();
	//! Tell the renderer the textures to use for distance fades. Interpolate between these two textures using fade_interp.
	void SetLossTextures(LPDIRECT3DTEXTURE9 t1,LPDIRECT3DTEXTURE9 t2){sky_loss_texture_1=t1;sky_loss_texture_2=t2;}
	void SetInscatterTextures(LPDIRECT3DTEXTURE9 t1,LPDIRECT3DTEXTURE9 t2){sky_inscatter_texture_1=t1;sky_inscatter_texture_2=t2;}
	void SetCloudTextures(LPDIRECT3DVOLUMETEXTURE9 t1,LPDIRECT3DVOLUMETEXTURE9 t2){cloud_texture_1=t1;cloud_texture_2=t2;}
	HRESULT Render();
	//! Call this before rendering.
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetFadeInterpolation(float f)
	{
		fade_interp=f;
	}
	void SetCloudInterpolation(float f)
	{
		cloud_interp=f;
	}
	void SetExposure(float f)
	{
		exposure=f;
	}
	void SetOvercastFactor(float f)
	{
		overcast_factor=f;
	}
protected:
	HRESULT Destroy();
	std::string filename;
	simul::sky::SkyInterface *skyInterface;
	simul::clouds::CloudInterface *cloudInterface;
	LPDIRECT3DDEVICE9		m_pd3dDevice;
	LPD3DXEFFECT			m_pEffect;			// The fx file for the sky
	D3DXHANDLE				worldViewProj;
	D3DXHANDLE				worldparam;
	D3DXHANDLE              m_hTechnique;		// Handle to technique in the effect 
	D3DXMATRIX				world,view,proj;

	D3DXHANDLE				hazeEccentricity;
	D3DXHANDLE				lightDir;
	D3DXHANDLE				eyePosition;
	D3DXHANDLE				fadeInterp;
	D3DXHANDLE				cloudInterp;
	D3DXHANDLE				overcast;
	D3DXHANDLE				exposureParam;
	D3DXHANDLE				sunlightColour;
	D3DXHANDLE				ambientColour;
	D3DXHANDLE				mieRayleighRatio;
	D3DXHANDLE				skyLossTexture1;
	D3DXHANDLE				skyLossTexture2;
	D3DXHANDLE				cloudTexture1;
	D3DXHANDLE				cloudTexture2;
	D3DXHANDLE				skyInscatterTexture1;
	D3DXHANDLE				skyInscatterTexture2;
D3DXHANDLE cloudCorner;
D3DXHANDLE cloudScales;
D3DXHANDLE cloudLightVector;
	LPDIRECT3DTEXTURE9		sky_loss_texture_1;
	LPDIRECT3DTEXTURE9		sky_loss_texture_2;
	LPDIRECT3DTEXTURE9		sky_inscatter_texture_1;
	LPDIRECT3DTEXTURE9		sky_inscatter_texture_2;
	LPDIRECT3DVOLUMETEXTURE9		cloud_texture_1;
	LPDIRECT3DVOLUMETEXTURE9		cloud_texture_2;
	class CDXUTXFileMesh*		mesh;
	float fade_interp;
	float cloud_interp;
	float exposure;
	float overcast_factor;
	float ambient_light[4];
};