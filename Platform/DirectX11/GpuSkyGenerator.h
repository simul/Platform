#pragma once
#include "Simul/Sky/BaseGpuSkyGenerator.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "HLSL/CppHLSL.hlsl"
#include "Simul/Platform/CrossPlatform/simul_gpu_sky.sl"
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx11effect.h>

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
			void Make2DLossAndInscatterTextures(int cycled_index
				,simul::sky::AtmosphericScatteringInterface *skyInterface
				,int NumElevations,int NumDistances
				,const std::vector<float> &altitudes_km,float max_distance_km
				,simul::sky::float4 sun_irradiance
				,simul::sky::float4 starlight
				,simul::sky::float4 dir_to_sun,simul::sky::float4 dir_to_moon
				,const simul::sky::HazeStruct &hazeStruct
				,unsigned tables_checksum
				,float overcast_base_km,float overcast_range_km
				,int index,int end_index,const simul::sky::float4 *density_table,const simul::sky::float4 *optical_table
				,const simul::sky::float4 *blackbody_table,int table_size,float maxDensityAltKm,bool InfraRed
				,float emissivity
				,float seaLevelTemperatureK);
			void CopyToMemory(int cycled_index,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl);
			// If we want the generator to put the data directly into 3d textures:
			void SetDirectTargets(TextureStruct **loss,TextureStruct **insc,TextureStruct **skyl,TextureStruct *light_table)
			{
				for(int i=0;i<3;i++)
				{
					if(loss)
						finalLoss[i]=loss[i];
					else
						finalLoss[i]=NULL;
					if(insc)
						finalInsc[i]=insc[i];
					else
						finalInsc[i]=NULL;
					if(skyl)
						finalSkyl[i]=skyl[i];
					else
						finalSkyl[i]=NULL;
					this->light_table=light_table;
				}
			}
		protected:
			ID3D1xDevice*					m_pd3dDevice;
			ID3D11DeviceContext*			m_pImmediateContext;
			ID3D1xEffect*					effect;
			ID3DX11EffectTechnique*			lossComputeTechnique;
			ID3DX11EffectTechnique*			inscComputeTechnique;
			ID3DX11EffectTechnique*			skylComputeTechnique;
			ID3DX11EffectTechnique*			lightComputeTechnique;
			
			ID3D1xBuffer*					constantBuffer;
	
			ConstantBuffer<GpuSkyConstants>	gpuSkyConstants;
			TextureStruct					*finalLoss[3];
			TextureStruct					*finalInsc[3];
			TextureStruct					*finalSkyl[3];
			TextureStruct					*light_table;
			TextureStruct					dens_tex,optd_tex;

			unsigned						tables_checksum;
		};
	}
}