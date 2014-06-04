#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include <map>
#include <string>

namespace simul
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT EffectTechnique
		{
		public:
			void *platform_technique;
			EffectTechnique();
		};
		class SIMUL_CROSSPLATFORM_EXPORT Effect
		{
		protected:
			std::map<std::string,crossplatform::EffectTechnique*> techniques;
		public:
			void *platform_effect;
			Effect();
			virtual ~Effect();
			virtual EffectTechnique *GetTechniqueByName(const char *name)=0;
			virtual void SetTexture(const char *name,void *tex)=0;
		};
	}
}

