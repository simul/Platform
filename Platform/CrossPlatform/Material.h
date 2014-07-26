#pragma once

#include <map>
#include "Export.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/Effect.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		class Texture;
		class Effect;
		struct DeviceContext;
		struct PhysicalLightRenderData
		{
			Texture *diffuseCubemap;
			vec3 dirToLight;
			vec3 lightColour;
		};
		// Cache for material
		class SIMUL_CROSSPLATFORM_EXPORT Material
		{
		public:
			Material();
			virtual ~Material();
			void SetEffect(crossplatform::Effect *e);
			crossplatform::Effect *GetEffect();
			void InvalidateDeviceObjects();
			bool HasTexture() const { return mDiffuse.mTextureName != 0; }
			virtual void Apply(crossplatform::DeviceContext &,PhysicalLightRenderData &)=0;
			struct ColorChannel
			{
				ColorChannel() : mTextureName(0)
				{
					mColor[0] = 0.0f;
					mColor[1] = 0.0f;
					mColor[2] = 0.0f;
					mColor[3] = 1.0f;
				}
				crossplatform::Texture *mTextureName;
				float mColor[4];
			};
			ColorChannel mEmissive;
			ColorChannel mAmbient;
			ColorChannel mDiffuse;
			ColorChannel mSpecular;
			float mShininess;
		protected:
			crossplatform::Effect *effect;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif