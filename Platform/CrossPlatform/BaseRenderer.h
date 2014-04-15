#ifndef SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#define SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#include "Simul/Math/Matrix4x4.h"

namespace simul
{
	namespace crossplatform
	{
		//! A simple struct encapsulating a view and a projection matrix.
		struct ViewStruct
		{
			int view_id;
			simul::math::Matrix4x4 view;
			simul::math::Matrix4x4 proj;
		};
		//! The base class for renderers. Placeholder for now.
		class BaseRenderer
		{
		};
	}
}
#endif