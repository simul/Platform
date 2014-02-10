
// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulCloudRenderer.h A DirectX renderer for clouds. Create an instance of this class in a DX program
// and use the Render() function once per frame.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Base/SmartPtr.h"
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Graph/Meta/Resource.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Platform/DirectX9/Framebuffer.h"
//#include "Simul/Platform/DirectX9/GpuCloudGenerator.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

typedef long HRESULT;
namespace simul
{
	namespace sound
	{
		namespace fmod
		{
			class NodeSound;
		}
	}
	namespace dx9
	{
//! A cloud rendering class. Create an instance of this class within a DirectX program,
//! or use SimulWeatherRenderer to manage cloud and sky rendering together.
SIMUL_DIRECTX9_EXPORT_CLASS SimulCloudRenderer
	: public simul::clouds::BaseCloudRenderer
	,public simul::graph::meta::ResourceUser<simul::graph::standardnodes::ShowProgressInterface>
{
public:
	SimulCloudRenderer(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem);
	virtual ~SimulCloudRenderer();
	//GpuCloudGenerator gpuCloudGenerator;
	META_BeginProperties
		META_ValueRangePropertyWithSetCall(int,NumBuffers,1,2,NumBuffersChanged,"Number of buffers to use in cloud rendering. More = faster.")
	META_EndProperties
	void RecompileShaders();
	//! Call this when the device has been created
	void RestoreDeviceObjects(void *pd3dDevice);
	//! Call this when the 3D device has been lost.
	void InvalidateDeviceObjects();
	void PreRenderUpdate(void *context);
	//! DX9 implementation of cloud rendering. For this platform, depth_testing and default_fog are ignored.
	bool Render(void *context,float exposure,bool cubemap,bool near_pass,const void *depth_alpha_tex,bool default_fog,bool write_alpha,int viewport_id,const simul::sky::float4& viewportTextureRegionXYWH);

	//! Save the first keyframe texture into a 2D image file by stacking X-Z slices vertically.
	void SaveCloudTexture(const char *filename);
	//! Draw clouds as horizontal layers
	bool RenderHorizontal(bool cubemap);
#if defined(XBOX) || defined(DOXYGEN)
	//! Call this once per frame to set the matrices (X360 only).
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
#endif
	const char *GetDebugText() const;
	float GetTiming() const;
	void *GetIlluminationTexture();
	void SetLossTexture(void *t1);
	void SetInscatterTextures(void *i,void *s,void *o);
	void SetIlluminationTexture(void *i);
	LPDIRECT3DTEXTURE9 GetNoiseTexture()
	{
		return (LPDIRECT3DTEXTURE9)noise_fb.GetColorTex();
	}
			void RenderCrossSections(void *context,int x0,int y0,int width,int height);
			void RenderAuxiliaryTextures(void *context,int x0,int y0,int width,int height);
	bool RenderLightVolume();
	void EnableFilter(bool f);
	bool IsYVertical() const{return y_vertical;}

protected:
	void DrawLines(void*,VertexXyzRgba *,int num,bool strip);
	// Make up to date with respect to keyframer:
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate(void*);
	void EnsureCorrectIlluminationTextureSizes();
	void EnsureIlluminationTexturesAreUpToDate();
	void EnsureTextureCycle();

	void NumBuffersChanged();

	void InternalRenderHorizontal(int viewport_id);
	void InternalRenderRaytrace(int viewport_id);
	void InternalRenderVolumetric(int viewport_id);
	bool wrap;
	struct PosVert_t
	{
		vec3 position;
	};
	struct Vertex_t
	{
		vec3 position;
		vec2 layerNoiseOffset;
		float layerFade;
		float layerDistance;
	};
	struct CPUFadeVertex_t : public Vertex_t
	{
		vec3 loss;
		vec3 inscatter;
	};
	Vertex_t *vertices;
	CPUFadeVertex_t *cpu_fade_vertices;
	void CreateNoiseTexture(void *context);
	simul::sound::fmod::NodeSound *sound;
	float timing;

	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pVtxDecl;
	LPDIRECT3DVERTEXDECLARATION9	m_pHudVertexDecl;

	LPD3DXEFFECT					m_pCloudEffect;
	D3DXHANDLE						m_hTechniqueCloud;		// Handle to technique in the effect
	D3DXHANDLE						m_hTechniqueCloudMask;		// Handle to technique in the effect
	D3DXHANDLE						m_hTechniqueCloudsAndLightning;	
	D3DXHANDLE						m_hTechniqueRaytraceWithLightning;	
	D3DXHANDLE						m_hTechniqueCrossSectionXZ;	
	D3DXHANDLE						m_hTechniqueCrossSectionXY;	
	D3DXHANDLE						m_hTechniqueRenderTo2DForSaving;
	D3DXHANDLE						m_hTechniqueColourLines;	
	

	D3DXHANDLE					worldViewProj;
	D3DXHANDLE					eyePosition;
	D3DXHANDLE					maxFadeDistanceMetres;
	D3DXHANDLE					lightResponse;
	D3DXHANDLE					crossSectionOffset;
	D3DXHANDLE					lightDir;
	D3DXHANDLE					skylightColour;
	D3DXHANDLE					fractalScale;
	D3DXHANDLE 					interp;
	D3DXHANDLE 					large_texcoords_scale;
	D3DXHANDLE 					mieRayleighRatio;
	D3DXHANDLE 					hazeEccentricity;
	D3DXHANDLE					cloudEccentricity;
	D3DXHANDLE					fadeInterp;
	D3DXHANDLE					distance;
	D3DXHANDLE					layerFade;
	D3DXHANDLE					alphaSharpness;
						
	D3DXHANDLE					lightningMultipliers;
	D3DXHANDLE					lightningColour;
	D3DXHANDLE					illuminationOrigin;
	D3DXHANDLE					illuminationScales;

	D3DXHANDLE					cloudDensity1;
	D3DXHANDLE					cloudDensity2;
	D3DXHANDLE					noiseTexture;
	D3DXHANDLE					largeScaleCloudTexture;
	D3DXHANDLE					lightningIlluminationTexture;
	D3DXHANDLE					skyLossTexture;
	D3DXHANDLE					skyInscatterTexture;
	D3DXHANDLE					skylightTexture;
	D3DXHANDLE					illuminationTexture;
	
	D3DXHANDLE					invViewProj;
	
	D3DXHANDLE					fractalRepeatLength;
	D3DXHANDLE					cloudScales;
	D3DXHANDLE					cloudOffset;
	D3DXHANDLE					raytraceLayerTexture;

	LPDIRECT3DVOLUMETEXTURE9	cloud_textures[3];
	simul::dx9::Framebuffer		noise_fb;
	LPDIRECT3DTEXTURE9			raytrace_layer_texture;
	LPDIRECT3DBASETEXTURE9		sky_loss_texture;
	LPDIRECT3DBASETEXTURE9		sky_inscatter_texture;
	LPDIRECT3DBASETEXTURE9		skylight_texture;
	LPDIRECT3DBASETEXTURE9		illumination_texture;
	LPDIRECT3DCUBETEXTURE9		cloud_cubemap;
	D3DXVECTOR4					lightning_colour;
	D3DXMATRIX					world,view,proj;
	LPDIRECT3DVERTEXBUFFER9		unitSphereVertexBuffer;
	LPDIRECT3DINDEXBUFFER9		unitSphereIndexBuffer;
	bool MakeCubemap(void *context); // not ready yet
	//! Once per frame, fill this 1-D texture with information on the layer distances and noise offsets
	bool FillRaytraceLayerTexture(int viewport_id);
	float last_time;
	bool rebuild_shaders;
};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif