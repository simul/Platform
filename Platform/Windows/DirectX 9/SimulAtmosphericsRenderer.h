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
	virtual HRESULT RestoreDeviceObjects(LPDIRECT3DDEVICE9 pd3dDevice)=0;
	//! Call this when the device has been lost.
	virtual HRESULT InvalidateDeviceObjects()=0;
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	virtual HRESULT Render()=0;
	virtual void SetInputTextures(LPDIRECT3DTEXTURE9 image,LPDIRECT3DTEXTURE9 depth)=0;
};

// A class that takes an image buffer and a z-buffer and applies atmospheric fading.
class SimulAtmosphericsRenderer : public SimulAtmosphericsInterface
{
public:
	SimulAtmosphericsRenderer();
	virtual ~SimulAtmosphericsRenderer();
	//standard d3d object interface functions

	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	HRESULT RestoreDeviceObjects(LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the device has been lost.
	HRESULT InvalidateDeviceObjects();
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	HRESULT Render();
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetLossTextures(LPDIRECT3DTEXTURE9 t1,LPDIRECT3DTEXTURE9 t2)
	{
		loss_texture_1=t1;
		loss_texture_2=t2;
	}
	void SetInscatterTextures(LPDIRECT3DTEXTURE9 t1,LPDIRECT3DTEXTURE9 t2)
	{
		inscatter_texture_1=t1;
		inscatter_texture_2=t2;
	}
	void SetSkyInterface(simul::sky::SkyInterface *si)
	{
		skyInterface=si;
	}
	void SetInputTextures(LPDIRECT3DTEXTURE9 image,LPDIRECT3DTEXTURE9 depth)
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
	simul::sky::SkyInterface *skyInterface;
	HRESULT Destroy();
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	vertexDecl;
	D3DXMATRIX				world,view,proj;

	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	LPD3DXEFFECT					effect;
	D3DXHANDLE						technique;
	// Variables for this effect:
	D3DXHANDLE						invViewProj;
	D3DXHANDLE						lightDir;
	D3DXHANDLE						MieRayleighRatio;
	D3DXHANDLE						HazeEccentricity;
	D3DXHANDLE						fadeInterp;
	D3DXHANDLE						overcastFactor;
	D3DXHANDLE						imageTexture;
	D3DXHANDLE						depthTexture;
	D3DXHANDLE						lossTexture1;
	D3DXHANDLE						lossTexture2;
	D3DXHANDLE						inscatterTexture1;
	D3DXHANDLE						inscatterTexture2;

	LPDIRECT3DTEXTURE9		loss_texture_1;
	LPDIRECT3DTEXTURE9		loss_texture_2;
	LPDIRECT3DTEXTURE9		inscatter_texture_1;
	LPDIRECT3DTEXTURE9		inscatter_texture_2;

	//! The depth buffer.
	LPDIRECT3DTEXTURE9				depth_texture;
	//! The un-faded image buffer.
	LPDIRECT3DTEXTURE9				input_texture;

	float fade_interp,overcast_factor;

};