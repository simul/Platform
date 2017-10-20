#pragma once

#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Clouds/TrueSkyRenderer.h"
#include <functional>
#include <unordered_map>

#pragma warning(push)
#pragma warning(disable:4251)

namespace simul
{
	namespace crossplatform
	{
		class CameraOutputInterface;
		class BaseOpticsRenderer;
		/// A class that faces the raw API and implements PlatformRendererInterface
		/// in order to translate to the platform-independent renderer.
		class SIMUL_CROSSPLATFORM_EXPORT RenderDelegater
			:public crossplatform::PlatformRendererInterface
		{
			std::unordered_map<int,crossplatform::RenderDelegate> renderDelegate;
			std::unordered_map<int,int2> viewSize;
			std::vector<crossplatform::StartupDeviceDelegate> startupDeviceDelegates;
			std::vector<crossplatform::ShutdownDeviceDelegate> shutdownDeviceDelegates;
			void *device;
			int last_view_id;
		public:
			RenderDelegater(crossplatform::RenderPlatform *r,simul::base::MemoryInterface *m);
			~RenderDelegater();
			simul::crossplatform::RenderPlatform *renderPlatform;
			virtual void	OnCreateDevice				(void* pd3dDevice);
			virtual int		AddView						();
			virtual void	RemoveView					(int);
			virtual void	ResizeView					(int view_id,int w,int h);
			virtual void	Render						(int,void* context,void* rendertarget,int w,int h);
			virtual void	OnLostDevice				();
			void			SetRenderDelegate			(int view_id,crossplatform::RenderDelegate d);
			void			RegisterStartupDelegate		(crossplatform::StartupDeviceDelegate d);
			void			RegisterShutdownDelegate	(crossplatform::ShutdownDeviceDelegate d);
		};
	}
}
#pragma warning(pop)