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
				return Enabled;
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
			FramebufferGL	fb[2];
			FramebufferGL	world_fb;
			FramebufferGL	dens_fb;
			GLuint			density_program;
			GLuint			clouds_program;
			GLuint			transform_program;
			GLenum			iformat;
			GLenum			itype;
			GLuint			density_texture;
			bool			readback_to_cpu;
			float			*density;	// used if we are using CPU to read back the density texture.
			int				density_gridsize;
		};
	}
}