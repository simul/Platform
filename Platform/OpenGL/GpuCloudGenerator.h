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
			void PerformFullGPURelight(float *target_direct_grid);
			void GPUTransferDataToTexture(unsigned char *target_texture
									,const float *light_grid
									,const float *ambient_grid);
		protected:
			FramebufferGL	fb[2];
		};
	}
}