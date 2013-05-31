#pragma once
#include "Simul/Sky/BaseGpuSkyGenerator.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"

#include <d3dx9.h>
#ifdef DX10
	#include <d3d10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
	#include <d3dx11effect.h>
#endif

namespace simul
{
	namespace dx11
	{
		class GpuSkyGenerator:public simul::sky::BaseGpuSkyGenerator
		{
		public:
			GpuSkyGenerator();
			~GpuSkyGenerator();
		// Implementing BaseGpuSkyGenerator
			void RestoreDeviceObjects(void *dev);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			//! Return true if the derived class can make sky tables using the GPU.
			bool CanPerformGPUGeneration() const;
			void Make2DLossAndInscatterTextures(simul::sky::AtmosphericScatteringInterface *skyInterface,int NumElevations,int NumDistances,
				simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl
				,const std::vector<float> &altitudes_km,float max_distance_km
				,simul::sky::float4 sun_irradiance
				,simul::sky::float4 starlight
				,simul::sky::float4 dir_to_sun,simul::sky::float4 dir_to_moon,float haze
				,float overcast,float overcast_base_km,float overcast_range_km
				,int index,int end_index,const simul::sky::float4 *density_table,const simul::sky::float4 *optical_table
		,const simul::sky::float4 *blackbody_table,int table_size,float maxDensityAltKm,bool InfraRed
				,float emissivity
				,float seaLevelTemperatureK);
		protected:
			FramebufferDX1x	fb[2];
			ID3D1xDevice*					m_pd3dDevice;
			ID3D11DeviceContext*			m_pImmediateContext;
			ID3D1xEffect*					effect;
			ID3D1xEffectTechnique*			lossTechnique;
			ID3D1xEffectTechnique*			inscTechnique;
			ID3D1xEffectTechnique*			skylTechnique;
			
			ID3D1xBuffer*					constantBuffer;
	
			ID3DX11EffectConstantBuffer		*gpuSkyConstants;
		};
	}
}