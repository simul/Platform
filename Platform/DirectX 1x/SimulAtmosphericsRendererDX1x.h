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
	#include <D3DX9.h>
	#include <d3d10.h>
	#include <d3dx10.h>
	#include <D3D10effect.h>
#endif
typedef long HRESULT;
namespace simul
{
	namespace sky
	{
		class SkyInterface;
	}
}

class SimulAtmosphericsInterface
{
public:
	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	virtual HRESULT RestoreDeviceObjects(ID3D10Device* pd3dDevice)=0;
	//! Call this when the device has been lost.
	virtual HRESULT InvalidateDeviceObjects()=0;
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	virtual HRESULT Render()=0;
	virtual void SetInputTextures(ID3D10Texture2D* image,ID3D10Texture2D* depth)=0;
};

// A class that takes an image buffer and a z-buffer and applies atmospheric fading.
class SimulAtmosphericsRenderer : public SimulAtmosphericsInterface
{
public:
	SimulAtmosphericsRenderer();
	virtual ~SimulAtmosphericsRenderer();
	//standard d3d object interface functions

	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	HRESULT RestoreDeviceObjects(ID3D10Device* pd3dDevice);
	//! Call this when the device has been lost.
	HRESULT InvalidateDeviceObjects();
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	HRESULT Render();
	void SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetLossTextures(ID3D10Texture3D* t1,ID3D10Texture3D* t2)
	{
		loss_texture_1=t1;
		loss_texture_2=t2;
	}
	void SetInscatterTextures(ID3D10Texture3D* t1,ID3D10Texture3D* t2)
	{
		inscatter_texture_1=t1;
		inscatter_texture_2=t2;
	}
	void SetSkyInterface(simul::sky::SkyInterface *si)
	{
		skyInterface=si;
	}
	void SetInputTextures(ID3D10Texture2D* image,ID3D10Texture2D* depth)
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
protected:
	simul::sky::SkyInterface *					skyInterface;
	HRESULT Destroy();
	ID3D10Device*								m_pd3dDevice;
	ID3D10InputLayout*							vertexDecl;
	D3DXMATRIX									world,view,proj;

	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	ID3D10Effect*								effect;
	ID3D10EffectTechnique*						technique;
	// Variables for this effect:
	ID3D10EffectVariable*						invViewProj;
	ID3D10EffectVariable*						lightDir;
	ID3D10EffectVariable*						MieRayleighRatio;
	ID3D10EffectVariable*						HazeEccentricity;
	ID3D10EffectVariable*						fadeInterp;
	ID3D10EffectVariable*						overcastFactor;
	ID3D10EffectVariable*						imageTexture;
	ID3D10EffectVariable*						depthTexture;
	ID3D10EffectVariable*						lossTexture1;
	ID3D10EffectVariable*						lossTexture2;
	ID3D10EffectVariable*						inscatterTexture1;
	ID3D10EffectVariable*						inscatterTexture2;

	ID3D10Texture3D*		loss_texture_1;
	ID3D10Texture3D*		loss_texture_2;
	ID3D10Texture3D*		inscatter_texture_1;
	ID3D10Texture3D*		inscatter_texture_2;

	//! The depth buffer.
	ID3D10Texture2D*				depth_texture;
	//! The un-faded image buffer.
	ID3D10Texture2D*				input_texture;

	float fade_interp,overcast_factor;

};