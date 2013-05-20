// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulSkyRendererDX1x.h A renderer for skies.

#pragma once
#include "Simul/Base/SmartPtr.h"
#include "Simul/Sky/SkyTexturesCallback.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include <d3dx9.h>
#ifdef DX10
	#include <d3d10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
	#include <d3dx11effect.h>
#endif
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"

namespace simul
{
	namespace sky
	{
		class AtmosphericScatteringInterface;
		class Sky;
		class SkyKeyframer;
	}
}

typedef long HRESULT;

//! A renderer for skies, this class will manage an instance of simul::sky::SkyNode and use it to calculate sky colours
//! in real time for the simul_sky.fx shader.
SIMUL_DIRECTX11_EXPORT_CLASS SimulSkyRendererDX1x:public simul::sky::BaseSkyRenderer
{
public:
	SimulSkyRendererDX1x(simul::sky::SkyKeyframer *sk);
	virtual ~SimulSkyRendererDX1x();

	//standard d3d object interface functions
	void RecompileShaders();
	//! Call this when the D3D device has been created or reset.
	void RestoreDeviceObjects(void* pd3dDevice);
	//! Call this when the D3D device has been shut down.
	void InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	bool Destroy();
	//! Call this to draw the sky, usually to the SimulWeatherRenderer's render target.
	bool Render(void *context,bool blend);
	bool RenderPointStars(void *context);
	void RenderSun(void *context,float exposure_hint);
	//! Draw the fade textures to screen
	bool RenderFades(void *context,int w,int h);
	virtual bool RenderPlanet(void *c,void* tex,float rad,const float *dir,const float *colr,bool do_lighting);
	//! Call this to draw the sun flare, usually drawn last, on the main render target.
	bool RenderFlare(float exposure);
	bool Render2DFades(void *context);
	//! Get a value, from zero to one, which represents how much of the sun is visible.
	//! Call this when the current rendering surface is the one that has obscuring
	//! objects like mountains etc. in it, and make sure these have already been drawn.
	//! GetSunOcclusion executes a pseudo-render of an invisible billboard, then
	//! uses a hardware occlusion query to see how many pixels have passed the z-test.
	float CalcSunOcclusion(float cloud_occlusion=0.f);
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);

	void Get2DLossAndInscatterTextures(void* *l1,void* *i1,void* *s);

	float GetFadeInterp() const;
	void SetStepsPerDay(unsigned steps);
//! Initialize textures
	void SetFadeTextureSize(unsigned width,unsigned height,unsigned num_altitudes);
	void FillFadeTex(ID3D11DeviceContext *context,int texture_index,int texel_index,int num_texels,
						const simul::sky::float4 *loss_float4_array,
						const simul::sky::float4 *inscatter_float4_array,
						const simul::sky::float4 *skylight_float4_array);
	void CycleTexturesForward();
	void SetYVertical(bool y);

	// for testing:
	void DrawCubemap(void *context,ID3D1xShaderResourceView*		m_pCubeEnvMapSRV);
protected:

	bool y_vertical;
	int cycle;
	bool IsYVertical(){return y_vertical;}

	void CreateFadeTextures();
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate(void *c);
	void EnsureTextureCycle();
	
	void BuildStarsBuffer();
	
	ID3D1xDevice*						m_pd3dDevice;
	ID3D1xBuffer*						m_pVertexBuffer;
	ID3D1xInputLayout*					m_pVtxDecl;
	ID3D1xInputLayout*					m_pStarsVtxDecl;
	ID3D1xBuffer*						m_pStarsVertexBuffer;
	ID3D1xEffect*						m_pSkyEffect;
	ID3D1xQuery*						d3dQuery;

	ID3D1xEffectMatrixVariable*			worldViewProj;
	ID3D1xEffectTechnique*				m_hTechniqueSky;
	ID3D1xEffectTechnique*				m_hTechniqueEarthShadow;
	ID3D1xEffectTechnique*				m_hTechniqueSky_CUBEMAP;
	ID3D1xEffectTechnique*				m_hTechniqueFade3DTo2D;
	ID3D1xEffectTechnique*				m_hTechniqueSun;
	ID3D1xEffectTechnique*				m_hTechniqueQuery;
	ID3D1xEffectTechnique*				m_hTechniqueFlare;
	ID3D1xEffectTechnique*				m_hTechniquePlanet;
	ID3D1xEffectTechnique*				m_hTechniquePointStars;
	ID3D1xEffectVectorVariable*			lightDirection;
	ID3D1xEffectVectorVariable*			mieRayleighRatio;
	ID3D1xEffectScalarVariable*			hazeEccentricity;
	ID3D1xEffectScalarVariable*			overcastFactor;
	ID3D1xEffectScalarVariable*			skyInterp;
	ID3D1xEffectScalarVariable*			altitudeTexCoord;
	ID3D1xEffectVectorVariable*			colour;

	ID3D1xEffectMatrixVariable*			projMatrix;
	ID3D1xEffectMatrixVariable*			cubemapViews;

	ID3D1xEffectShaderResourceVariable*	flareTexture;
	ID3D1xEffectShaderResourceVariable*	inscTexture;
	ID3D1xEffectShaderResourceVariable*	skylTexture;
	ID3D1xEffectShaderResourceVariable*	fadeTexture1;
	ID3D1xEffectShaderResourceVariable*	fadeTexture2;
	
	ID3DX11EffectConstantBuffer*		earthShadowUniforms;

	ID3D1xTexture3D*					loss_textures[3];
	ID3D1xTexture3D*					inscatter_textures[3];
	ID3D1xTexture3D*					skylight_textures[3];

	// Small framebuffers we render to once per frame to perform fade interpolation.
	FramebufferDX1x*					loss_2d;
	FramebufferDX1x*					inscatter_2d;
	FramebufferDX1x*					skylight_2d;

	ID3D1xShaderResourceView*			flare_texture_SRV;
	ID3D1xShaderResourceView*			loss_textures_SRV[3];
	ID3D1xShaderResourceView*			insc_textures_SRV[3];
	ID3D1xShaderResourceView*			skyl_textures_SRV[3];
	ID3D1xShaderResourceView*			moon_texture_SRV;
	
	// Shader constant buffers
	ID3D11Buffer*						earthShadowBuffer;

	int mapped_fade;
	ID3D11DeviceContext *mapped_context;
	D3D1x_MAPPED_TEXTURE3D loss_texture_mapped;
	D3D1x_MAPPED_TEXTURE3D insc_texture_mapped;
	D3D1x_MAPPED_TEXTURE3D skyl_texture_mapped;

	void MapFade(ID3D11DeviceContext *context,int s);
	void UnmapFade();
	D3DXMATRIX				world,view,proj;
	void DrawCube(void *context);
	void DrawLines(void *context,Vertext *lines,int vertex_count,bool strip=false);
	void PrintAt3dPos(void *context,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
};