#pragma once

#include <map>
#include "D3dx11effect.h"
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
			void Apply(crossplatform::DeviceContext &) const;
			ID3DX11Effect *effect;
		};
	}
}