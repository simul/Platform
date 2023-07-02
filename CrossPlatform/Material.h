#pragma once

#include <map>
#include "Export.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Effect.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace platform
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
			Channel<vec3> emissive;
			Channel<vec3> normal;
			Channel<float> roughness;
			Channel<float> metal;
			Channel<float> ambientOcclusion;
		protected:
			crossplatform::Effect *effect=nullptr;
			std::string name;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif