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
#include "Simul/Platform/CrossPlatform/CppSl.hs"
#include "Simul/Platform/CrossPlatform/rain_constants.sl"
#include "Simul/Platform/DirectX11/Utilities.h"
typedef long HRESULT;

class SimulPrecipitationRendererDX1x:public simul::clouds::BasePrecipitationRenderer
{
public:
	SimulPrecipitationRendererDX1x();
	virtual ~SimulPrecipitationRendererDX1x();
	//standard d3d object interface functions:
	//! Call this when the D3D device has been created or reset.
	void RestoreDeviceObjects(void* dev);
	void RecompileShaders();
	void SetCubemapTexture(void *);
	//! Call this when the D3D device has been shut down.
	void InvalidateDeviceObjects();
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	//! Call this to draw the clouds, including any illumination by lightning.
	void Render(void *context);
protected:
	void RenderParticles(void *context);
	ID3D11Device*							m_pd3dDevice;
	ID3D11InputLayout*						m_pVtxDecl;
	ID3D11Buffer*							m_pVertexBuffer;
	ID3DX11Effect*							m_pRainEffect;		// The fx file for the sky
	ID3D11ShaderResourceView*				rain_texture;
	ID3D11ShaderResourceView*				random_SRV;
	ID3D11ShaderResourceView*				cubemap_SRV;
	ID3D1xEffectShaderResourceVariable*		rainTexture;
	vec3  *particles;
	
	ID3DX11EffectTechnique*					m_hTechniqueRain;
	ID3DX11EffectTechnique*					m_hTechniqueParticles;
	D3DXMATRIX								view,proj;
	ConstantBuffer<RainConstants>			rainConstants;
	ConstantBuffer<RainPerViewConstants>	perViewConstants;
};
