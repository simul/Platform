#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/spherical_harmonics_constants.sl"
#include "Platform/Shaders/SL/light_probe_constants.sl"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace crossplatform
	{
		/// A class that calculates spherical harmonics from a cubemap, and stores the results in a structured buffer.
		class SIMUL_CROSSPLATFORM_EXPORT SphericalHarmonics
		{
		public:
			SphericalHarmonics();
			~SphericalHarmonics();
			//! Platform-dependent function called when initializing the Spherical Harmonics.
			void RestoreDeviceObjects(RenderPlatform *r);
			//! Platform-dependent function called when uninitializing the Spherical Harmonics.
			void InvalidateDeviceObjects();
			//! Make sure no invalid data is retained.
			void ResetBuffers();
			//! Platform-dependent function to reload the shaders - only use this for debug purposes.
			void RecompileShaders();
			int bands;
			void SetBands(int b)
			{
				if (b>MAX_SH_BANDS)
					b = MAX_SH_BANDS;
				if (bands != b)
				{
					bands = b;
					sphericalHarmonics.InvalidateDeviceObjects();
				}
			}
			//! Calculate the spherical harmonics of this cubemap and store the result internally.
			//! Changing the number of bands will resize the internal storeage.
			void CalcSphericalHarmonics(DeviceContext &deviceContext,Texture *texture);

			StructuredBuffer<vec4> &GetSphericalHarmonics()
			{
				return sphericalHarmonics;
			}
			//! Probe values from cubemap.
			bool Probe(DeviceContext &deviceContext
				,Texture *buffer_texture
				,int mip_size
				,int face_index
				,uint2 pos
				,uint2 size
				,vec4 *targetValuesFloat4);
			/// Draw a diffuse environment map to the specified framebuffer.
			void RenderEnvmap(GraphicsDeviceContext &deviceContext,crossplatform::Texture *target,int cubemapIndex,float blend);
			/// Taking the zero mip as the initial data source, use the formula roughness=mip/max_mip to render it down to the lower mips.
			void RenderMipsByRoughness(GraphicsDeviceContext &deviceContext, crossplatform::Texture *target);
			/// Copy from a given mip face to the next one down, with blending or without (if blend is 0).
			void CopyMip(GraphicsDeviceContext &deviceContext,Texture *tex,int face,int mip,float blend);
		protected:
			RenderPlatform *renderPlatform;
			StructuredBuffer<vec4>	probeResultsRW;
			StructuredBuffer<SphericalHarmonicsSample>	sphericalSamples;
			ConstantBuffer<SphericalHarmonicsConstants>	sphericalHarmonicsConstants;
			StructuredBuffer<vec4>						sphericalHarmonics;
			Effect										*sphericalHarmonicsEffect;
			int											shSeed;
			crossplatform::Effect						*lightProbesEffect;
			crossplatform::EffectTechnique				*mip_from_roughness_blend;
			crossplatform::EffectTechnique				*mip_from_roughness_no_blend;
			crossplatform::EffectTechnique				*jitter;
			crossplatform::EffectTechnique				*encode;
			crossplatform::ConstantBuffer<LightProbeConstants>	lightProbeConstants;
			crossplatform::ShaderResource				_samplesBuffer;
			crossplatform::ShaderResource				_targetBuffer;
			crossplatform::ShaderResource				_basisBuffer;
			crossplatform::ShaderResource				_samplesBufferRW;
			crossplatform::ShaderResource				_cubemapTexture;
			
		};

	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

