#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/Topology.h"
#include "Simul/Platform/CrossPlatform/TwoResFramebuffer.h"
#include "Simul/Platform/CrossPlatform/SL/mixed_resolution_constants.sl"
#include "Simul/Geometry/Orientation.h"
#include <set>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		class Texture;
		class Effect;
		struct Viewport;
		class RenderPlatform;
		///
		enum ViewType
		{
			MAIN_3D_VIEW
			,OCULUS_VR
		};
		//! A class that encapsulates the generated mixed-resolution depth textures, and (optionally) a framebuffer with colour and depth.
		//! One instance of MixedResolutionView will be created and maintained for each live 3D view.
		class SIMUL_CROSSPLATFORM_EXPORT MixedResolutionView
		{
		public:
			/// Default constructor.
			MixedResolutionView();
			/// Destructor.
			~MixedResolutionView();

			/// Restore device objects.
			///
			/// \param [in,out]	renderPlatform	If non-null, the render platform.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void InvalidateDeviceObjects();

			/// Gets screen width.
			///
			/// \return	The screen width.
			int GetScreenWidth() const;

			/// Gets screen height.
			///
			/// \return	The screen height.
			int GetScreenHeight() const;

			/// Sets the resolution.
			///
			/// \param	w	The width.
			/// \param	h	The height.
			void SetResolution(int w,int h);

			/// Sets external framebuffer.
			///
			/// \param	ext	Whether to use an external framebuffer.
			void SetExternalFramebuffer(bool ext);
			/// Gets resolved header buffer.
			///
			/// \return	null if it fails, else the resolved header buffer.
			crossplatform::Texture						*GetResolvedHDRBuffer();

			/// Gets the framebuffer.
			///
			/// \return	null if it fails, else the framebuffer.
			crossplatform::BaseFramebuffer				*GetFramebuffer()
			{
				return hdrFramebuffer;
			}
			const crossplatform::BaseFramebuffer		*GetFramebuffer() const
			{
				return hdrFramebuffer;
			}
			/// Type of view.
			crossplatform::ViewType						viewType;
			bool										vrDistortion;
		private:
			///      A framebuffer with depth.
			simul::crossplatform::BaseFramebuffer		*hdrFramebuffer;
			/// The resolved texture.
			simul::crossplatform::Texture				*resolvedTexture;
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
		public:
			/// Width of the screen.
			int								ScreenWidth;
			/// Height of the screen.
			int								ScreenHeight;
			int								last_framenumber;
		public:
			/// true to use external framebuffer.
			bool							useExternalFramebuffer;
		};

		/// A class to store a set of MixedResolutionView objects, one per view id.
		class SIMUL_CROSSPLATFORM_EXPORT MixedResolutionViewManager
		{
		public:
			/// Default constructor.
			MixedResolutionViewManager():
				last_created_view_id(-1)
				,renderPlatform(NULL)
			{}
			
			typedef std::map<int,MixedResolutionView*>	ViewMap;

			/// Restore the device objects.
			///
			/// \param [in,out]	renderPlatform	If non-null, the render platform.
			void							RestoreDeviceObjects	(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void							InvalidateDeviceObjects	();

			/// Gets a view.
			///
			/// \param	view_id	Identifier for the view.
			///
			/// \return	null if it fails, else the view.
			MixedResolutionView				*GetView				(int view_id);
			
			const MixedResolutionView		*GetView		(int view_id) const;

			/// Gets the views.
			///
			/// \return	the views.
			const ViewMap &MixedResolutionViewManager::GetViews() const;

			/// Adds a view.
			///
			/// \param	external_framebuffer	Whether to use an external framebuffer.
			///
			/// \return	An int view_id.
			int								AddView					(bool external_framebuffer);

			/// Removes the view.
			///
			/// \param	view_id	The id of the view to remove.
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