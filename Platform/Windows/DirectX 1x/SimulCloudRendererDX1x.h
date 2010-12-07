// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulCloudRendererDX1x.h A DirectX 10 renderer for clouds. Create an instance of this class in a dx10 program
//! and use the Render() function once per frame.

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

#include "Simul/Base/SmartPtr.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/CloudRenderCallback.h"
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Platform/Windows/DirectX 1x/MacrosDx1x.h"

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
		class BaseSkyInterface;
		class FadeTableInterface;
		class OvercastCallback;
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

//! A cloud rendering class. Create an instance of this class within a DirectX program.
class SimulCloudRendererDX1x : public simul::clouds::BaseCloudRenderer
{
public:
	SimulCloudRendererDX1x();
	virtual ~SimulCloudRendererDX1x();
	//! Call this once to set the sky interface that this cloud renderer can use for distance fading.
	void SetSkyInterface(simul::sky::BaseSkyInterface *si);
	//! Call this when the D3D device has been created or reset.
	void SetFadeTableInterface(simul::sky::FadeTableInterface *fti);
	//! Call this when the device has been created
	HRESULT RestoreDeviceObjects( ID3D1xDevice* pd3dDevice);
	//! Call this when the 3D device has been lost.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	//! Call this once per frame to update the clouds.
	void Update(float dt);
	//! Call this to draw the clouds, including any illumination by lightning.
	HRESULT Render(bool cubemap=false);
	//! Call this to render the lightning bolts (cloud illumination is done in the main Render function).
	HRESULT RenderLightning();
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
	simul::clouds::LightningRenderInterface *GetLightningRenderInterface();
	//! Get a float between zero and one which represents the interpolation between cloud keyframes.
	float GetInterpolation() const;
	void SetStepsPerHour(unsigned s);
	//! Return true if the camera is above the cloudbase altitude.
	bool IsCameraAboveCloudBase() const;
	float GetSunOcclusion() const;
	const char *GetDebugText() const;
	void SetEnableStorms(bool s);
	float GetTiming() const;
	//! Get the list of three textures used for cloud rendering.
	ID3D1xTexture3D* *GetCloudTextures();
	const float *GetCloudScales() const;
	const float *GetCloudOffset() const;
	void SetLossTextures(ID3D1xResource *l1,ID3D1xResource *l2);
	void SetInscatterTextures(ID3D1xResource *i1,ID3D1xResource *i2);

	void SetNoiseTextureProperties(int s,int f,int o,float p);
	void SetAltitudeTextureCoordinate(float f)
	{
		altitude_tex_coord=f;
	}
	// implementing CloudRenderCallback:
	void SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z);
	void FillCloudTextureSequentially(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array);
	void FillCloudTextureBlock(int ,int,int,int,int,int,int,const unsigned *)
	{
	}
	void CycleTexturesForward();

	void SetIlluminationGridSize(unsigned width_x,unsigned length_y,unsigned depth_z){}
	void FillIlluminationSequentially(int source_index,int texel_index,int num_texels,const unsigned char *uchar8_array){}
	void FillIlluminationBlock(int source_index,int x,int y,int z,int w,int l,int d,const unsigned char *uchar8_array){}

	// Save and load a sky sequence
	std::ostream &Save(std::ostream &os) const;
	std::istream &Load(std::istream &is) const;
	//! Clear the sequence()
	void New();
protected:
	int mapped;
	void Unmap();
	void Map(int texture_index);
	bool y_vertical;
	simul::sound::fmod::NodeSound *sound;
	unsigned texel_index[4];
	bool lightning_active;
	float timing;

	ID3D1xDevice*					m_pd3dDevice;
	ID3D1xDeviceContext*			m_pImmediateContext;
	ID3D1xBuffer *					vertexBuffer;
	ID3D1xInputLayout*				m_pVtxDecl;
	ID3D1xInputLayout*				m_pLightningVtxDecl;

	ID3D1xEffect*					m_pLightningEffect;
	ID3D1xEffectTechnique*			m_hTechniqueLightning;	// Handle to technique in the effect 
	ID3D1xEffect*					m_pCloudEffect;
	ID3D1xEffectTechnique*			m_hTechniqueCloud;		// Handle to technique in the effect
	ID3D1xEffectTechnique*			m_hTechniqueCloudsAndLightning;	

	ID3D1xEffectMatrixVariable* 	l_worldViewProj;
	ID3D1xEffectMatrixVariable* 	worldViewProj;
	ID3D1xEffectVectorVariable* 	eyePosition;
	ID3D1xEffectVectorVariable* 	lightResponse;
	ID3D1xEffectVectorVariable* 	lightDir;
	ID3D1xEffectVectorVariable* 	skylightColour;
	ID3D1xEffectVectorVariable* 	sunlightColour;
	ID3D1xEffectVectorVariable* 	fractalScale;
	ID3D1xEffectScalarVariable* 	interp;
	ID3D1xEffectVectorVariable* 	mieRayleighRatio;
	ID3D1xEffectScalarVariable* 	cloudEccentricity;
	ID3D1xEffectScalarVariable* 	hazeEccentricity;
	ID3D1xEffectScalarVariable* 	fadeInterp;
	ID3D1xEffectScalarVariable* 	altitudeTexCoord;

	ID3D1xEffectVectorVariable* 	lightningMultipliers;
	ID3D1xEffectVectorVariable* 	lightningColour;
	ID3D1xEffectVectorVariable* 	illuminationOrigin;
	ID3D1xEffectVectorVariable* 	illuminationScales;

	ID3D1xEffectShaderResourceVariable*		cloudDensity1;
	ID3D1xEffectShaderResourceVariable*		cloudDensity2;
	ID3D1xEffectShaderResourceVariable*		noiseTexture;

	ID3D1xEffectShaderResourceVariable*		lightningIlluminationTexture;
	ID3D1xEffectShaderResourceVariable*		skyLossTexture1;
	ID3D1xEffectShaderResourceVariable*		skyLossTexture2;
	ID3D1xEffectShaderResourceVariable*		skyInscatterTexture1;
	ID3D1xEffectShaderResourceVariable*		skyInscatterTexture2;

	ID3D1xShaderResourceView*				cloudDensityResource[3];
	ID3D1xShaderResourceView*				noiseTextureResource;
	ID3D1xShaderResourceView*				lightningIlluminationTextureResource;
	ID3D1xShaderResourceView*				skyLossTexture1Resource;
	ID3D1xShaderResourceView*				skyLossTexture2Resource;
	ID3D1xShaderResourceView*				skyInscatterTexture1Resource;
	ID3D1xShaderResourceView*				skyInscatterTexture2Resource;

	ID3D1xTexture3D*	cloud_textures[3];
	ID3D1xTexture3D*	illumination_texture;
	ID3D1xTexture2D*	noise_texture;
	ID3D1xTexture1D*	lightning_texture;
	ID3D1xTexture2D*	large_scale_cloud_texture;
	ID3D1xTexture2D*	sky_loss_texture_1;
	ID3D1xTexture2D*	sky_loss_texture_2;
	ID3D1xTexture2D*	sky_inscatter_texture_1;
	ID3D1xTexture2D*	sky_inscatter_texture_2;
	ID3D1xTexture2D*	cloud_cubemap;
	D3DXVECTOR4			cam_pos;
	D3DXVECTOR4			lightning_colour;
	D3DXMATRIX			world,view,proj;

	D3D1x_MAPPED_TEXTURE3D mapped_cloud_texture;
	HRESULT UpdateIlluminationTexture(float dt);
	float LookupLargeScaleTexture(float x,float y);
	HRESULT CreateIlluminationTexture();
	HRESULT CreateLightningTexture();
	virtual bool CreateNoiseTexture(bool override_file=false);
	HRESULT CreateCloudEffect();
	HRESULT MakeCubemap(); // not ready yet
	
	float time_step;
	bool enable_lightning;

	float last_time;

	int noise_texture_size;
	int noise_texture_frequency;
	int texture_octaves;
	float texture_persistence;
};