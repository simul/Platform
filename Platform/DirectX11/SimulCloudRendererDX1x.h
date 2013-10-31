// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulCloudRendererDX1x.h A DirectX 10/11 renderer for clouds. Create an instance of this class in a dx10 program
// and use the Render() function once per frame.

#pragma once

#include <tchar.h>
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#include "Simul/Graph/Meta/Group.h"

#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/GpuCloudGenerator.h"

namespace simul
{
	namespace clouds
	{
		class CloudInterface;
		class LightningRenderInterface;
		class CloudGeometryHelper;
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

namespace simul
{
	namespace dx11
	{
	//! A cloud rendering class. Create an instance of this class within a DirectX program.
		SIMUL_DIRECTX11_EXPORT_CLASS SimulCloudRendererDX1x : public simul::clouds::BaseCloudRenderer
		{
		public:
			SimulCloudRendererDX1x(simul::clouds::CloudKeyframer *cloudKeyframer,simul::base::MemoryInterface *mem);
			virtual ~SimulCloudRendererDX1x();
			void RecompileShaders();
			//! Call this when the D3D device has been created or reset
			void RestoreDeviceObjects( void* pd3dDevice);
			//! Call this when the 3D device has been lost.
			void InvalidateDeviceObjects();
			//! Call this to release the memory for D3D device objects.
			bool Destroy();
			void PreRenderUpdate(void *context);
			//! Call this to draw the clouds, including any illumination by lightning.
			bool Render(void *context,float exposure,bool cubemap,bool near_pass,const void *depth_tex,bool default_fog,bool write_alpha,int viewport_id,const simul::sky::float4& viewportTextureRegionXYWH);
			void RenderDebugInfo(void *context,int width,int height);
			void RenderAuxiliaryTextures(void *context,int width,int height);
			void RenderCrossSections(void *context,int width,int height);
			//! Call this to render the lightning bolts (cloud illumination is done in the main Render function).
			bool RenderLightning(void *context,int viewport_id);
			//! Call this once per frame to set the matrices.
			void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
			//! Return true if the camera is above the cloudbase altitude.
			bool IsCameraAboveCloudBase() const;
			void SetEnableStorms(bool s);
			float GetTiming() const;
			//! Get the list of three textures used for cloud rendering.
			CloudShadowStruct GetCloudShadowTexture();
			void SetLossTexture(void *t);
			void SetInscatterTextures(void* i,void *s,void *o);
			void SetIlluminationTexture(void *i);
			void SetLightTableTexture(void *l);
			simul::clouds::BaseGpuCloudGenerator *GetBaseGpuCloudGenerator(){return &gpuCloudGenerator;}

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
			simul::clouds::BaseGpuCloudGenerator *GetGpuCloudGenerator(){return &gpuCloudGenerator;}
		protected:
			simul::dx11::GpuCloudGenerator gpuCloudGenerator;
			void RenderCombinedCloudTexture(void *context);
			void RenderCloudShadowTexture(void *context);
			void DrawLines(void *context,VertexXyzRgba *vertices,int vertex_count,bool strip);
			// Make up to date with respect to keyframer:
			void EnsureCorrectTextureSizes();
			void EnsureTexturesAreUpToDate(void *context);
			void EnsureCorrectIlluminationTextureSizes();
			void EnsureIlluminationTexturesAreUpToDate();
			void EnsureTextureCycle();

			void CreateMeshBuffers();
			void Unmap();
			void Map(ID3D11DeviceContext *context,int texture_index);
			unsigned texel_index[4];
			bool lightning_active;
			ID3D1xDevice*							m_pd3dDevice;
		
			ID3D1xInputLayout*						m_pLightningVtxDecl;
			ID3D11SamplerState*						m_pWrapSamplerState;
			ID3D11SamplerState*						m_pClampSamplerState;

			ID3D1xEffect*							m_pLightningEffect;
			ID3D1xEffectTechnique*					m_hTechniqueLightning;
			ID3D1xEffect*							m_pCloudEffect;
			ID3D1xEffectTechnique*					m_hTechniqueCloud;
			ID3D1xEffectTechnique*					m_hTechniqueRaytraceNearPass;
			ID3D1xEffectTechnique*					m_hTechniqueRaytraceForward;
			ID3D1xEffectTechnique*					m_hTechniqueSimpleRaytrace;
			ID3D1xEffectTechnique*					m_hTechniqueRaytrace3DNoise;
			ID3D1xEffectTechnique*					m_hTechniqueCloudsAndLightning;

			ID3D1xEffectTechnique*					m_hTechniqueCrossSectionXZ;
			ID3D1xEffectTechnique*					m_hTechniqueCrossSectionXY;
			
			ConstantBuffer<CloudConstants>			cloudConstants;
			StructuredBuffer<SmallLayerData>		layerBuffer;
			ID3D11Buffer*							cloudPerViewConstantBuffer;
			ID3D11Buffer*							layerConstantsBuffer;
			ID3D1xEffectMatrixVariable* 			l_worldViewProj;
			
			ID3D1xEffectShaderResourceVariable*		cloudDensity;
			ID3D1xEffectShaderResourceVariable*		cloudDensity1;
			ID3D1xEffectShaderResourceVariable*		cloudDensity2;
			ID3D1xEffectShaderResourceVariable*		noiseTexture;
			ID3D1xEffectShaderResourceVariable*		noiseTexture3D;

			ID3D1xEffectShaderResourceVariable*		lightningIlluminationTexture;
			ID3D1xEffectShaderResourceVariable*		skyLossTexture;
			ID3D1xEffectShaderResourceVariable*		skyInscatterTexture;
			ID3D1xEffectShaderResourceVariable*		skylightTexture;
			ID3D1xEffectShaderResourceVariable*		depthTexture;
			ID3D1xEffectShaderResourceVariable*		lightTableTexture;

			TextureStruct							cloud_textures[3];

			ID3D1xShaderResourceView*				noiseTextureResource;
			ID3D1xShaderResourceView*				lightningIlluminationTextureResource;
			ID3D1xShaderResourceView*				skyLossTexture_SRV;
			ID3D1xShaderResourceView*				skyInscatterTexture_SRV;
			ID3D1xShaderResourceView*				overcInscTexture_SRV;
			ID3D1xShaderResourceView*				skylightTexture_SRV;
			ID3D1xShaderResourceView*				illuminationTexture_SRV;
			ID3D1xShaderResourceView*				lightTableTexture_SRV;
			simul::dx11::Framebuffer				shadow_fb;

			// A texture whose x-axis represents azimuth, and whose y-axis represents distance
			// as a proportion of shadow range. The texels represent how much illumination accumulates between the viewer
			// and that distance.
			simul::dx11::Framebuffer				godrays_fb;

			simul::dx11::TextureStruct				cloud_texture;
			
			ID3D1xBuffer*							computeConstantBuffer;
			ID3D11ComputeShader*					m_pComputeShader;

			ID3D1xTexture3D*						illumination_texture;
			
			D3D1x_MAPPED_TEXTURE3D					mapped_illumination;

			ID3D11Texture2D*	noise_texture;
			TextureStruct		noise_texture_3D;
			ID3D1xTexture1D*	lightning_texture;
			ID3D1xTexture2D*	cloud_cubemap;
			
			ID3D1xBlendState*	blendAndWriteAlpha;
			ID3D1xBlendState*	blendAndDontWriteAlpha;
			
			D3DXMATRIX			view,proj;

			bool UpdateIlluminationTexture(float dt);
			float LookupLargeScaleTexture(float x,float y);

			bool CreateLightningTexture();
			void CreateNoiseTexture(void *context);
			void Create3DNoiseTexture(void *context);
			bool CreateCloudEffect();
			bool MakeCubemap(); // not ready yet
			void RenderNoise(void *context);
			
			bool enable_lightning;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif