// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulCloudRendererDX1x.h A DirectX 10/11 renderer for clouds. Create an instance of this class in a dx10 program
//! and use the Render() function once per frame.

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

#include "Simul/Base/SmartPtr.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/CloudRenderCallback.h"
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Platform/DirectX1x/MacrosDx1x.h"
#include "Simul/Platform/DirectX1x/Export.h"

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
		class OvercastCallback;
	}
}
typedef long HRESULT;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

//! A cloud rendering class. Create an instance of this class within a DirectX program.
SIMUL_DIRECTX1x_EXPORT_CLASS SimulCloudRendererDX1x : public simul::clouds::BaseCloudRenderer
{
public:
	SimulCloudRendererDX1x(simul::clouds::CloudKeyframer *cloudKeyframer);
	virtual ~SimulCloudRendererDX1x();
	void RecompileShaders();
	//! Call this when the D3D device has been created or reset
	void RestoreDeviceObjects( void* pd3dDevice);
	//! Call this when the 3D device has been lost.
	void InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	bool Destroy();
	//! Call this to draw the clouds, including any illumination by lightning.
	bool Render(bool cubemap,bool depth_testing,bool default_fog);
	void RenderCrossSections(int width,int height);
	//! Call this to render the lightning bolts (cloud illumination is done in the main Render function).
	bool RenderLightning();
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
	simul::clouds::LightningRenderInterface *GetLightningRenderInterface();

	//! Return true if the camera is above the cloudbase altitude.
	bool IsCameraAboveCloudBase() const;
	float GetSunOcclusion() const;
	const TCHAR *GetDebugText() const;
	void SetEnableStorms(bool s);
	float GetTiming() const;
	//! Get the list of three textures used for cloud rendering.
	void* *GetCloudTextures();
	void SetLossTexture(void *t);
	void SetInscatterTextures(void *t,void *s);

	void SetNoiseTextureProperties(int s,int f,int o,float p);
	void SetAltitudeTextureCoordinate(float f)
	{
		altitude_tex_coord=f;
	}
	// implementing CloudRenderCallback:
	void SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z){}
	void FillCloudTextureSequentially(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array){}
	void FillCloudTextureBlock(int,int,int,int,int,int,int,const unsigned *){}
	void CycleTexturesForward(){}

	void SetIlluminationGridSize(unsigned width_x,unsigned length_y,unsigned depth_z);
	void FillIlluminationSequentially(int source_index,int texel_index,int num_texels,const unsigned char *uchar8_array);
	void FillIlluminationBlock(int,int,int,int,int,int,int,const unsigned char *){}

	// Save and load a sky sequence
	std::ostream &Save(std::ostream &os) const;
	std::istream &Load(std::istream &is) const;
	//! Clear the sequence()
	void New();
	void SetYVertical(bool y);
	bool IsYVertical() const;
protected:
	// Make up to date with respect to keyframer:
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate();
	void EnsureCorrectIlluminationTextureSizes();
	void EnsureIlluminationTexturesAreUpToDate();
	void EnsureTextureCycle();

	bool y_vertical;
	int mapped;
	void Unmap();
	void Map(int texture_index);
	unsigned texel_index[4];
	bool lightning_active;
	float timing;

	ID3D1xDevice*					m_pd3dDevice;
	ID3D1xDeviceContext*			m_pImmediateContext;
	ID3D1xBuffer *					vertexBuffer;
	ID3D1xInputLayout*				m_pVtxDecl;
	ID3D1xInputLayout*				m_pLightningVtxDecl;

	ID3D1xEffect*					m_pLightningEffect;
	ID3D1xEffectTechnique*			m_hTechniqueLightning;
	ID3D1xEffect*					m_pCloudEffect;
	ID3D1xEffectTechnique*			m_hTechniqueCloud;
	ID3D1xEffectTechnique*			m_hTechniqueCloudsAndLightning;

	ID3D1xEffectTechnique*			m_hTechniqueCrossSectionXZ;
	ID3D1xEffectTechnique*			m_hTechniqueCrossSectionXY;

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
	ID3D1xEffectScalarVariable* 	alphaSharpness;

	ID3D1xEffectVectorVariable* 	lightningMultipliers;
	ID3D1xEffectVectorVariable* 	lightningColour;
	ID3D1xEffectVectorVariable* 	illuminationOrigin;
	ID3D1xEffectVectorVariable* 	illuminationScales;

	ID3D1xEffectShaderResourceVariable*		cloudDensity1;
	ID3D1xEffectShaderResourceVariable*		cloudDensity2;
	ID3D1xEffectShaderResourceVariable*		noiseTexture;

	ID3D1xEffectShaderResourceVariable*		lightningIlluminationTexture;
	ID3D1xEffectShaderResourceVariable*		skyLossTexture;
	ID3D1xEffectShaderResourceVariable*		skyInscatterTexture;
	ID3D1xEffectShaderResourceVariable*		skylightTexture;

	ID3D1xShaderResourceView*				cloudDensityResource[3];
	ID3D1xShaderResourceView*				noiseTextureResource;
	ID3D1xShaderResourceView*				lightningIlluminationTextureResource;
	ID3D1xShaderResourceView*				skyLossTexture_SRV;
	ID3D1xShaderResourceView*				skyInscatterTexture_SRV;
	ID3D1xShaderResourceView*				skylightTexture_SRV;

	ID3D1xTexture3D*			cloud_textures[3];
	ID3D1xTexture3D*			illumination_texture;
	
	D3D1x_MAPPED_TEXTURE3D mapped_illumination;

	ID3D1xTexture2D*	noise_texture;
	ID3D1xTexture1D*	lightning_texture;
	ID3D1xTexture2D*	cloud_cubemap;
	D3DXVECTOR4			cam_pos;
	D3DXVECTOR4			lightning_colour;
	D3DXMATRIX			world,view,proj;

	D3D1x_MAPPED_TEXTURE3D mapped_cloud_texture;
	bool UpdateIlluminationTexture(float dt);
	float LookupLargeScaleTexture(float x,float y);

	bool CreateLightningTexture();
	virtual bool CreateNoiseTexture(bool override_file=false);
	bool CreateCloudEffect();
	bool MakeCubemap(); // not ready yet
	
	bool enable_lightning;
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif