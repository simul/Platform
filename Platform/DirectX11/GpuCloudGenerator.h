#pragma once

#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"

#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif


namespace simul
{
	namespace dx11
	{
		//! A generator for cloud volumes using DirectX 11.
		class GpuCloudGenerator: public simul::clouds::BaseGpuCloudGenerator
		{
		public:
			GpuCloudGenerator();
			~GpuCloudGenerator();
			bool CanPerformGPULighting() const
			{
				return Enabled&&renderPlatform!=NULL;
			}
			int GetDensityGridsize(const int *grid);
			void PerformGPURelight(	int light_index
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
		};
	}
}