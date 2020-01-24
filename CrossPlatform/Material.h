#pragma once

#include <map>
#include "Export.h"
#include "Simul/Platform/Shaders/SL/CppSl.sl"
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
			Texture *specularCubemap;
			vec3 dirToLight;
			vec3 lightColour;
		};
		struct ColorChannelInitStruct
		{
			vec3 colour;
			const char *textureName;
		};
		struct MaterialInitStruct
		{
			ColorChannelInitStruct emissive;
			ColorChannelInitStruct albedo;
			ColorChannelInitStruct specular;
			float mShininess;
			const char *effectName;
		};
		// Cache for material
		class SIMUL_CROSSPLATFORM_EXPORT Material
		{
		public:
			Material() {}
			Material(const char *name);
			virtual ~Material();
			void SetEffect(crossplatform::Effect *e);
			crossplatform::Effect *GetEffect();
			void InvalidateDeviceObjects();
			void Apply(crossplatform::DeviceContext &,PhysicalLightRenderData &);
			template<typename T> struct Channel
			{
				Channel() : texture(nullptr)
				{
					memset((T*)&value, 0, sizeof(T));
				}
				crossplatform::Texture *texture;
				T value;
			};
			Channel<vec3> albedo;
			Channel<float> roughness;
			Channel<float> metal;
			Channel<float> ambientOcclusion;
		protected:
			crossplatform::Effect *effect;
			std::string name;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif