#ifndef SIMUL_CROSSPLATFORM_HDRRENDERER_H
#define SIMUL_CROSSPLATFORM_HDRRENDERER_H
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/hdr_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/image_constants.sl"
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Effect.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT HdrRenderer
		{
		public:
			HdrRenderer();
			virtual ~HdrRenderer();
			META_BeginProperties
				META_ValueProperty(bool,Glow,"Whether to apply a glow effect")
			META_EndProperties
			void SetBufferSize(int w,int h);
			//! Call when we've got a fresh device - on startup or when the device has been restored.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			//! Render: write the given texture to screen using the HDR rendering shaders
			void Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma,float offsetX);
			void Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma);
			void RenderInfraRed(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,vec3 infrared_integration_factors,float Exposure,float Gamma);
			void RenderWithOculusCorrection(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma,float offsetX);
			//! Create the glow texture that will be overlaid due to strong lights.
			void RenderGlowTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture);
			void RenderDebug(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h);
			void RecompileShaders();
		protected:
			crossplatform::RenderPlatform		*renderPlatform;
			crossplatform::Texture				*brightpassTextures[4];
			simul::crossplatform::Texture		*glowTextures[4];
			int Width,Height;
			void DoGaussian(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *brightpassTexture,crossplatform::Texture *targetTexture);
		
			//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
			crossplatform::Effect*				hdr_effect;
			crossplatform::EffectTechnique*		exposureGammaTechnique;
			crossplatform::EffectTechnique*		glowExposureGammaTechnique;
			crossplatform::EffectTechnique*		warpExposureGamma;
			crossplatform::EffectTechnique*		warpGlowExposureGamma;
			
			crossplatform::EffectTechnique*		glowTechnique;

			crossplatform::Effect*				m_pGaussianEffect;
			crossplatform::EffectTechnique*		gaussianRowTechnique;
			crossplatform::EffectTechnique*		gaussianColTechnique;

			crossplatform::ConstantBuffer<HdrConstants>			hdrConstants;
			crossplatform::ConstantBuffer<ImageConstants>		imageConstants;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif