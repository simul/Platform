#pragma once
#include "Platform/CrossPlatform/Export.h"
namespace platform
{
	namespace crossplatform
	{
		class Texture;
		class RenderPlatform;
		struct GraphicsDeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT DemoOverlay
		{
			Texture *texture1,*texture2;
			RenderPlatform *renderPlatform;
		public:
			DemoOverlay();
			~DemoOverlay();
			void RestoreDeviceObjects(RenderPlatform *renderPlatform);
			void InvalidateDeviceObjects();
			void Render(GraphicsDeviceContext &deviceContext);
		};
	}
}