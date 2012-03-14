// Copyright (c) 2007-2011 Simul Software Ltd
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
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Platform/DirectX9/Export.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif


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
	}
}
typedef long HRESULT;

//! A renderer for 2D cloud layers, e.g. cirrus clouds.
SIMUL_DIRECTX9_EXPORT_CLASS Simul2DCloudRenderer: public simul::clouds::BaseCloudRenderer
{
public:
	Simul2DCloudRenderer(simul::clouds::CloudKeyframer *ck);
	virtual ~Simul2DCloudRenderer();
	//standard d3d object interface functions
	bool Create( LPDIRECT3DDEVICE9 pd3dDevice);
	void RecompileShaders();
	//! Call this when the D3D device has been created or reset.
	bool RestoreDeviceObjects(void *pd3dDevice);
	//! Call this when the D3D device has been shut down.
	bool InvalidateDeviceObjects();
	//! Return debugging information.
	const char *GetDebugText() const;
	//! Call this to draw the clouds, including any illumination by lightning.
	//! On DX9, depth_testing and default_fog are ignored for now.
	bool Render(bool cubemap,bool depth_testing,bool default_fog);
	bool RenderCrossSections(int width);
#if defined(XBOX) || defined(DOXYGEN)
	//! Call this once per frame to set the matrices (X360 only).
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
#endif
	//! Set the wind horizontal velocity components in metres per second.
	void SetWind(float speed,float heading_degrees);
	//! Get an interface to the Simul cloud object.
	simul::clouds::CloudKeyframer *GetCloudKeyframer();
	void Enable(bool val);
	// implementing CloudRenderCallback:
	void SetCloudTextureSize(unsigned ,unsigned ,unsigned ){}
	void FillCloudTextureSequentially(int ,int ,int ,const unsigned *){}
	void FillCloudTextureBlock(int,int,int,int,int,int,int,const unsigned *){}
	void CycleTexturesForward(){}

	// a texture
	void SetExternalTexture(LPDIRECT3DTEXTURE9	tex);
	
	virtual void **GetCloudTextures(){return NULL;}
	void SetYVertical(bool y)
	{
		y_vertical=y;
	}
	bool IsYVertical() const{return y_vertical;}
protected:
	// Make up to date with respect to keyframer:
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate();
	void EnsureCorrectIlluminationTextureSizes(){}
	void EnsureIlluminationTexturesAreUpToDate(){}
	void EnsureTextureCycle();

	bool y_vertical;
	bool enabled;
	simul::base::SmartPtr<simul::clouds::Cloud2DGeometryHelper> helper;

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

	virtual bool CreateNoiseTexture(bool override_file=false);
	bool CreateImageTexture();
	bool MakeCubemap(); // not ready yet
	float texture_scale;
	virtual bool IsYVertical()
	{
		return y_vertical;
	}
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
