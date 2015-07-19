#pragma once
#include "Simul/Platform/CrossPlatform/LensFlare.h"
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/optics_constants.sl"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"

#include "Simul/Platform/CrossPlatform/LensFlare.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
		struct ViewStruct;
	}
	namespace crossplatform
	{
		//! A base class for rendering optical effects such as lens flare.
		SIMUL_CROSSPLATFORM_EXPORT_CLASS BaseOpticsRenderer
		{
		public:
			BaseOpticsRenderer(simul::base::MemoryInterface *);
			virtual ~BaseOpticsRenderer();
			//! To be called when a rendering device has been initialized.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			static void EnsureEffectsAreBuilt(crossplatform::RenderPlatform *r);
			virtual void RecompileShaders();
			//! To be called when the rendering device is no longer valid.
			virtual void InvalidateDeviceObjects();
			//! Render the lens flares based on the given direction to the light, and its colour.
			virtual void RenderFlare(crossplatform::DeviceContext &devicContext,float exposure,const float *dir,const float *light);
		protected:
			float flare_magnitude,flare_angular_size;
			simul::crossplatform::LensFlare lensFlare;
			void SetOpticsConstants(OpticsConstants &c,const crossplatform::ViewStruct &viewStruct,const float *direction,const float *colour,float angularRadiusRadians);
			
		protected:
			crossplatform::RenderPlatform*			renderPlatform;
			crossplatform::Effect*					effect;
			crossplatform::EffectTechnique*			m_hTechniqueFlare;
			crossplatform::Texture*					flare_texture;
			std::vector<crossplatform::Texture*>	halo_textures;
		protected:
			std::string								FlareTexture;
			crossplatform::ConstantBuffer<OpticsConstants>			opticsConstants;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif