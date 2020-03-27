#pragma once
#include "Platform/CrossPlatform/Export.h"
namespace simul
{
	namespace crossplatform
	{
		class Texture;
		class RenderPlatform;
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT DemoOverlay
		{
			Texture *texture1,*texture2;
			RenderPlatform *renderPlatform;
		public:
			DemoOverlay();
			~DemoOverlay();
			void RestoreDeviceObjects(RenderPlatform *renderPlatform);
			void InvalidateDeviceObjects();
			void Render(DeviceContext &deviceContext);
		};
	}
}