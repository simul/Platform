#ifndef BASE_FRAMEBUFFER_H
#define BASE_FRAMEBUFFER_H
#include "Simul/Platform/CrossPlatform/Export.h"

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
			//! Call this when the API-dependent device has been created.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r)=0;
			//! Call this when the API-dependent device has been lost or is shutting down.
			virtual void InvalidateDeviceObjects()=0;
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
			virtual void Deactivate(void*)=0;
			//! Deactivate only the depth buffer, so it can be used as a texture for rendering to the colour buffer.
			virtual void DeactivateDepth(void*){}
			//! Set the API-dependent colour buffer format for this framebuffer. Across all API's, setting 0 means no rendering to colour.
			virtual void SetFormat(int)=0;
			//! Set the API-dependent colour depth format for this framebuffer. Across all API's, setting 0 means no rendering to depth.
			virtual void SetDepthFormat(int)=0;
			//! Clear the colour and depth buffers if present.
			virtual void Clear(void* context,float R,float G,float B,float A,float depth,int mask=0)=0;
			//! Set the size of the framebuffer in pixel height and width.
			virtual void ClearColour(void* context,float,float,float,float)=0;
			virtual void SetWidthAndHeight(int w,int h)=0;
			virtual void SetAntialiasing(int s)=0;
			//! Get the API-dependent pointer or id for the colour buffer target.
			virtual void* GetColorTex()=0;
			virtual Texture *GetTexture()=0;
			virtual Texture *GetDepthTexture()=0;
			//! Copy from the rt to the given target memory. If not starting at the top of the texture (start_texel>0), the first byte written
			//! is at \em target, which is the address to copy the given chunk to, not the base address of the whole in-memory texture.
			virtual void CopyToMemory(void *context,void *target,int start_texel=0,int texels=0)=0;
			// Get the dimensions of the specified texture - temporary, do not use.
			virtual void GetTextureDimensions(const void* /*tex*/, unsigned int& /*widthOut*/, unsigned int& /*heightOut*/) const {}
		//protected:
			//! The size of the buffer
			int Width,Height;
			int numAntialiasingSamples;
			bool depth_active, colour_active;
			crossplatform::RenderPlatform *renderPlatform;
		};
		struct TwoResFramebuffer
		{
			TwoResFramebuffer():renderPlatform(0){}
			virtual crossplatform::BaseFramebuffer *GetLowResFarFramebuffer()=0;
			virtual crossplatform::BaseFramebuffer *GetLowResNearFramebuffer()=0;
			virtual crossplatform::BaseFramebuffer *GetHiResFarFramebuffer()=0;
			virtual crossplatform::BaseFramebuffer *GetHiResNearFramebuffer()=0;
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *)=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual void SetDimensions(int w,int h,int downscale)=0;
			virtual void GetDimensions(int &w,int &h,int &downscale)=0;
		protected:
			crossplatform::RenderPlatform *renderPlatform;
		};
	}
}
#endif