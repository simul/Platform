// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// Simul2DCloudRenderer.h A renderer for 2D cloud layers.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3dx9.h>
	#include <d3DX11.h>
#endif
#include "Simul/Base/SmartPtr.h"
namespace simul
{
	namespace clouds
	{
		class CloudInterface;
		class Cloud2DGeometryHelper;
		class FastCloudNode;
	}
	namespace sky
	{
		class SkyInterface;
		class FadeTableInterface;
	}
}
typedef long HRESULT;

//! A renderer for 2D cloud layers, e.g. cirrus clouds.
class Simul2DCloudRenderer
{
public:
	Simul2DCloudRenderer();
	virtual ~Simul2DCloudRenderer();
	//! Call this once to set the sky interface that this cloud renderer can use for distance fading.
	void SetSkyInterface(simul::sky::SkyInterface *si);
	void SetFadeTableInterface(simul::sky::FadeTableInterface *fti);
	//standard d3d object interface functions
	HRESULT Create( ID3D10Device* pd3dDevice);
	//! Call this when the D3D device has been created or reset.
	HRESULT RestoreDeviceObjects( ID3D10Device* pd3dDevice);
	//! Call this when the D3D device has been shut down.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	//! Call this once per frame to update the clouds.
	void Update(float dt);
	//! Call this to draw the clouds, including any illumination by lightning.
	HRESULT Render();
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p);
	//! Set the wind horizontal velocity components in metres per second.
	void SetWindVelocity(float x,float y);
	//! Get an interface to the Simul cloud object.
	simul::clouds::CloudInterface *GetCloudInterface();
protected:
	simul::clouds::CloudInterface *cloudInterface;
	simul::sky::SkyInterface *skyInterface;
	simul::sky::FadeTableInterface *fadeTableInterface;
	simul::clouds::Cloud2DGeometryHelper *helper;

	ID3D10Device*				m_pd3dDevice;
	ID3D10InputLayout*	m_pVtxDecl;

	ID3D10Effect*					m_pCloudEffect;
	ID3D10EffectTechnique*			m_hTechniqueCloud;	// Handle to technique in the effect 
	ID3D10EffectVariable* worldViewProj;
	ID3D10EffectVariable* eyePosition;
	ID3D10EffectVariable* lightResponse;
	ID3D10EffectVariable* lightDir;
	ID3D10EffectVariable* skylightColour;
	ID3D10EffectVariable* sunlightColour;
	ID3D10EffectVariable* fractalScale;
	ID3D10EffectVariable* interp;
	ID3D10EffectVariable* layerDensity;
	ID3D10EffectVariable* imageEffect;

	ID3D10EffectVariable*	cloudDensity1;
	ID3D10EffectVariable*	cloudDensity2;
	ID3D10EffectVariable*	noiseTexture;
	ID3D10EffectVariable*	imageTexture;

	ID3D10Texture2D*			cloud_textures[3];
	ID3D10Texture2D*			noise_texture;
	ID3D10Texture2D*			image_texture;
	D3DXVECTOR4					cam_pos;
	D3DXMATRIX					world,view,proj;

	HRESULT UpdateCloudTexture();
	HRESULT FillInCloudTextures();
	HRESULT CreateCloudTextures();
	HRESULT CreateNoiseTexture();
	HRESULT CreateImageTexture();
	HRESULT CreateCloudEffect();
	HRESULT RenderCloudsToBuffer();
	HRESULT MakeCubemap(); // not ready yet
	simul::base::SmartPtr<simul::clouds::FastCloudNode> cloudNode;
	float texture_scale;
	unsigned cloud_texel_index;
};