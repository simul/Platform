#pragma once

#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "HLSL/CppHLSL.hlsl"
#include "Simul/Platform/CrossPlatform/simul_gpu_clouds.sl"

#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx11effect.h>

namespace simul
{
	namespace dx11
	{
		//! A generator for cloud volumes using DirectX 11.
		class GpuCloudGenerator: public simul::clouds::BaseGpuCloudGenerator
		{
		public:
			GpuCloudGenerator();
			~GpuCloudGenerator();
			void RestoreDeviceObjects(void *dev);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			bool CanPerformGPULighting() const
			{
				return Enabled&&m_pd3dDevice!=NULL;
			}
			int GetDensityGridsize(const int *grid);
			void* Make3DNoiseTexture(int noise_size,const float  *noise_src_ptr);
			void FillDensityGrid(	int index
									,const int *grid
									,int start_texel
									,int texels
									,float humidity
									,float baseLayer
									,float transition
									,float upperDensity
									,float diffusivity
									,float time
									,void* noise_tex
									,int octaves
									,float persistence
									,bool mask);
			virtual void PerformGPURelight(	int light_index
											,float *target
											,const int *light_grid
											,int start_texel
											,int texels
											,const int *density_grid
											,const float *Matrix4x4LightToDensityTexcoords
											,const float *lightspace_extinctions_float3
											,bool wrap_light_tex);
			void GPUTransferDataToTexture(	int index
											,unsigned char *target
											,const float *DensityToLightTransform
											,const float *light
											,const int *light_grid
											,const float *ambient
											,const int *density_grid
											,int start_texel
											,int texels
											,bool wrap_light_tex);
			// If we want the generator to put the data directly into 3d textures:
			void SetDirectTargets(TextureStruct **textures)
			{
				for(int i=0;i<3;i++)
				{
					if(textures)
						finalTexture[i]=textures[i];
					else
						finalTexture[i]=NULL;
				}
			}
		protected:
			simul::dx11::Framebuffer			fb[2];
			simul::dx11::Framebuffer			world_fb;
			simul::dx11::Framebuffer			dens_fb;
			simul::dx11::Framebuffer			mask_fb;
			
			ID3D11Device*						m_pd3dDevice;
			ID3D11DeviceContext*				m_pImmediateContext;
			ID3DX11Effect*						effect;
			ID3DX11EffectTechnique*				densityTechnique;
			ID3DX11EffectTechnique*				densityComputeTechnique;
			ID3DX11EffectTechnique*				maskTechnique;
			ID3DX11EffectTechnique*				lightingTechnique;
			ID3DX11EffectTechnique*				lightingComputeTechnique;
			ID3DX11EffectTechnique*				secondaryLightingComputeTechnique;
			ID3DX11EffectTechnique*				transformTechnique;
			ID3DX11EffectTechnique*				transformComputeTechnique;

			ID3D11Texture3D						*volume_noise_tex;
			ID3D11ShaderResourceView			*volume_noise_tex_srv;

			TextureStruct						density_texture;
			TextureStruct						*finalTexture[3];
			TextureStruct						directLightTextures[2];
			TextureStruct						indirectLightTextures[2];
			ConstantBuffer<GpuCloudConstants>	gpuCloudConstants;
			ID3D11SamplerState*					m_pWwcSamplerState;
			ID3D11SamplerState*					m_pCccSamplerState;
		};
	}
}