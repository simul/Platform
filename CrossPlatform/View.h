#pragma once
#include "Export.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Topology.h"

#include "Platform/Math/Orientation.h"
#include <set>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace crossplatform
	{
		class Texture;
		class Effect;
		class Framebuffer;
		struct Viewport;
		class RenderPlatform;
		///
		enum ViewType : int
		{
			MAIN_3D_VIEW
			,OCULUS_VR
		};
		//! A class that encapsulates the generated mixed-resolution depth textures, and (optionally) a framebuffer with colour and depth.
		//! One instance of View will be created and maintained for each live 3D view.
		class SIMUL_CROSSPLATFORM_EXPORT View
		{
		protected:
			static int last_class_id;
		private:
			static int static_class_id;
		public:
			/// Default constructor.
			View();
			/// Destructor.
			virtual ~View();

			virtual int GetClassId() const
			{
				return static_class_id;
			}
			static int GetStaticClassId()
			{
				return static_class_id;
			}
			//! Platform-dependent function called when initializing the view.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			//! Platform-dependent function called when uninitializing the view.
			void InvalidateDeviceObjects();

			/// Gets screen width.
			int GetScreenWidth() const;

			/// Gets screen height.
			int GetScreenHeight() const;

			/// Sets the resolution.
			void SetResolution(int w,int h);

			/// Sets external framebuffer.
			void SetExternalFramebuffer(bool ext);
			/// Gets resolved HDR buffer.
			crossplatform::Texture						*GetResolvedHDRBuffer();

			/// Type of view.
			crossplatform::ViewType						viewType;
			bool										vrDistortion;
		protected:
			/// A framebuffer with depth.
			platform::crossplatform::Framebuffer		*hdrFramebuffer;
			/// The resolved texture.
			platform::crossplatform::Texture				*resolvedTexture;
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
		public:
			/// Width of the screen.
			int											ScreenWidth;
			/// Height of the screen.
			int											ScreenHeight;
			long long									last_framenumber;
			/// True to use external framebuffer.
			bool										useExternalFramebuffer;
		};

		/// A class to store a set of View objects, one per view id.
		class SIMUL_CROSSPLATFORM_EXPORT ViewManager
		{
		public:
			/// Default constructor.
			ViewManager():
				renderPlatform(NULL)
				,last_created_view_id(-1)
			{}
			typedef std::map<int,View*>	ViewMap;
			//! Platform-dependent function called when initializing the view manager.
			void							RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			//! Platform-dependent function called when uninitializing the view manager.
			void							InvalidateDeviceObjects	();
			/// Gets a view.
			/// \return	null if it fails, else the view.
			View			*GetView		(int view_id);
			const View		*GetView		(int view_id) const;
			/// Delete old views
			void CleanUp(int current_framenumber,int max_age);
			/// Gets the views.
			const ViewMap &GetViews() const;
			/// Adds a view.
			/// \return	An int view_id.
			View*							AddView	(int);
			int								AddView	(int,View *v);
			/// Removes the view.
			void							RemoveView				(int view_id);
			/// Clears this object to its blank/initial state.
			void							Clear					();
		protected:
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
			ViewMap							views;
			/// Identifier for the last created view.
			int								last_created_view_id;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif