#pragma once

#include <map>
#include "Export.h"
#include "Simul/Platform/CrossPlatform/Material.h"

namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT Material:public crossplatform::Material
		{
		public:
			Material();
			virtual ~Material();
			void Apply(crossplatform::DeviceContext &,crossplatform::PhysicalLightRenderData &physicalLightRenderData) const;
		};
	}
}