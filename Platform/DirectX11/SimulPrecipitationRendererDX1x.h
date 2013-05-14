// Copyright (c) 2007-2013 Simul Software Ltd
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
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Math/float3.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Platform/DirectX11/HLSL/CppHLSL.hlsl"
typedef long HRESULT;

struct RainConstantBuffer
{
	mat4 worldViewProj;
	float offset;
	float intensity;
	vec4 lightColour;
	vec3 lightDir;
};
class SimulPrecipitationRendererDX1x:public simul::clouds::BasePrecipitationRenderer
{
public:
	SimulPrecipitationRendererDX1x();
	virtual ~SimulPrecipitationRendererDX1x();
	//standard d3d object interface functions:
	//! Call this when the D3D device has been created or reset.
	void RestoreDeviceObjects(void* dev);
	void RecompileShaders();
	//! Call this when the D3D device has been shut down.
	void InvalidateDeviceObjects();
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	//! Call this to draw the clouds, including any illumination by lightning.
	void Render(void *context);
protected:
	ID3D1xDevice*					m_pd3dDevice;
	ID3D1xInputLayout*				m_pVtxDecl;
	ID3D1xBuffer					*m_pVertexBuffer;
	ID3D1xEffect*					m_pRainEffect;		// The fx file for the sky
	ID3D1xShaderResourceView*		rain_texture;
	ID3D1xEffectShaderResourceVariable*	rainTexture;

	ID3D1xEffectMatrixVariable* 	worldViewProj;
	ID3D1xEffectScalarVariable*		offset;
	ID3D1xEffectVectorVariable*		lightColour;
	ID3D1xEffectVectorVariable*		lightDir;
	ID3D1xEffectScalarVariable*		intensity;
	
	ID3D11Buffer*					pShadingCB;
	
	ID3D1xEffectTechnique*			m_hTechniqueRain;	// Handle to technique in the effect 
	D3DXMATRIX						view,proj;
	
	RainConstantBuffer				rainConstantBuffer;
};
