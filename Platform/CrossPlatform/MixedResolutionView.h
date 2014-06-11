#pragma once
#include "Export.h"
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
		};
	}
}
