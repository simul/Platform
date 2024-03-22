#ifndef SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#define SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#include "Platform/Math/Matrix4x4.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/ViewStruct.h"
#include <stack>


#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace platform
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
		/// Values that represent what pass to render, be it the near pass, the far, or both: far to render target 0, near to render target 1.
		enum NearFarPass
		{
			FAR_PASS
			,NEAR_PASS
			,BOTH_PASSES
			,EIGHT_PASSES
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