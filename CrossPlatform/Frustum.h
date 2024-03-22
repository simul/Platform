#pragma once

#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"

namespace platform
{
	namespace crossplatform
	{
		/// How to interpret the depth texture.
		enum DepthTextureStyle
		{
			/// Depth textures are interpreted as representing the z-output of the projection matrix transformation.
			PROJECTION
			/// Depth textures are interpreted as representing a linear distance in the z-direction from the near clipping plane.
			,
			DISTANCE_FROM_NEAR_PLANE
		};
		/// A useful class to represent a view frustum.
		struct SIMUL_CROSSPLATFORM_EXPORT Frustum
		{
			tvector4<float> tanHalfFov; // xy= tangent of half-angle, zw=offset
			float nearZ, farZ;
			bool reverseDepth;
		};
	}
}