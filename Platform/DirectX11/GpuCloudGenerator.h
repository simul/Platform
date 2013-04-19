#pragma once

#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"

#include <d3dx9.h>
#ifdef DX10
	#include <d3d10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
	#include <d3dx11effect.h>
#endif

namespace simul
{
	namespace dx11
	{
		class GpuCloudGenerator: public simul::clouds::BaseGpuCloudGenerator
		{
		public:
			GpuCloudGenerator();
			~GpuCloudGenerator();
			void CreateVolumeNoiseTexture(int size,const float *src_ptr);
			void RestoreDeviceObjects(void *dev);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			bool CanPerformGPULighting() const
			{
				return Enabled&&m_pd3dDevice!=NULL;
			}
			int GetDensityGridsize(const int *grid);
			void *FillDensityGrid(const int *grid
									,int start_texel
									,int texels
									,float humidity
									,float baseLayer
									,float transition
									,float time
									,int noise_size,int octaves,float persistence
									,const float  *noise_src_ptr);
			virtual void PerformGPURelight(float *target
									,const int *light_grid
									,int start_texel
									,int texels
									,const int *density_grid
									,const float *Matrix4x4LightToDensityTexcoords,const float *lightspace_extinctions_float3);
			void GPUTransferDataToTexture(unsigned char *target
											,const float *DensityToLightTransform
											,const float *light,const int *light_grid
											,const float *ambient,const int *density_grid
											,int start_texel
											,int texels);
		protected:
			FramebufferDX1x	fb[2];
			FramebufferDX1x	world_fb;
			FramebufferDX1x	dens_fb;
			ID3D1xDevice*					m_pd3dDevice;
			ID3D1xDeviceContext*			m_pImmediateContext;
			ID3D1xEffect*					effect;
			ID3D1xEffectTechnique*			densityTechnique;
			ID3D1xEffectTechnique*			lightingTechnique;
			ID3D1xEffectTechnique*			transformTechnique;
			int								density_gridsize;
			ID3D11Texture3D					*density_texture;
			ID3D11ShaderResourceView		*density_texture_srv;
			ID3D11Buffer					*gpuCloudConstantsBuffer;
		};
	}
}