#pragma once
#include "SimulDirectXHeader.h"
#include "Simul/Platform/CrossPlatform/MixedResolutionView.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/mixed_resolution_constants.sl"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Camera/Camera.h"
#include <set>

namespace simul
{
	namespace dx11
	{
		///
		enum ViewType
		{
			MAIN_3D_VIEW
			,OCULUS_VR
		};
		//! A class that encapsulates the generated mixed-resolution depth textures, and (optionally) a framebuffer with colour and depth.
		//! One instance of MixedResolutionView will be created and maintained for each live 3D view.
		struct SIMUL_DIRECTX11_EXPORT MixedResolutionView:public crossplatform::MixedResolutionView
		{
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

			/// Sets a resolution.
			///
			/// \param	w	The width.
			/// \param	h	The height.
			void SetResolution(int w,int h);

			/// Sets external framebuffer.
			///
			/// \param	ext	Whether to use an external framebuffer.
			void SetExternalFramebuffer(bool ext);

			/// If an external depth buffer is used, pass it here, wrappered in a
			/// simul::crossplatform::Texture.
			///
			/// \param [in,out]	tex	If non-null, the tex.
			void SetExternalDepthTexture(crossplatform::Texture *tex,crossplatform::Viewport v);

			/// Resolve framebuffer.
			///
			/// \param [in,out]	deviceContext	Context for the device.
			void ResolveFramebuffer(crossplatform::DeviceContext &deviceContext);

			/// Debugging onscreen info:
			///
			/// \param [in,out]	deviceContext	Context for the device.
			/// \param	x0					 	The x coordinate 0.
			/// \param	y0					 	The y coordinate 0.
			/// \param	dx					 	The dx.
			/// \param	dy					 	The dy.
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy);

			/// Gets resolved header buffer.
			///
			/// \return	null if it fails, else the resolved header buffer.
			ID3D11ShaderResourceView *MixedResolutionView::GetResolvedHDRBuffer();

			/// Gets the framebuffer.
			///
			/// \return	null if it fails, else the framebuffer.
			crossplatform::BaseFramebuffer			*GetFramebuffer()
			{
				return &hdrFramebuffer;
			}

			/// Gets hi resource depth texture.
			///
			/// \return	null if it fails, else the hi resource depth texture.
			crossplatform::Texture					*GetHiResDepthTexture()
			{
				return &hiResDepthTexture;
			}

			/// Gets low resource depth texture.
			///
			/// \return	null if it fails, else the low resource depth texture.
			crossplatform::Texture					*GetLowResDepthTexture()
			{
				return &lowResDepthTexture;
			}

			/// Gets low resource scratch texture.
			///
			/// \return	null if it fails, else the low resource scratch texture.
			crossplatform::Texture					*GetLowResScratchTexture()
			{
				return &lowResScratch;
			}
			crossplatform::Texture					*GetHiResScratchTexture()
			{
				return &hiResScratch;
			}
			/// Type of the view.
			ViewType						viewType;

			/// private:
			///      A framebuffer with depth.
			simul::dx11::Framebuffer		hdrFramebuffer;
			/// The depth from the HDR framebuffer can be resolved into this texture:
			simul::dx11::Texture			hiResDepthTexture;
			/// The low  depth texture.
			simul::dx11::Texture			lowResDepthTexture;
			/// The low-res scratch.
			simul::dx11::Texture			lowResScratch;
			/// The high-res scratch.
			simul::dx11::Texture			hiResScratch;
			/// The resolved texture.
			simul::dx11::Texture			resolvedTexture;
			/// The render platform.
			crossplatform::RenderPlatform	*renderPlatform;
			/// The camera.
			const simul::camera::CameraOutputInterface	*camera;
		public:
			/// Width of the screen.
			int								ScreenWidth;
			/// Height of the screen.
			int								ScreenHeight;
			/// true to use external framebuffer.
			bool							useExternalFramebuffer;
			/// The external depth texture.
			crossplatform::Texture			*externalDepthTexture;
			crossplatform::Viewport			depthTextureViewport;
		};
		/// A class to render mixed-resolution depth buffers.
		class SIMUL_DIRECTX11_EXPORT MixedResolutionRenderer
		{
		public:
			/// Default constructor.
			MixedResolutionRenderer();
			/// Destructor.
			~MixedResolutionRenderer();

			/// Restore device objects.
			///
			/// \param [in,out]	renderPlatform	If non-null, the render platform.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void InvalidateDeviceObjects();
			/// Recompile shaders.
			void RecompileShaders();

			/// Downscale depth.
			///
			/// \param [in,out]	deviceContext   	Cross-platform deviceContext.
			/// \param [in,out]	view				The view.
			/// \param	s							The downscale factor.
			/// \param	depthToLinFadeDistParams	Options for controlling the depth to linear fade distance conversion.
			void DownscaleDepth(crossplatform::DeviceContext &deviceContext,MixedResolutionView *view,int s,vec4 depthToLinFadeDistParams
								,bool includeLowResDepth);
		protected:
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
			/// The depth forward effect.
			crossplatform::Effect						*depthForwardEffect;
			/// The depth reverse effect.
			crossplatform::Effect						*depthReverseEffect;
			/// The constant buffer for the shader effect.
			crossplatform::ConstantBuffer<MixedResolutionConstants>	mixedResolutionConstants;
		};

		/// A class to store a set of MixedResolutionView objects, one per view id.
		class SIMUL_DIRECTX11_EXPORT MixedResolutionViewManager
		{
		public:
			/// Default constructor.
			MixedResolutionViewManager():
				last_created_view_id(-1)
				,renderPlatform(NULL)
			{}

			/// Restore the device objects.
			///
			/// \param [in,out]	renderPlatform	If non-null, the render platform.
			void							RestoreDeviceObjects	(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void							InvalidateDeviceObjects	();
			/// Recompile the shaders.
			void							RecompileShaders		();

			/// Downscale depth.
			///
			/// \param [in,out]	deviceContext	Cross-platform deviceContext.
			/// \param	s					 	The downscale factor.
			/// \param	max_dist_metres		 	The maximum distance in metres.
			void							DownscaleDepth			(crossplatform::DeviceContext &deviceContext,int s,float max_dist_metres,bool includeLowResDepth);

			/// Gets a view.
			///
			/// \param	view_id	Identifier for the view.
			///
			/// \return	null if it fails, else the view.
			MixedResolutionView				*GetView				(int view_id);

			/// Gets the views.
			///
			/// \return	null if it fails, else the views.
			std::set<MixedResolutionView*>	GetViews				();

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
			typedef std::map<int,MixedResolutionView*>	ViewMap;
			ViewMap							views;
			/// Identifier for the last created view.
			int								last_created_view_id;
			/// The mixed resolution renderer.
			MixedResolutionRenderer			mixedResolutionRenderer;
		};
	}
}