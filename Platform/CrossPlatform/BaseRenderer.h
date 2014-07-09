#ifndef SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#define SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#include "Simul/Math/Matrix4x4.h"
#include "SL/CppSl.hs"

namespace simul
{
	namespace crossplatform
	{
/// How to interpret the depth texture.
		enum DepthTextureStyle
		{
			/// Depth textures are interpreted as representing the z-output of the projection matrix transformation.
			PROJECTION
			/// Depth textures are interpreted as representing a linear distance in the z-direction from the near clipping plane.
			,DISTANCE_FROM_NEAR_PLANE
		};
		/// A simple struct encapsulating a view and a projection matrix.
		struct ViewStruct
		{
			int view_id;							///< An id unique to each rendered view, but persistent across frames.
			math::Matrix4x4 view;					///< The view matrix. If considered as row-major, position information is in the 4th row.
			math::Matrix4x4 proj;					///< The projection matrix, row-major.
			DepthTextureStyle depthTextureStyle;	///< How to interpret any depth texture passed from outside.
		};
		struct MixedResolutionStruct
		{
			inline MixedResolutionStruct(int W,int H,int s)
			{
				this->W=W;
				this->H=H;
				w=(W+s-1)/s;
				h=(H+s-1)/s;
				downscale=s;
			}
			int W,H;
			int w,h;
			int downscale;
			// The ratios that we multiply low-res texture coordinates by to get the hi-res equivalent.
			// These will be close to one, but not exact unless the hi-res buffer is an exact multiple of the low-res one.
			//float xratio,yratio;GetTransformLowResToHiRes();
			inline vec4 GetTransformLowResToHiRes() const
			{
				return vec4(0.f,0.f,(float)(w*downscale)/(float)W,(float)(h*downscale)/(float)H);
			}
			inline vec4 GetTransformHiResToLowRes() const
			{
				return vec4(0.f,0.f,(float)W/(float)(w*downscale),(float)H/(float)(h*downscale));
			}
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