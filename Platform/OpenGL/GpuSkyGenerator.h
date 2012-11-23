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
			void Make2DLossAndInscatterTextures(simul::sky::SkyInterface *skyInterface,int NumElevations,int NumDistances,
				simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl,const std::vector<float> &altitudes_km,float max_distance_km,int index,int end_index,bool InfraRed);
		protected:
		// framebuffer to render out by distance.
			FramebufferGL	fb[2];
		};
	}
}