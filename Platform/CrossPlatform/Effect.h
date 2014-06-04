#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include <string>
#include <map>
struct ID3DX11Effect;
struct ID3DX11EffectTechnique;
#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		class Texture;
		class SIMUL_CROSSPLATFORM_EXPORT EffectTechnique
		{
		public:
			void *platform_technique;
			inline ID3DX11EffectTechnique *asD3DX11EffectTechnique()
			{
				return (ID3DX11EffectTechnique*)platform_technique;
			}
		};
		class SIMUL_CROSSPLATFORM_EXPORT Effect
		{
		protected:
			typedef std::map<std::string,EffectTechnique *> TechniqueMap;
			TechniqueMap techniques;
		public:
			void *platform_effect;
			Effect();
			virtual ~Effect();
			inline ID3DX11Effect *asD3DX11Effect()
			{
				return (ID3DX11Effect*)platform_effect;
			}
			virtual EffectTechnique *GetTechniqueByName(const char *name)=0;
			virtual void SetTexture(const char *name,void *tex)=0;
			virtual void SetTexture(const char *name,Texture &t)=0;
		};
	}
}
