#ifndef SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#define SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#include "Platform/Math/Matrix4x4.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/Export.h"


#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		// NVCHANGE_BEGIN: TrueSky + VR MultiRes Support
		struct MultiResViewport
		{
			vec2 TopLeft;
			vec2 Size;
		};
		struct MultiResScissorRect
		{
			vec2 Min;
			vec2 Max;
		};
		struct MultiResConstants
		{
			vec2 multiResToLinearSplitsX;
			vec2 multiResToLinearSplitsY;
			vec2 multiResToLinearX0;
			vec2 multiResToLinearX1;
			vec2 multiResToLinearX2;
			vec2 multiResToLinearY0;
			vec2 multiResToLinearY1;
			vec2 multiResToLinearY2;

			vec2 linearToMultiResSplitsX;
			vec2 linearToMultiResSplitsY;
			vec2 linearToMultiResX0;
			vec2 linearToMultiResX1;
			vec2 linearToMultiResX2;
			vec2 linearToMultiResY0;
			vec2 linearToMultiResY1;
			vec2 linearToMultiResY2;

			MultiResViewport multiResViewports[9];
			MultiResScissorRect multiResScissorRects[9];
		};
		// NVCHANGE_END: TrueSky + VR MultiRes Support
		/// A simple struct encapsulating a view and a projection matrix.
		struct SIMUL_CROSSPLATFORM_EXPORT ViewStruct
		{
			ViewStruct():
				initialized(false)
				,view_id(0)
				,depthTextureStyle(DepthTextureStyle::PROJECTION)
			{}
			ViewStruct(const ViewStruct &vs) = default;

			bool initialized;
			int view_id;			///< An id unique to each rendered view, but persistent across frames.
			math::Matrix4x4 model;	///< The model matrix
			math::Matrix4x4 view;	///< The view matrix. If considered as row-major, position information is in the 4th row.
			math::Matrix4x4 proj;	///< The projection matrix, row-major.
			// derived matrices
			math::Matrix4x4 invViewProj;
			math::Matrix4x4 invView;
			math::Matrix4x4 viewProj;
			// derived vectors.
			vec3 cam_pos,view_dir,up;
			simul::crossplatform::Frustum frustum;	///< The viewing frustum, calculated from the proj matrix and stored for convenience using simul::crossplatform::GetFrustumFromProjectionMatrix.
			DepthTextureStyle depthTextureStyle;	///< How to interpret any depth texture passed from outside.
			//! MUST be called whenever view or proj change.
			void Init();
		};
		/// Values that represent what pass to render, be it the near pass, the far, or both: far to render target 0, near to render target 1.
		enum NearFarPass
		{
			FAR_PASS
			,NEAR_PASS
			,BOTH_PASSES
			,EIGHT_PASSES
		};
		/// A structure to translate between buffers of different resolutions.
		struct MixedResolutionStruct
		{
			inline MixedResolutionStruct(int W,int H,int s,vec2 offs)
			{
				this->W=W;
				this->H=H;
				if (s > 1)
				{
					w = (W + s - 1) / s + 1;
					h = (H + s - 1) / s + 1;
					pixelOffset = offs;
				}
				else
				{
					w = W;
					h = H;
					pixelOffset = offs;
				}
				downscale=s;
			}
			int W,H;
			int w,h;
			int downscale;
			/// This is the offset in higher-resolution pixels from the top-left of the lower-res buffer to the top-left of the higher-res buffer.
			vec2 pixelOffset;
			// The ratios that we multiply low-res texture coordinates by to get the hi-res equivalent.
			// These will be close to one, but not exact unless the hi-res buffer is an exact multiple of the low-res one.
			//float xratio,yratio;GetTransformLowResToHiRes();
			inline vec4 GetTransformLowResToHiRes() const
			{
				float uu=(downscale>1)?0.5f:0.0f;
				float A=(float)(w*downscale)/(float)W;
				float B=(float)(h*downscale)/(float)H;
				float X=(float)(-pixelOffset.x)/(float)W-uu*(1.f/w)*A;
				float Y=(float)(-pixelOffset.y)/(float)H-uu*(1.f/h)*B;
				return vec4(X,Y,A,B);
			}
			// X = -px/W - uu/w*a
			// U = X + A u
			// =>
			//		u= (U-X)/A
			//			=- X/A+(1/A)U
			//			= (-X/A) + (1/A) U
			//			= x + a U

			// where x=-X/A and a=1/A
			inline vec4 GetTransformHiResToLowRes() const
			{
				if(downscale<=1)
					return vec4(0,0,1.0f,1.0f);
				float a=(float)W/(float)(w*downscale);
				float b=(float)H/(float)(h*downscale);
				static float uu=0.25f;
				float x=(float)(pixelOffset.x)*a/(float)(W)+uu*(1.f/w);
				float y=(float)(pixelOffset.y)*b/(float)(H)+uu*(1.f/h);
				return vec4(x,y,a,b);//1.0f-1.0f/b
			}
		};
		//! The base class for renderers. Placeholder for now.
		class SIMUL_CROSSPLATFORM_EXPORT BaseRenderer
		{
		//protected:
		//	virtual scene::RenderPlatform *GetRenderPlatform()=0;
		public:
			virtual ~BaseRenderer(){}
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif