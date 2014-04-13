#pragma once

#include <map>
#include "Export.h"
#include "Simul/Scene/Material.h"

namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT Material:public scene::Material
		{
		public:
			Material();
			virtual ~Material();
			void Apply() const;
		};
	}
}