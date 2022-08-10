#pragma once

#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include <functional>
#include <unordered_map>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#endif

namespace platform
{
	namespace crossplatform
	{
		class CameraOutputInterface;
		/// A class that faces the raw API and implements PlatformRendererInterface
		/// in order to translate to the platform-independent renderer.
		class SIMUL_CROSSPLATFORM_EXPORT RenderDelegater
			:public PlatformRendererInterface
		{
			std::unordered_map<int,crossplatform::RenderDelegate> renderDelegate;
			std::unordered_map<int,int2> viewSize;
			std::vector<crossplatform::StartupDeviceDelegate> startupDeviceDelegates;
			std::vector<crossplatform::ShutdownDeviceDelegate> shutdownDeviceDelegates;
			int last_view_id;
			platform::crossplatform::RenderPlatform *renderPlatform;
		public:
			RenderDelegater(crossplatform::RenderPlatform *r=nullptr);
			~RenderDelegater();
			virtual void SetRenderPlatform(crossplatform::RenderPlatform *r);
			virtual int		AddView						();
			virtual void	RemoveView					(int);
			virtual void	ResizeView					(int view_id,int w,int h);
			virtual void	Render						(int,void* context,void* rendertarget,int w,int h,long long f, void* context_allocator = nullptr);
			virtual void	OnLostDevice				();
			void			SetRenderDelegate			(int view_id,crossplatform::RenderDelegate d);
			void			RegisterShutdownDelegate	(crossplatform::ShutdownDeviceDelegate d);
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif