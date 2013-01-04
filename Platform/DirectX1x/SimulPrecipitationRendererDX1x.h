// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <d3dx9.h>
#ifdef DX10
#include <d3d10.h>
#include <d3dx10.h>
#include <d3d10effect.h>
#else
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#endif
typedef long HRESULT;
#include <vector>
#include "Simul/Platform/DirectX1x/MacrosDX1x.h"
#include "Simul/Math/float3.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/DirectX1x/Export.h"
typedef long HRESULT;
class SimulPrecipitationRendererDX1x
{
public:
	SimulPrecipitationRendererDX1x();
	virtual ~SimulPrecipitationRendererDX1x();
	//standard d3d object interface functions:
	//! Call this when the D3D device has been created or reset.
	HRESULT RestoreDeviceObjects( void* dev);
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
	ID3D1xDevice*					m_pd3dDevice;
	ID3D1xDeviceContext *			m_pImmediateContext;
	ID3D1xInputLayout*				m_pVtxDecl;
	ID3D1xBuffer					*m_pVertexBuffer;
	ID3D1xEffect*					m_pRainEffect;		// The fx file for the sky
	ID3D1xShaderResourceView*		rain_texture;
	ID3D1xEffectShaderResourceVariable*	rainTexture;
	ID3D1xEffectMatrixVariable*		worldViewProj;
	ID3D1xEffectScalarVariable*		offset;
	ID3D1xEffectVectorVariable*		lightColour;
	ID3D1xEffectScalarVariable*		intensity;
	ID3D1xEffectTechnique*			m_hTechniqueRain;	// Handle to technique in the effect 
	D3DXMATRIX				world,view,proj;
	D3DXVECTOR3				cam_pos;
	float radius,height,offs,rain_intensity;
	float light_colour[4];
};