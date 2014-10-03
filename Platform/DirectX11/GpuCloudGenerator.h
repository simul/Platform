#pragma once

#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"

#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif


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
			void SetHarmonicLighting(bool h)
			{
				harmonic_secondary=h;
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			bool CanPerformGPULighting() const
			{
				return Enabled&&renderPlatform!=NULL;
			}
			int GetDensityGridsize(const int *grid);
			crossplatform::Texture* Make3DNoiseTexture(int noise_size,const float  *noise_src_ptr,int generation_number);
			void FillDensityGrid(	int index
									,const clouds::GpuCloudsParameters &params
									,int start_texel
									,int texels);
			void PerformGPURelight(	int light_index
									,const clouds::GpuCloudsParameters &params
									,float *target
									,int start_texel
									,int texels);
			void GPUTransferDataToTexture(	int cycled_index
											,const clouds::GpuCloudsParameters &params
											,unsigned char *target
											,int start_texel
											,int texels);
			// If we want the generator to put the data directly into 3d textures:
			void SetDirectTargets(crossplatform::Texture **textures)
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
			crossplatform::BaseFramebuffer		*mask_fb;
			
			crossplatform::Effect*				effect;
			crossplatform::EffectTechnique*		densityComputeTechnique;
			crossplatform::EffectTechnique*		maskTechnique;
			crossplatform::EffectTechnique*		lightingComputeTechnique;
			crossplatform::EffectTechnique*		secondaryLightingComputeTechnique;
			crossplatform::EffectTechnique*		secondaryHarmonicTechnique;
			crossplatform::EffectTechnique*		transformComputeTechnique;

			crossplatform::Texture				*volume_noise_tex;

			crossplatform::Texture				*density_texture;
			crossplatform::Texture				*finalTexture[3];
			crossplatform::Texture				*directLightTextures[2];
			crossplatform::Texture				*indirectLightTextures[2];
			crossplatform::ConstantBuffer<GpuCloudConstants>	gpuCloudConstants;
			crossplatform::SamplerState*	m_pWwcSamplerState;
			crossplatform::SamplerState*	m_pWccSamplerState;
			crossplatform::SamplerState*	m_pCwcSamplerState;
	bool harmonic_secondary;
		};
	}
}