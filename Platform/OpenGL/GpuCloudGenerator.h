#pragma once
#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"

namespace simul
{
	namespace opengl
	{
		class GpuCloudGenerator : public simul::clouds::BaseGpuCloudGenerator
		{
		public:
			GpuCloudGenerator();
			virtual ~GpuCloudGenerator();
			void CreateVolumeNoiseTexture(int size,const float *src_ptr);
			void RestoreDeviceObjects(void *dev);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			bool CanPerformGPULighting() const
			{
				return true;
			}
			void FillDensityGrid(float *target,const int *grid
											,float humidity
											,float time
											,int noise_size,int octaves,float persistence
											,const float  *noise_src_ptr);
			void PerformFullGPURelight(float *target,const int *,const float *dens,const int *density_grid,const float *Matrix4x4LightToDensityTexcoords,const float *lightspace_extinctions_float3);
			void GPUTransferDataToTexture(unsigned char *target
											,const float *DensityToLightTransform
											,const float *light,const int *light_grid
											,const float *ambient
											,const float *density,const int *density_grid);
		protected:
			FramebufferGL	fb[2];
			FramebufferGL	world_fb;
			FramebufferGL	dens_fb;
		};
	}
}