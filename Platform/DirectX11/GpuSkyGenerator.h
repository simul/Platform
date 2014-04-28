#pragma once
#include "Simul/Sky/BaseGpuSkyGenerator.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#if WINVER<0x0602
#include <d3dx9.h>
#endif
#include <d3d11.h>
#if WINVER<0x0602
#include <d3dx11.h>
#endif
#include "Simul/External/DirectX/Effects11/Inc/D3dx11effect.h"

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
			void MakeLossAndInscatterTextures(int cycled_index,
				simul::sky::AtmosphericScatteringInterface *skyInterface
				,const sky::GpuSkyParameters &gpuSkyParameters
				,const sky::GpuSkyAtmosphereParameters &gpuSkyAtmosphereParameters
				,const sky::GpuSkyInfraredParameters &gpuSkyInfraredParameters);
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
			ID3D11Device*					m_pd3dDevice;
			ID3D11DeviceContext*			m_pImmediateContext;
			ID3DX11Effect*					effect;
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