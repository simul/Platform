#ifndef BASE_FRAMEBUFFER_H
#define BASE_FRAMEBUFFER_H
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"

namespace simul
{
	namespace crossplatform
	{
		class Texture;
		struct DeviceContext;
		class RenderPlatform;
		//! A base class for API-dependent framebuffer classes.
		SIMUL_CROSSPLATFORM_EXPORT_CLASS BaseFramebuffer
		{
		public:
			BaseFramebuffer(int w=0,int h=0);
			virtual ~BaseFramebuffer(){};
			//! Call this when the API-dependent device has been created.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r)=0;
			//! Call this when the API-dependent device has been lost or is shutting down.
			virtual void InvalidateDeviceObjects()=0;
			/// Return true if the API-dependent objects have been updated to match the properties.
			virtual bool IsValid() const=0;
			//! Call this if needed (not usually) to ensure that the buffers are created.
			virtual bool CreateBuffers()=0;
			//! Activate the framebuffer and set the viewport- must be followed after rendering by a call to \ref Deactivate().
			virtual void ActivateViewport(crossplatform::DeviceContext &,float viewportX, float viewportY, float viewportW, float viewportH)=0;
			//! Activate the framebuffer - must be followed after rendering by a call to \ref Deactivate().
			virtual void Activate(crossplatform::DeviceContext &)=0;
			//! Activate the colour part of this framebuffer, without depth - must be followed after rendering by a call to \ref Deactivate().
			virtual void ActivateColour(crossplatform::DeviceContext &,const float viewportXYWH[4])=0;
			virtual void ActivateDepth(crossplatform::DeviceContext &)=0;
			//! Return true if the framebuffer's depth buffer has been activated and not yet deactivated.
			virtual bool IsDepthActive() const;
			//! Return true if the framebuffer's colour buffer has been activated and not yet deactivated.
			virtual bool IsColourActive() const;
			//! Deactivate the framebuffer - must be preceded a call to \ref Activate().
			virtual void Deactivate(DeviceContext &)=0;
			//! Deactivate only the depth buffer, so it can be used as a texture for rendering to the colour buffer.
			virtual void DeactivateDepth(DeviceContext &){}
			//! Set the API-dependent colour buffer format for this framebuffer. Across all API's, setting 0 means no rendering to colour.
			virtual void SetFormat(PixelFormat)=0;
			//! Set the API-dependent colour depth format for this framebuffer. Across all API's, setting 0 means no rendering to depth.
			virtual void SetDepthFormat(PixelFormat)=0;
			//! Clear the colour and depth buffers if present.
			virtual void Clear(crossplatform::DeviceContext &context,float R,float G,float B,float A,float depth,int mask=0)=0;
			//! Set the size of the framebuffer in pixel height and width.
			virtual void ClearColour(crossplatform::DeviceContext &context,float,float,float,float)=0;
			virtual void SetWidthAndHeight(int w,int h)=0;
			//! Some hardware has fast RAM that's good for framebuffers.
			virtual void SetUseFastRAM(bool /*colour*/,bool /*depth*/){};
			virtual void SetAntialiasing(int s)=0;
			//! Get the API-dependent pointer or id for the colour buffer target.
			virtual void* GetColorTex()=0;
			virtual Texture *GetTexture()=0;
			virtual Texture *GetDepthTexture()=0;
			//! Copy from the rt to the given target memory. If not starting at the top of the texture (start_texel>0), the first byte written
			//! is at \em target, which is the address to copy the given chunk to, not the base address of the whole in-memory texture.
			virtual void CopyToMemory(crossplatform::DeviceContext &context,void *target,int start_texel=0,int texels=0)=0;
			// Get the dimensions of the specified texture - temporary, do not use.
			virtual void GetTextureDimensions(const void* /*tex*/, unsigned int& /*widthOut*/, unsigned int& /*heightOut*/) const {}
		//protected:
			//! The size of the buffer
			int Width,Height;
			int numAntialiasingSamples;
			bool depth_active, colour_active;
			crossplatform::RenderPlatform *renderPlatform;
		};
		struct SIMUL_CROSSPLATFORM_EXPORT TwoResFramebuffer
		{
			TwoResFramebuffer();
			crossplatform::BaseFramebuffer *GetLowResFarFramebuffer()
			{
				return lowResFarFramebufferDx11;
			}
			crossplatform::BaseFramebuffer *GetLowResNearFramebuffer()
			{
				return lowResNearFramebufferDx11;
			}
			crossplatform::BaseFramebuffer *GetHiResFarFramebuffer()
			{
				return hiResFarFramebufferDx11;
			}
			crossplatform::BaseFramebuffer *GetHiResNearFramebuffer()
			{
				return hiResNearFramebufferDx11;
			}
			crossplatform::Texture *GetLossTexture();
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			virtual void InvalidateDeviceObjects();
			virtual void SetDimensions(int w,int h,int downscale,int hiResDownscale);
			virtual void GetDimensions(int &w,int &h,int &downscale,int &hiResDownscale);
			/// Activate BOTH Hi-resolution framebuffers - far in target 0, near in target 1. Must be followed by DeactivateHiRes after rendering.
			virtual void ActivateHiRes(crossplatform::DeviceContext &);
			/// Deactivate both hi-res framebuffers.
			virtual void DeactivateHiRes(crossplatform::DeviceContext &);
			/// Activate BOTH low-resolution framebuffers - far in target 0, near in target 1. Must be followed by DeactivatelLowRes after rendering.
			virtual void ActivateLowRes(crossplatform::DeviceContext &);
			/// Deactivate both low-res framebuffers.
			virtual void DeactivateLowRes(crossplatform::DeviceContext &);
			/// Deactivate the depth buffer
			virtual void DeactivateDepth(crossplatform::DeviceContext &);
		protected:
			crossplatform::RenderPlatform *renderPlatform;
			crossplatform::Texture *lossTexture;
			int HiResDownscale;
			int Width,Height,Downscale;
			crossplatform::BaseFramebuffer	*lowResFarFramebufferDx11;
			crossplatform::BaseFramebuffer	*lowResNearFramebufferDx11;
			crossplatform::BaseFramebuffer	*hiResFarFramebufferDx11;
			crossplatform::BaseFramebuffer	*hiResNearFramebufferDx11;
		};
	}
}
#endif