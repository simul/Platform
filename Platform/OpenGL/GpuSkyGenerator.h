#pragma once
#include "Simul/Sky/BaseGpuSkyGenerator.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"

namespace simul
{
	namespace opengl
	{
		class GpuSkyGenerator:public simul::sky::BaseGpuSkyGenerator
		{
		public:
			GpuSkyGenerator();
			virtual ~GpuSkyGenerator();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			//! Return true if the derived class can make sky tables using the GPU.
			bool CanPerformGPUGeneration() const;
			/*void MakeLossAndInscatterTextures(sky::float4 wavelengthsNmm,int cycled_index,
				simul::sky::AtmosphericScatteringInterface *skyInterface
				,const simul::sky::GpuSkyParameters &gpuSkyParameters
				,const simul::sky::GpuSkyAtmosphereParameters &gpuSkyAtmosphereParameters
				,const simul::sky::GpuSkyInfraredParameters &gpuSkyInfraredParameters);*/
		protected:
		// framebuffer to render out by distance.
			FramebufferGL		fb[2];
		};
	}
}