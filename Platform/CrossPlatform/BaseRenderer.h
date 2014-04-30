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
		struct MixedResolutionStruct
		{
			/// The ratios that we multiply low-res texture coordinates by to get the hi-res equivalent.
			/// These will be close to one, but not exact unless the hi-res buffer is an exact multiple of the low-res one.
			float xratio,yratio;
		};
		//! The base class for renderers. Placeholder for now.
		class BaseRenderer
		{
		//protected:
		//	virtual scene::RenderPlatform *GetRenderPlatform()=0;
		};
	}
}
#endif