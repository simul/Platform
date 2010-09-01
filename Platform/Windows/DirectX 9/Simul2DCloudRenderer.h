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
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Base/SmartPtr.h"
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Clouds/CloudKeyframer.h"
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
class Simul2DCloudRenderer: public simul::clouds::BaseCloudRenderer
{
public:
	Simul2DCloudRenderer();
	virtual ~Simul2DCloudRenderer();
	//! Call this once to set the sky interface that this cloud renderer can use for distance fading.
	void SetSkyInterface(simul::sky::SkyInterface *si);
	void SetFadeTableInterface(simul::sky::FadeTableInterface *fti);
	//standard d3d object interface functions
	HRESULT Create( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the D3D device has been created or reset.
	HRESULT RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the D3D device has been shut down.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	//! Call this once per frame to update the clouds.
	void Update(float dt);
	//! Return debugging information.
	const char *GetDebugText() const;
	//! Call this to draw the clouds, including any illumination by lightning.
	HRESULT Render();
	HRESULT RenderTexture();
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	//! Set the wind horizontal velocity components in metres per second.
	void SetWind(float speed,float heading_degrees);
	//! Get an interface to the Simul cloud object.
	simul::clouds::CloudInterface *GetCloudInterface();
	simul::clouds::CloudKeyframer *GetCloudKeyframer();
	void Enable(bool val);
	void SetStepsPerHour(unsigned s);
	// implementing CloudRenderCallback:
	void SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z);
	void FillCloudTextureSequentially(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array);
	void FillCloudTextureBlock(int texture_index,int x,int y,int z,int w,int l,int d,const unsigned *uint32_array)
	{
	}
	void CycleTexturesForward();
	void SetCloudiness(float c);
	// a texture
	void SetExternalTexture(LPDIRECT3DTEXTURE9	tex);
protected:
	bool enabled;
	simul::clouds::CloudInterface *cloudInterface;
	simul::sky::SkyInterface *skyInterface;
	simul::sky::FadeTableInterface *fadeTableInterface;
	simul::base::SmartPtr<simul::clouds::Cloud2DGeometryHelper> helper;
	simul::base::SmartPtr<simul::clouds::CloudKeyframer> cloudKeyframer;

	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pVtxDecl;

	LPD3DXEFFECT					m_pCloudEffect;
	D3DXHANDLE						m_hTechniqueCloud;	// Handle to technique in the effect 
	D3DXHANDLE worldViewProj;
	D3DXHANDLE eyePosition;
	D3DXHANDLE lightResponse;
	D3DXHANDLE lightDir;
	D3DXHANDLE skylightColour;
	D3DXHANDLE sunlightColour;
	D3DXHANDLE fractalScale;
	D3DXHANDLE interp;
	D3DXHANDLE layerDensity;
	D3DXHANDLE imageEffect;
	D3DXHANDLE mieRayleighRatio;
	D3DXHANDLE hazeEccentricity;
	D3DXHANDLE cloudEccentricity;

	D3DXHANDLE	cloudDensity1;
	D3DXHANDLE	cloudDensity2;
	D3DXHANDLE	noiseTexture;
	D3DXHANDLE	imageTexture;

	LPDIRECT3DTEXTURE9			cloud_textures[3];
	LPDIRECT3DTEXTURE9			noise_texture;
	LPDIRECT3DTEXTURE9			image_texture;
	bool own_image_texture;
	D3DXVECTOR4					cam_pos;
	D3DXMATRIX					world,view,proj;

	HRESULT CreateNoiseTexture();
	HRESULT CreateImageTexture();
	HRESULT MakeCubemap(); // not ready yet
	simul::base::SmartPtr<simul::clouds::FastCloudNode> cloudNode;
	float texture_scale;
	unsigned cloud_texel_index;
	bool texture_complete;
};