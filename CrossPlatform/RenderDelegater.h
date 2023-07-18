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
		/// This represents an interface that faces the raw API.
		/// The implementing class should keep a list of integer view id's
		class RenderDelegaterInterface
		{
		protected:
			virtual ~RenderDelegaterInterface() = default;

		public:
			//! Add a view. This tells the renderer to create any internal stuff it needs to handle a viewport, so that it is ready when Render() is called. It returns an identifier for that view.
			virtual int					AddView()=0;
			//! Remove the view. This might not have an immediate effect internally, but is a courtesy to the interface.
			virtual void				RemoveView(int)=0;
			//! For a view that has already been created, this ensures that it has the requested size and format.
			virtual void				ResizeView(int view_id,int w,int h)=0;
			//! Render the specified view. It's up to the renderer to decide what that means. The renderTexture is required because many API's don't allow querying of the current state.
			//! It will be assumed for simplicity that the viewport should be restored to the entire size of the renderTexture.
			virtual void				Render(int view_id,void* pContext,void* renderTexture,int w,int h,long long frame,void* context_allocator=nullptr)=0;
			virtual void				SetRenderDelegate(int /*view_id*/,crossplatform::RenderDelegate /*d*/){}
		};
		/// A class that faces the raw API and implements RenderDelegaterInterface
		/// in order to translate to the platform-independent renderer.
		class SIMUL_CROSSPLATFORM_EXPORT RenderDelegater
			:public RenderDelegaterInterface
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
			virtual int		AddView						() override;
			virtual void	RemoveView					(int) override;
			virtual void	ResizeView					(int view_id,int w,int h) override;
			virtual void	Render						(int,void* context,void* rendertarget,int w,int h,long long f, void* context_allocator = nullptr) override;
			virtual void	OnLostDevice				() ;
			void			SetRenderDelegate			(int view_id,crossplatform::RenderDelegate d) override;
			void			RegisterShutdownDelegate	(crossplatform::ShutdownDeviceDelegate d) ;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif