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
			void Make2DLossAndInscatterTextures(simul::sky::AtmosphericScatteringInterface *skyInterface,int NumElevations,int NumDistances,
				simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl
				,const std::vector<float> &altitudes_km,float max_distance_km
				,simul::sky::float4 sun_irradiance
				,simul::sky::float4 dir_to_sun,simul::sky::float4 dir_to_moon,float haze
				,float overcast,float overcast_base_km,float overcast_range_km
				,int index,int end_index,const simul::sky::float4 *density_table,const simul::sky::float4 *optical_table,int table_size,float maxDensityAltKm,bool InfraRed);
		protected:
		// framebuffer to render out by distance.
			FramebufferGL	fb[2];
			GLuint			loss_program;
			GLuint			insc_program;
			GLuint			skyl_program;
		};
	}
}