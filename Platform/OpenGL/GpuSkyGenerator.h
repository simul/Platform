#pragma once
#include "Simul/Sky/BaseGpuSkyGenerator.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"

namespace simul
{
	namespace opengl
	{
		class GpuSkyGenerator:public simul::sky::BaseGpuSkyGenerator
		{
		public:
			GpuSkyGenerator();
			virtual ~GpuSkyGenerator();
			void RestoreDeviceObjects(void *dev);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			//! Return true if the derived class can make sky tables using the GPU.
			bool CanPerformGPUGeneration() const;
			void Make2DLossAndInscatterTextures(int cycled_index,
				simul::sky::AtmosphericScatteringInterface *skyInterface
				,const simul::sky::GpuSkyParameters &gpuSkyParameters
				,const simul::sky::GpuSkyAtmosphereParameters &gpuSkyAtmosphereParameters
				,const simul::sky::GpuSkyInfraredParameters &gpuSkyInfraredParameters);
			virtual void CopyToMemory(int cycled_index,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl);
			virtual void CopyToMemory(int cycled_index,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl);
		protected:
		// framebuffer to render out by distance.
			FramebufferGL	fb[2];
			GLuint			loss_program;
			GLuint			insc_program;
			GLuint			skyl_program;
			GLuint gpuSkyConstantsUBO;
			GLint gpuSkyConstantsBindingIndex;
			simul::sky::float4			*loss_cache;
			simul::sky::float4			*insc_cache;
			simul::sky::float4			*skyl_cache;
			int				cache_size;
		};
	}
}