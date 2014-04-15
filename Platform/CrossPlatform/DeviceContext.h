#ifndef SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#define SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H

namespace simul
{
	namespace crossplatform
	{
		//! The base class for Device contexts. The actual context pointer is only applicable in DirectX - in OpenGL, it will be null.
		//! The DeviceContext also carries a pointer to the platform-specific RenderPlatform.
		struct DeviceContext
		{
			void *platform_context;
		};
	}
}
#endif