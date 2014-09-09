#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Geometry/Orientation.h"
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		class SIMUL_CROSSPLATFORM_EXPORT MixedResolutionView
		{
		public:
			MixedResolutionView();
			virtual ~MixedResolutionView();
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform)=0;
			virtual void InvalidateDeviceObjects()=0;
			crossplatform::PixelFormat GetDepthFormat() const;
			void SetDepthFormat(crossplatform::PixelFormat p);
			void UpdatePixelOffset(const crossplatform::ViewStruct &viewStruct,int scale);
		public:
			/// Offset in pixels from top-left of the low-res view to top-left of the full-res.
			vec2							pixelOffset;
			static vec2 LoResToHiResOffset(vec2 pixelOffset,int hiResScale);
			static vec2 WrapOffset(vec2 pixelOffset,int scale);
			/// Width of the screen.
			int								ScreenWidth;
			/// Height of the screen.
			int								ScreenHeight;
		protected:
			simul::geometry::SimulOrientation	view_o;
			crossplatform::PixelFormat depthFormat;
		};
	}
}
