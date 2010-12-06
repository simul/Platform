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
class SimulPrecipitationRenderer
{
public:
	SimulPrecipitationRenderer();
	virtual ~SimulPrecipitationRenderer();
	//standard d3d object interface functions:
	//! Call this when the D3D device has been created or reset.
	HRESULT RestoreDeviceObjects( ID3D11Device* pd3dDevice);
	//! Call this when the D3D device has been shut down.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	//! Call this once per frame to update the clouds.
	void Update(float dt);
	//! Call this to draw the clouds, including any illumination by lightning.
	HRESULT Render();
	void SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p);
	//! Set the strength of the rain or precipitation effect.
	void SetIntensity(float i)
	{
		rain_intensity=i;
	}
	//! Set the colour of light, e.g. sunlight, that illuminates the precipitation.
	void SetLightColour(const float c[4]);
protected:
	ID3D11Device*		m_pd3dDevice;
	ID3D11InputLayout* m_pVtxDecl;
	ID3DX11Effect*			m_pRainEffect;		// The fx file for the sky
	ID3D11Texture2D*		rain_texture;
	ID3DX11EffectVariable*				worldViewProj;
	ID3DX11EffectVariable*				offset;
	ID3DX11EffectVariable*				lightColour;
	ID3DX11EffectVariable*				intensity;
	ID3DX11EffectTechnique*	m_hTechniqueRain;	// Handle to technique in the effect 
	D3DXMATRIX				world,view,proj;
	D3DXVECTOR3				cam_pos;
	float radius,height,offs,rain_intensity;
	float light_colour[4];
};