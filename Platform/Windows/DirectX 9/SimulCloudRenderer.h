// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulCloudRenderer.h A DirectX renderer for clouds. Create an instance of this class in a DX program
//! and use the Render() function once per frame.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Base/SmartPtr.h"
#define THREADED_VERSION
//#define STORM_VERSION
namespace simul
{
	namespace clouds
	{
		class CloudInterface;
		class LightningRenderInterface;
		class CloudGeometryHelper;
		class ThunderCloudNode;
	}
	namespace sky
	{
		class SkyInterface;
		class FadeTableInterface;
	}
	namespace sound
	{
		namespace fmod
		{
			class NodeSound;
		}
	}
}
typedef long HRESULT;

//! A cloud rendering class. Create an instance of this class within a DirectX program,
//! or use SimulWeatherRenderer to manage cloud and sky rendering together.
class SimulCloudRenderer
{
public:
	SimulCloudRenderer();
	virtual ~SimulCloudRenderer();
	//! Call this once to set the sky interface that this cloud renderer can use for distance fading.
	void SetSkyInterface(simul::sky::SkyInterface *si);
	//! Call this when the D3D device has been created or reset.
	void SetFadeTableInterface(simul::sky::FadeTableInterface *fti);
	//! Call this when the device has been created
	HRESULT RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the 3D device has been lost.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	//! Call this once per frame to update the clouds.
	void Update(float dt);
	HRESULT RenderNewMethod();//experimental, do not use
	//! Call this to draw the clouds, including any illumination by lightning.
	HRESULT Render();
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
	//! Call this to render the lightning bolts (cloud illumination is done in the main Render function).
	HRESULT RenderLightning();
	//! Set the wind horizontal velocity components in metres per second.
	void SetWindVelocity(float x,float y);
	//! Get an interface to the Simul cloud object.
	simul::clouds::CloudInterface *GetCloudInterface();
	simul::clouds::LightningRenderInterface *GetLightningRenderInterface();
	//! Get a float between zero and one which represents the interpolation between cloud keyframes.
	float GetInterpolation() const {return cloud_interp;}
	//! Between zero and one: how overcast is the sky?
	float GetOvercastFactor() const;
	//! How much, if any precipitation should be shown, between zero and one.
	float GetPrecipitationIntensity() const;
	//! Return true if the camera is above the cloudbase altitude.
	bool IsCameraAboveCloudBase() const;
	float GetSunOcclusion() const;
	const char *GetDebugText() const;
	//! Set the overall required cloudiness, or humidity.
	void SetCloudiness(float h);
	void SetEnableStorms(bool s);
	float GetTiming() const;
	//! Get the list of three textures used for cloud rendering.
	LPDIRECT3DVOLUMETEXTURE9 *GetCloudTextures();
	const float *GetCloudScales() const;
	const float *GetCloudOffset() const;
	void SetDetail(float d);
	void SetLossTextures(LPDIRECT3DTEXTURE9 t1,LPDIRECT3DTEXTURE9 t2){sky_loss_texture_1=t1;sky_loss_texture_2=t2;}
	void SetInscatterTextures(LPDIRECT3DTEXTURE9 t1,LPDIRECT3DTEXTURE9 t2){sky_inscatter_texture_1=t1;sky_inscatter_texture_2=t2;}
	void SetFadeInterpolation(float f)
	{
		fade_interp=f;
	}
	void SetNoiseTextureProperties(int s,int f,int o,float p)
	{
		noise_texture_size=s;
		noise_texture_frequency=f;
		texture_octaves=o;
		texture_persistence=p;
		CreateNoiseTexture();
	}
	HRESULT RenderDistances();
protected:
	void CalculateOvercastFactor();
	bool y_vertical;
	float sun_occlusion;
	float global_cloudiness;
	float detail;
	simul::clouds::CloudInterface *cloudInterface;
	simul::sky::SkyInterface *skyInterface;
	simul::sky::FadeTableInterface *fadeTableInterface;
	simul::clouds::CloudGeometryHelper *helper;
	simul::sound::fmod::NodeSound *sound;
	unsigned texel_index[4];
	bool lightning_active;
	float timing;

	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pVtxDecl;
	LPDIRECT3DVERTEXDECLARATION9	m_pLightningVtxDecl;

	LPD3DXEFFECT					m_pLightningEffect;
	D3DXHANDLE						m_hTechniqueLightning;	// Handle to technique in the effect 
	LPD3DXEFFECT					m_pCloudEffect;
	D3DXHANDLE						m_hTechniqueCloud;		// Handle to technique in the effect
	D3DXHANDLE						m_hTechniqueCloudsAndLightning;	
	
	D3DXHANDLE l_worldViewProj;
	D3DXHANDLE worldViewProj;
	D3DXHANDLE eyePosition;
	D3DXHANDLE lightResponse;
	D3DXHANDLE lightDir;
	D3DXHANDLE skylightColour;
	D3DXHANDLE sunlightColour;
	D3DXHANDLE fractalScale;
	D3DXHANDLE interp;
	D3DXHANDLE large_texcoords_scale;
	D3DXHANDLE mieRayleighRatio;
	D3DXHANDLE hazeEccentricity;
	D3DXHANDLE cloudEccentricity;
	D3DXHANDLE fadeInterp;
	D3DXHANDLE overcast;
	D3DXHANDLE distance;
	D3DXHANDLE cornerPos;
	D3DXHANDLE texScales;
	D3DXHANDLE layerFade;

	D3DXHANDLE lightningMultipliers;
	D3DXHANDLE lightningColour;
	D3DXHANDLE illuminationOrigin;
	D3DXHANDLE illuminationScales;

	D3DXHANDLE	cloudDensity1;
	D3DXHANDLE	cloudDensity2;
	D3DXHANDLE	noiseTexture;
	D3DXHANDLE	largeScaleCloudTexture;
	D3DXHANDLE	lightningIlluminationTexture;
	D3DXHANDLE	skyLossTexture1;
	D3DXHANDLE	skyLossTexture2;
	D3DXHANDLE	skyInscatterTexture1;
	D3DXHANDLE	skyInscatterTexture2;

	LPDIRECT3DVOLUMETEXTURE9	cloud_textures[3];
	LPDIRECT3DVOLUMETEXTURE9	illumination_texture;
	LPDIRECT3DTEXTURE9			noise_texture;
	LPDIRECT3DTEXTURE9			lightning_texture;
	LPDIRECT3DTEXTURE9			large_scale_cloud_texture;
	LPDIRECT3DTEXTURE9			sky_loss_texture_1;
	LPDIRECT3DTEXTURE9			sky_loss_texture_2;
	LPDIRECT3DTEXTURE9			sky_inscatter_texture_1;
	LPDIRECT3DTEXTURE9			sky_inscatter_texture_2;
	LPDIRECT3DCUBETEXTURE9		cloud_cubemap;
	D3DXVECTOR4					cam_pos;
	D3DXVECTOR4					lightning_colour;
	D3DXMATRIX					world,view,proj;
	LPDIRECT3DVERTEXBUFFER9		unitSphereVertexBuffer;
	LPDIRECT3DINDEXBUFFER9		unitSphereIndexBuffer;
	HRESULT UpdateIlluminationTexture(float dt);
	HRESULT UpdateCloudTexture();
	HRESULT FillInCloudTextures();
	HRESULT CreateIlluminationTexture();
	HRESULT CreateCloudTextures();
	HRESULT CreateLightningTexture();
	HRESULT CreateNoiseTexture();
	HRESULT CreateCloudEffect();
	HRESULT MakeCubemap(); // not ready yet
	simul::base::SmartPtr<simul::clouds::ThunderCloudNode> cloudNode;
	bool edge_fill;
	unsigned edge_x,edge_y;
	unsigned cloud_texel_index;
	float time_step;
	float cloud_interp;
	bool texture_complete;
	float interp_step_time;
	float interp_time_1;
	bool enable_lightning;
	float overcast_factor;
	float overcast_factor_1;
	float overcast_factor_2;
	
	float precipitation;
	float precipitation_1;
	float precipitation_2;

	float last_hour;
	float fade_interp;

	int noise_texture_size;
	int noise_texture_frequency;
	int texture_octaves;
	float texture_persistence;
};