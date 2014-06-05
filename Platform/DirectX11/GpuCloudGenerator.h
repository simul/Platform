#pragma once

#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Texture.h"

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
			void RestoreDeviceObjects(void *dev);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			bool CanPerformGPULighting() const
			{
				return Enabled&&m_pd3dDevice!=NULL;
			}
			int GetDensityGridsize(const int *grid);
			void* Make3DNoiseTexture(int noise_size,const float  *noise_src_ptr,int generation_number);
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
			void SetDirectTargets(dx11::Texture **textures)
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
			simul::dx11::Framebuffer			mask_fb;
			
			ID3D11Device*						m_pd3dDevice;
			ID3D11DeviceContext*				m_pImmediateContext;
			ID3DX11Effect*						effect;
			ID3DX11EffectTechnique*				densityComputeTechnique;
			ID3DX11EffectTechnique*				maskTechnique;
			ID3DX11EffectTechnique*				lightingComputeTechnique;
			ID3DX11EffectTechnique*				secondaryLightingComputeTechnique;
			ID3DX11EffectTechnique*				secondaryHarmonicTechnique;
			ID3DX11EffectTechnique*				transformComputeTechnique;

			ID3D11Texture3D						*volume_noise_tex;
			ID3D11ShaderResourceView			*volume_noise_tex_srv;

			dx11::Texture						density_texture;
			dx11::Texture						*finalTexture[3];
			dx11::Texture						directLightTextures[2];
			dx11::Texture						indirectLightTextures[2];
			ConstantBuffer<GpuCloudConstants>	gpuCloudConstants;
			ID3D11SamplerState*					m_pWwcSamplerState;
			ID3D11SamplerState*					m_pWccSamplerState;
			ID3D11SamplerState*					m_pCwcSamplerState;
	bool harmonic_secondary;
		};
	}
}