// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulCloudRendererDX11.h A DirectX 10 renderer for clouds. Create an instance of this class in a DX11 program
//! and use the Render() function once per frame.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3dx9.h>
	#include <d3d11.h>
	#include <d3dx11.h>
	#include <D3dx11effect.h>
#endif
#include "Simul/Base/SmartPtr.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/CloudRenderCallback.h"
#include "Simul/Clouds/BaseCloudRenderer.h"

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
class SimulCloudRendererDX11 : public simul::clouds::BaseCloudRenderer
{
public:
	SimulCloudRendererDX11();
	virtual ~SimulCloudRendererDX11();
	//! Call this once to set the sky interface that this cloud renderer can use for distance fading.
	void SetSkyInterface(simul::sky::BaseSkyInterface *si);
	//! Call this when the D3D device has been created or reset.
	void SetFadeTableInterface(simul::sky::FadeTableInterface *fti);
	//! Call this when the device has been created
	HRESULT RestoreDeviceObjects( ID3D11Device* pd3dDevice);
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
	ID3D11Texture3D* *GetCloudTextures();
	const float *GetCloudScales() const;
	const float *GetCloudOffset() const;
	void SetLossTextures(ID3D11Resource *l1,ID3D11Resource *l2);
	void SetInscatterTextures(ID3D11Resource *i1,ID3D11Resource *i2);

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

	ID3D11Device*					m_pd3dDevice;
	ID3D11DeviceContext *			m_pImmediateContext;
	ID3D11Buffer *					vertexBuffer;
	ID3D11InputLayout*				m_pVtxDecl;
	ID3D11InputLayout*				m_pLightningVtxDecl;

	ID3DX11Effect*					m_pLightningEffect;
	ID3DX11EffectTechnique*			m_hTechniqueLightning;	// Handle to technique in the effect 
	ID3DX11Effect*					m_pCloudEffect;
	ID3DX11EffectTechnique*			m_hTechniqueCloud;		// Handle to technique in the effect
	ID3DX11EffectTechnique*			m_hTechniqueCloudsAndLightning;	

	ID3DX11EffectMatrixVariable* 	l_worldViewProj;
	ID3DX11EffectMatrixVariable* 	worldViewProj;
	ID3DX11EffectVectorVariable* 	eyePosition;
	ID3DX11EffectVectorVariable* 	lightResponse;
	ID3DX11EffectVectorVariable* 	lightDir;
	ID3DX11EffectVectorVariable* 	skylightColour;
	ID3DX11EffectVectorVariable* 	sunlightColour;
	ID3DX11EffectVectorVariable* 	fractalScale;
	ID3DX11EffectScalarVariable* 	interp;
	ID3DX11EffectVectorVariable* 	mieRayleighRatio;
	ID3DX11EffectScalarVariable* 	cloudEccentricity;
	ID3DX11EffectScalarVariable* 	hazeEccentricity;
	ID3DX11EffectScalarVariable* 	fadeInterp;
	ID3DX11EffectScalarVariable* 	altitudeTexCoord;

	ID3DX11EffectVectorVariable* 	lightningMultipliers;
	ID3DX11EffectVectorVariable* 	lightningColour;
	ID3DX11EffectVectorVariable* 	illuminationOrigin;
	ID3DX11EffectVectorVariable* 	illuminationScales;

	ID3DX11EffectShaderResourceVariable*		cloudDensity1;
	ID3DX11EffectShaderResourceVariable*		cloudDensity2;
	ID3DX11EffectShaderResourceVariable*		noiseTexture;

	ID3DX11EffectShaderResourceVariable*		lightningIlluminationTexture;
	ID3DX11EffectShaderResourceVariable*		skyLossTexture1;
	ID3DX11EffectShaderResourceVariable*		skyLossTexture2;
	ID3DX11EffectShaderResourceVariable*		skyInscatterTexture1;
	ID3DX11EffectShaderResourceVariable*		skyInscatterTexture2;

	ID3D11ShaderResourceView*				cloudDensityResource[3];
	ID3D11ShaderResourceView*				noiseTextureResource;
	ID3D11ShaderResourceView*				lightningIlluminationTextureResource;
	ID3D11ShaderResourceView*				skyLossTexture1Resource;
	ID3D11ShaderResourceView*				skyLossTexture2Resource;
	ID3D11ShaderResourceView*				skyInscatterTexture1Resource;
	ID3D11ShaderResourceView*				skyInscatterTexture2Resource;

	ID3D11Texture3D*	cloud_textures[3];
	ID3D11Texture3D*	illumination_texture;
	ID3D11Texture2D*	noise_texture;
	ID3D11Texture1D*	lightning_texture;
	ID3D11Texture2D*	large_scale_cloud_texture;
	ID3D11Texture2D*	sky_loss_texture_1;
	ID3D11Texture2D*	sky_loss_texture_2;
	ID3D11Texture2D*	sky_inscatter_texture_1;
	ID3D11Texture2D*	sky_inscatter_texture_2;
	ID3D11Texture2D*	cloud_cubemap;
	D3DXVECTOR4			cam_pos;
	D3DXVECTOR4			lightning_colour;
	D3DXMATRIX			world,view,proj;

	D3D11_MAPPED_SUBRESOURCE mapped_cloud_texture;
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