// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <tchar.h>
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
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
typedef long HRESULT;
namespace simul
{
	namespace sky
	{
		class AtmosphericScatteringInterface;
	}
}

// A class that takes an image buffer and a z-buffer and applies atmospheric fading.
class SimulAtmosphericsRendererDX1x : public simul::sky::BaseAtmosphericsRenderer
{
public:
	SimulAtmosphericsRendererDX1x();
	virtual ~SimulAtmosphericsRendererDX1x();
	
	void SetBufferSize(int w,int h);

	// BaseAtmosphericsRenderer.
	void SetLossTexture(void* t);
	void SetInscatterTextures(void* t,void *s);
	void SetCloudsTexture(void* t);
	void RecompileShaders();

	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	void RestoreDeviceObjects(void* pd3dDevice);
	//! Call this when the device has been lost.
	void InvalidateDeviceObjects();
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetCloudShadowTexture(void *c){}
	//! Render the Atmospherics.
	void RenderAsOverlay(void *context,const void *depthTexture,float exposure);
protected:
	HRESULT Destroy();
	ID3D1xDevice*								m_pd3dDevice;
	ID3D1xInputLayout*							vertexDecl;
	D3DXMATRIX									view,proj;

	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	ID3D1xEffect*								effect;
	ID3D1xEffectTechnique*						singlePassTechnique;
	ID3D1xEffectTechnique*						twoPassOverlayTechnique;
	// Variables for this effect:
	ID3D1xEffectMatrixVariable*					invViewProj;
	ID3D1xEffectVectorVariable*					lightDir;
	ID3D1xEffectVectorVariable*					MieRayleighRatio;
	ID3D1xEffectScalarVariable*					HazeEccentricity;
	ID3D1xEffectScalarVariable*					fadeInterp;
	ID3D1xEffectShaderResourceVariable*			depthTexture;
	ID3D1xEffectShaderResourceVariable*			lossTexture;
	ID3D1xEffectShaderResourceVariable*			inscatterTexture;
	ID3D1xEffectShaderResourceVariable*			skylightTexture;

	ID3D1xShaderResourceView*					skyLossTexture_SRV;
	ID3D1xShaderResourceView*					skyInscatterTexture_SRV;
	ID3D1xShaderResourceView*					skylightTexture_SRV;
	ID3D1xShaderResourceView*					clouds_texture;

	ID3D11Buffer*								constantBuffer;
	ID3D11Buffer*								atmosphericsUniforms2ConstantsBuffer;
};