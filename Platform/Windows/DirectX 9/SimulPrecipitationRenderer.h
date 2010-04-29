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
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
typedef long HRESULT;
class SimulPrecipitationRenderer
{
public:
	SimulPrecipitationRenderer();
	virtual ~SimulPrecipitationRenderer();
	//standard d3d object interface functions:
	//! Call this when the D3D device has been created or reset.
	HRESULT RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the D3D device has been shut down.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	//! Call this once per frame to update the clouds.
	void Update(float dt);
	//! Call this to draw the clouds, including any illumination by lightning.
	HRESULT Render();
#ifdef XBOX
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
#endif
	//! Set the strength of the rain or precipitation effect.
	void SetIntensity(float i)
	{
		rain_intensity=i;
	}
	//! Set the colour of light, e.g. sunlight, that illuminates the precipitation.
	void SetLightColour(const float c[4]);
	// Set a texture not created by this class to be used:
	HRESULT SetExternalRainTexture(LPDIRECT3DTEXTURE9 tex);
protected:
	LPDIRECT3DDEVICE9		m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9 m_pVtxDecl;
	LPD3DXEFFECT			m_pRainEffect;		// The fx file for the sky
	LPDIRECT3DTEXTURE9		rain_texture;
	D3DXHANDLE				worldViewProj;
	D3DXHANDLE				offset;
	D3DXHANDLE				lightColour;
	D3DXHANDLE				intensity;
	D3DXHANDLE              m_hTechniqueRain;	// Handle to technique in the effect 
	D3DXMATRIX				world,view,proj;
	D3DXVECTOR3				cam_pos;
	float radius,height,offs,rain_intensity;
	float light_colour[4];
	bool external_rain_texture;		// Is the rain texture created outside this class?
};