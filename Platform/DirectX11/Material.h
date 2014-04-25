#pragma once

#include <map>
#include <d3dx11effect.h>
#include "Export.h"
#include "Simul/Scene/Material.h"

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Material:public scene::Material
		{
		public:
			Material();
			virtual ~Material();
			void Apply(void *context) const;
			ID3DX11Effect *effect;
		};
	}
}