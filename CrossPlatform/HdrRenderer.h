#ifndef SIMUL_CROSSPLATFORM_HDRRENDERER_H
#define SIMUL_CROSSPLATFORM_HDRRENDERER_H
#include "Platform/Core/PropertyMacros.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Shaders/hdr_constants.sl"
#include "Platform/CrossPlatform/Shaders/image_constants.sl"
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Effect.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT HdrRenderer
		{
		public:
			HdrRenderer();
			virtual ~HdrRenderer();
			void SetBufferSize(int w,int h);
			//! Platform-dependent function called when initializing the HDR renderer.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			//! Platform-dependent function called when uninitializing the HDR renderer.
			void InvalidateDeviceObjects();
			//! Render: write the given texture to screen using the HDR rendering shaders
			void Render(GraphicsDeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma,float offsetX);
			void Render(GraphicsDeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma);
			void RenderInfraRed(GraphicsDeviceContext &deviceContext,crossplatform::Texture *texture,vec3 infrared_integration_factors,float Exposure,float Gamma);
			void RenderWithOculusCorrection(GraphicsDeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma,float offsetX);
			//! Draw the debug textures
			void RenderDebug(GraphicsDeviceContext &deviceContext, int x0, int y0, int w, int h);
			void RecompileShaders();
		protected:
			crossplatform::RenderPlatform		*renderPlatform;
			int Width,Height;
			//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
			crossplatform::Effect*				hdr_effect;
			crossplatform::EffectTechnique*		exposureGammaTechnique;
			crossplatform::EffectPass*			exposureGammaMainPass;
			crossplatform::EffectPass*			exposureGammaMSAAPass;
			crossplatform::EffectTechnique*		warpExposureGamma;
			
			crossplatform::Effect*				m_pGaussianEffect;
			crossplatform::EffectTechnique*		gaussianRowTechnique;
			crossplatform::EffectTechnique*		gaussianColTechnique;
			crossplatform::ShaderResource hdr_effect_imageTexture;
			crossplatform::ShaderResource hdr_effect_imageTextureMS;
			crossplatform::ConstantBuffer<HdrConstants>			hdrConstants;
			crossplatform::ConstantBuffer<ImageConstants>		imageConstants;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif