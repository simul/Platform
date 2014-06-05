#pragma once

#include <map>

#include "Export.h"
#include "Simul/Platform/CrossPlatform/Material.h"
struct ID3DX11Effect;
namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Material:public crossplatform::Material
		{
		public:
			Material();
			virtual ~Material();
			void Apply(crossplatform::DeviceContext &) const;
			ID3DX11Effect *effect;
		};
	}
}