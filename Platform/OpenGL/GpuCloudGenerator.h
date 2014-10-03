#pragma once
#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/Export.h"
#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT GpuCloudGenerator : public simul::clouds::BaseGpuCloudGenerator
		{
		public:
			GpuCloudGenerator();
			virtual ~GpuCloudGenerator();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			bool CanPerformGPULighting() const
			{
				return Enabled;
			}
			int GetDensityGridsize(const int *grid);
			crossplatform::Texture* Make3DNoiseTexture(int noise_size,const float *noise_src_ptr,int generation_number);
			void CycleTexturesForward();
			void FillDensityGrid(int index,const clouds::GpuCloudsParameters &params
									,int start_texel
									,int texels);
			virtual void PerformGPURelight(int light_index
											,const clouds::GpuCloudsParameters &params
											,float *target
											,int start_texel
											,int texels);
			void GPUTransferDataToTexture(	int cycled_index
												,const clouds::GpuCloudsParameters &params
												,unsigned char *target
												,int start_texel
												,int texels);
		protected:
			FramebufferGL	fb[2];
			FramebufferGL	world_fb;
			FramebufferGL	dens_fb;
			GLuint			transform_program;
			GLuint			volume_noise_tex;
			crossplatform::PixelFormat			iformat;
			GLenum			itype;
			GLuint			density_texture;
			bool			readback_to_cpu;
			float			*density;	// used if we are using CPU to read back the density texture.
			int				density_gridsize;
			simul::opengl::ConstantBuffer<GpuCloudConstants> gpuCloudConstants;
			Texture	directLightTextures[2];
			Texture	indirectLightTextures[2];
			Texture	maskTexture;
			int				last_generation_number;
		};
	}
}