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
			void CycleTexturesForward();
			void FillDensityGrid(	int index
									,const int *grid
									,int start_texel
									,int texels
									,float humidity
									,float baseLayer
									,float transition
									,float upperDensity
									,float time
									,void* noise_tex,int octaves,float persistence
											,bool mask);
			virtual void PerformGPURelight(	int light_index
											,float *target
											,const int *light_grid
											,int start_texel
											,int texels
											,const int *density_grid
											,const float *Matrix4x4LightToDensityTexcoords
											,const float *lightspace_extinctions_float3);
			void GPUTransferDataToTexture(	int index
											,unsigned char *target
											,const float *DensityToLightTransform
											,const float *light
											,const int *light_grid
											,const float *ambient
											,const int *density_grid
											,int start_texel
											,int texels);
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
			
			ID3D1xDevice*						m_pd3dDevice;
			ID3D11DeviceContext*				m_pImmediateContext;
			ID3D1xEffect*						effect;
			ID3D1xEffectTechnique*				densityTechnique;
			ID3D1xEffectTechnique*				maskTechnique;
			ID3D1xEffectTechnique*				lightingTechnique;
			ID3D1xEffectTechnique*				transformTechnique;
			ID3D11Texture3D						*volume_noise_tex;
			ID3D11ShaderResourceView			*volume_noise_tex_srv;
			ID3D11Texture3D						*density_texture;
			TextureStruct						*finalTexture[3];
			TextureStruct						lightTextures[2];
			ID3D11ShaderResourceView			*density_texture_srv;
			ConstantBuffer<GpuCloudConstants>	gpuCloudConstants;

		};
	}
}