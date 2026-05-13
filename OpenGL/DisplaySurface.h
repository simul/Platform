#pragma once

#include "Export.h"
#include "Platform/CrossPlatform/DisplaySurface.h"

typedef struct GLFWwindow GLFWwindow;
namespace platform
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT DisplaySurface: public crossplatform::DisplaySurface
		{
		public:
			DisplaySurface(int view_id);
			~DisplaySurface();
			void RestoreDeviceObjects(cp_hwnd handle, crossplatform::RenderPlatform* renderPlatform, bool vsync, crossplatform::PixelFormat outFmt)override;
			void InvalidateDeviceObjects() override;
			void Render(platform::core::ReadWriteMutex *delegatorReadWriteMutex,long long frameNumber) override;
			void EndFrame() override;
		private:
			//! Will resize the swap chain only if needed
			void Resize();
			crossplatform::PixelFormat pixelFormat;

#ifdef _MSC_VER
			HDC             hDC;
			HGLRC           hRC;
#endif
			void InitSwapChain();
		};
	}
}
