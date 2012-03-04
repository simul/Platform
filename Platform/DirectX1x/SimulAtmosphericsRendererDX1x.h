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
#include "Simul/Platform/DirectX1x/MacrosDX1x.h"
#include "Simul/Platform/DirectX1x/FramebufferDX1x.h"
#include "Simul/Platform/DirectX1x/Export.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
typedef long HRESULT;
namespace simul
{
	namespace sky
	{
		class SkyInterface;
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
	void SetYVertical(bool y);
	void SetDistanceTexture(void* t);
	void SetLossTexture(void* t);
	void SetInscatterTexture(void* t);
	void RecompileShaders();

	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	HRESULT RestoreDeviceObjects(ID3D1xDevice* pd3dDevice);
	//! Call this when the device has been lost.
	HRESULT InvalidateDeviceObjects();
	void SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetInputTextures(ID3D1xTexture2D* image,ID3D1xTexture2D* depth)
	{
		input_texture=image;
		depth_texture=depth;
	}
	void SetOvercastFactor(float of)
	{
		overcast_factor=of;
	}
	void SetFadeInterpolation(float s)
	{
		fade_interp=s;
	}
	//! Render the Atmospherics.
	void StartRender();
	void FinishRender();
protected:
	FramebufferDX1x								*framebuffer;
	simul::sky::SkyInterface *					skyInterface;
	HRESULT Destroy();
	ID3D1xDevice*								m_pd3dDevice;
	ID3D1xDeviceContext*						m_pImmediateContext;
	ID3D1xInputLayout*							vertexDecl;
	D3DXMATRIX									world,view,proj;

	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	ID3D1xEffect*								effect;
	ID3D1xEffectTechnique*						technique;
	// Variables for this effect:
	ID3D1xEffectMatrixVariable*					invViewProj;
	ID3D1xEffectVectorVariable*					lightDir;
	ID3D1xEffectVectorVariable*					MieRayleighRatio;
	ID3D1xEffectScalarVariable*					HazeEccentricity;
	ID3D1xEffectScalarVariable*					fadeInterp;
	ID3D1xEffectShaderResourceVariable*			imageTexture;
	ID3D1xEffectShaderResourceVariable*			depthTexture;
	ID3D1xEffectShaderResourceVariable*			lossTexture1;
	ID3D1xEffectShaderResourceVariable*			inscatterTexture1;

	ID3D1xTexture2D*					loss_texture_1;
	ID3D1xTexture2D*					inscatter_texture_1;

	//! The depth buffer.
	ID3D1xTexture2D*				depth_texture;
	//! The un-faded image buffer.
	ID3D1xTexture2D*				input_texture;

	float fade_interp,overcast_factor;

};