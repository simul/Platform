#ifndef TWORESFRAMEBUFFER_H
#define TWORESFRAMEBUFFER_H
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Geometry/Orientation.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		class Texture;
		struct DeviceContext;
		class RenderPlatform;
		struct Viewport;
		struct ViewStruct;
		//! A framebuffer class for mixed-resolution rendering.
		class SIMUL_CROSSPLATFORM_EXPORT TwoResFramebuffer
		{
		public:
			TwoResFramebuffer();
			crossplatform::BaseFramebuffer *GetLowResFramebuffer(int index)
			{
				return lowResFramebuffers[index];
			}
			crossplatform::Texture *GetLossTexture();
			crossplatform::Texture *GetVolumeTexture(int num);
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			virtual void InvalidateDeviceObjects();
			virtual void SetDimensions(int w,int h,int downscale);
			virtual void GetDimensions(int &w,int &h,int &downscale);
			/// Activate BOTH low-resolution framebuffers - far in target 0, near in target 1. Must be followed by DeactivatelLowRes after rendering.
			virtual void ActivateLowRes(crossplatform::DeviceContext &);
			/// Deactivate both low-res framebuffers.
			virtual void DeactivateLowRes(crossplatform::DeviceContext &);
			/// Deactivate the depth buffer
			virtual void DeactivateDepth(crossplatform::DeviceContext &);
			virtual void ActivateVolume(crossplatform::DeviceContext &,int num);
			virtual void DeactivateVolume(crossplatform::DeviceContext &);

			/// Debugging onscreen info:
			///
			/// \param [in,out]	deviceContext	Context for the device.
			/// \param	depthTexture			The main depth texture.
			/// \param	viewport			 	The viewport in use for the depth texture.
			/// \param	x0					 	The left edge of area to use for the debug display.
			/// \param	y0					 	The top of this debug display.
			/// \param	dx					 	The width of the display.
			/// \param	dy					 	The height of the display.
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,const crossplatform::Viewport *viewport,int x0,int y0,int dx,int dy);

			/// Update the pixel offset for the specified view.
			void UpdatePixelOffset(const crossplatform::ViewStruct &viewStruct,int scale);
			/// Returns	the low-res depth texture.
			crossplatform::Texture				*GetLowResDepthTexture(int idx=-1);
			crossplatform::PixelFormat GetDepthFormat() const;
			void SetDepthFormat(crossplatform::PixelFormat p);
			int									final_octave;
			vec2								GetPixelOffset() const
			{
				return pixelOffset;
			}
		protected:
			int									Width,Height,Downscale;
			int									volume_num;
			crossplatform::PixelFormat			depthFormat;
			simul::geometry::SimulOrientation	view_o;
			crossplatform::RenderPlatform		*renderPlatform;
			crossplatform::Texture				*nearFarTextures[4];
			crossplatform::Texture				*lossTexture;
			crossplatform::Texture				*volumeTextures[2];
			crossplatform::BaseFramebuffer		*lowResFramebuffers[3];
			/// Offset in pixels from top-left of the low-res view to top-left of the full-res.
			vec2								pixelOffset;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif