#pragma once
#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/Export.h"

namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT GpuCloudGenerator : public simul::clouds::BaseGpuCloudGenerator
		{
		public:
			GpuCloudGenerator();
			virtual ~GpuCloudGenerator();
			void RestoreDeviceObjects(void *dev);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			bool CanPerformGPULighting() const
			{
				return false;//Enabled;
			}
			int GetDensityGridsize(const int *grid);
			void* Make3DNoiseTexture(int noise_size,const float *noise_src_ptr);
			void CycleTexturesForward();
			void FillDensityGrid(int index,const clouds::GpuCloudsParameters &params
											,int start_texel
											,int texels
											,const simul::clouds::MaskMap &masks);
			virtual void PerformGPURelight(int index,float *target
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
											,const float *light,const int *light_grid
											,const float *ambient,const int *density_grid
											,int start_texel
											,int texels
											,bool wrap_light_tex);
		protected:
			FramebufferGL	fb[2];
			FramebufferGL	world_fb;
			FramebufferGL	dens_fb;
			GLuint			density_program;
			GLuint			lighting_program;
			GLuint			transform_program;
			GLuint volume_noise_tex;
			GLenum			iformat;
			GLenum			itype;
			GLuint			density_texture;
			bool			readback_to_cpu;
			float			*density;	// used if we are using CPU to read back the density texture.
			int				density_gridsize;
			GLuint			gpuCloudConstantsUBO;
			GLint			gpuCloudConstantsBindingIndex;
		};
	}
}