#pragma once

#include <fbxsdk.h>
#include <map>
#include "Export.h"
#include "Simul/Scene/Material.h"

namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT Material:public scene::MaterialCache
		{
		public:
			Material();
			virtual ~Material();
			void Apply() const;
		};
	}
}