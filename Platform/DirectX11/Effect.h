#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include <string>
#include <map>
#include <d3d11.h>

#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Effect:public simul::crossplatform::Effect
		{
		public:
			Effect(void *device,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			Effect();
			~Effect();
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			void SetTexture(const char *name,void *tex);
			void SetTexture(const char *name,crossplatform::Texture &t);
		};
	}
}
