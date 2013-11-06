// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulSkyRendererDX1x.h A renderer for skies.

#pragma once
#include "Simul/Sky/SkyTexturesCallback.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Math/Matrix4x4.h"
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx11effect.h>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/HLSL/CppHLSL.hlsl"
#include "Simul/Platform/DirectX11/GpuSkyGenerator.h"

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

namespace simul
{
	namespace dx11
	{
//! A renderer for skies, this class will manage an instance of simul::sky::SkyNode and use it to calculate sky colours
//! in real time for the simul_sky.fx shader.
SIMUL_DIRECTX11_EXPORT_CLASS SimulSkyRendererDX1x:public simul::sky::BaseSkyRenderer
{
public:
	SimulSkyRendererDX1x(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *mem);
	virtual ~SimulSkyRendererDX1x();

	//standard d3d object interface functions
	void RecompileShaders();
	//! Call this when the D3D device has been created or reset.
	void RestoreDeviceObjects(void* pd3dDevice);
	//! Call this when the D3D device has been shut down.
	void InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	bool Destroy();
	//! \deprecated This function is no longer used, as the sky is drawn by the atmospherics renderer. See simul::sky::BaseAtmosphericsRenderer.
	bool Render(void *context,bool blend);
	bool RenderPointStars(void *context,float exposure);
	void RenderSun(void *context,float exposure);
	//! Draw the fade textures to screen
	bool RenderFades(void *context,int w,int h);
	virtual void RenderPlanet(void *c,void* tex,float rad,const float *dir,const float *colr,bool do_lighting);
	//! Call this to draw the sun flare, usually drawn last, on the main render target.
	bool RenderFlare(float exposure);
	bool Render2DFades(void *context);
	void RenderIlluminationBuffer(void *context);
	//! Get a value, from zero to one, which represents how much of the sun is visible.
	//! Call this when the current rendering surface is the one that has obscuring
	//! objects like mountains etc. in it, and make sure these have already been drawn.
	//! GetSunOcclusion executes a pseudo-render of an invisible billboard, then
	//! uses a hardware occlusion query to see how many pixels have passed the z-test.
	float CalcSunOcclusion(float cloud_occlusion=0.f);
	//! Call this once per frame to set the matrices.
	void SetMatrices(const simul::math::Matrix4x4 &view,const simul::math::Matrix4x4 &proj);

	void Get2DLossAndInscatterTextures(void* *loss,void* *insc,void* *skyl,void* *overc);
	void *GetIlluminationTexture();
			void *GetLightTableTexture();

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
	void DrawCubemap(void *context,ID3D1xShaderResourceView*	m_pCubeEnvMapSRV,D3DXMATRIX view,D3DXMATRIX proj);
	simul::sky::BaseGpuSkyGenerator *GetBaseGpuSkyGenerator(){return &gpuSkyGenerator;}
protected:
	int cycle;
	bool IsYVertical(){return false;}

	void CreateFadeTextures();
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate(void *c);
	void EnsureTextureCycle();
	
	void BuildStarsBuffer();
	
	ID3D1xDevice*						m_pd3dDevice;
	ID3D1xBuffer*						m_pVertexBuffer;
	ID3D1xInputLayout*					m_pStarsVtxDecl;
	ID3D1xBuffer*						m_pStarsVertexBuffer;
	ID3D1xEffect*						m_pSkyEffect;
	ID3D1xQuery*						d3dQuery;

	ID3D1xEffectTechnique*				m_hTechniqueFade3DTo2D;
	ID3D1xEffectTechnique*				m_hTechniqueSun;
	ID3D1xEffectTechnique*				m_hTechniqueQuery;
	ID3D1xEffectTechnique*				m_hTechniqueFlare;
	ID3D1xEffectTechnique*				m_hTechniquePlanet;
	ID3D1xEffectTechnique*				m_hTechniquePointStars;

			ID3D1xEffectTechnique*				m_TechniqueLightTableInterp;

	ID3D1xEffectShaderResourceVariable*	flareTexture;
	ID3D1xEffectShaderResourceVariable*	inscTexture;
	ID3D1xEffectShaderResourceVariable*	skylTexture;
	ID3D1xEffectShaderResourceVariable*	fadeTexture1;
	ID3D1xEffectShaderResourceVariable*	fadeTexture2;
	ID3D1xEffectShaderResourceVariable*	illuminationTexture;
	
	ConstantBuffer<EarthShadowUniforms>	earthShadowUniforms;
	ConstantBuffer<SkyConstants>		skyConstants;
void SetConstantsForPlanet(SkyConstants &skyConstants,const float *viewmatrix,const float *projmatrix,const float *dir,const float *light_dir);

	TextureStruct						loss_textures[3];
	TextureStruct						insc_textures[3];
	TextureStruct						skyl_textures[3];
			TextureStruct						light_table;

	// Small framebuffers we render to once per frame to perform fade interpolation.
	simul::dx11::Framebuffer*			loss_2d;
	simul::dx11::Framebuffer*			inscatter_2d;
	simul::dx11::Framebuffer*			overcast_2d;
	simul::dx11::Framebuffer*			skylight_2d;
	TextureStruct						light_table_2d;

	// A framebuffer where x=azimuth, y=elevation, r=start depth, g=end depth.
	simul::dx11::Framebuffer			illumination_fb;
	ID3D1xShaderResourceView*			flare_texture_SRV;
	ID3D1xShaderResourceView*			moon_texture_SRV;

	void MapFade(ID3D11DeviceContext *context,int s);
	void UnmapFade(int i);
	simul::math::Matrix4x4				world,view,proj;
	void DrawLines(void *context,Vertext *lines,int vertex_count,bool strip=false);
	void PrintAt3dPos(void *context,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
	simul::dx11::GpuSkyGenerator gpuSkyGenerator;
};
}
}