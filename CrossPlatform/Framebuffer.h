#ifndef BASE_FRAMEBUFFER_H
#define BASE_FRAMEBUFFER_H
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include <stack>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace crossplatform
	{
		class Texture;
		struct DeviceContext;
		class RenderPlatform;
		//! A base class for API-dependent framebuffer classes.
		class SIMUL_CROSSPLATFORM_EXPORT Framebuffer
		{
		public:
			Framebuffer(const char *n);
			virtual ~Framebuffer();
			//! Platform-dependent function called when initializing the framebuffer.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			//! Platform-dependent function called when uninitializing the framebuffer.
			virtual void InvalidateDeviceObjects();
			/// Return true if the API-dependent objects have been updated to match the properties.
			virtual bool IsValid() const;
			virtual void SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth);
			//! Call this if needed (not usually) to ensure that the buffers are created.
			virtual bool CreateBuffers();
			//! Activate the framebuffer - must be followed after rendering by a call to \ref Deactivate().
			virtual void Activate(crossplatform::GraphicsDeviceContext &)=0;
			virtual void ActivateDepth(crossplatform::GraphicsDeviceContext &)=0;
			//! Return true if the framebuffer's depth buffer has been activated and not yet deactivated.
			virtual bool IsDepthActive() const;
			//! Return true if the framebuffer's colour buffer has been activated and not yet deactivated.
			virtual bool IsColourActive() const;
			//! Deactivate the framebuffer - must be preceded a call to \ref Activate().
			virtual void Deactivate(GraphicsDeviceContext &)=0;
			//! Deactivate only the depth buffer, so it can be used as a texture for rendering to the colour buffer.
			virtual void DeactivateDepth(GraphicsDeviceContext &){}
			//! Set the API-dependent colour buffer format for this framebuffer. Across all API's, setting 0 means no rendering to colour.
			virtual void SetFormat(PixelFormat);
			//! Set the API-dependent colour depth format for this framebuffer. Across all API's, setting 0 means no rendering to depth.
			virtual void SetDepthFormat(PixelFormat);
			PixelFormat GetFormat() const
			{
				return target_format;
			}
			PixelFormat GetDepthFormat() const
			{
				return depth_format;
			}
			virtual void SetGenerateMips(bool);
			//! Clear the colour and depth buffers if present.
			virtual void Clear(crossplatform::GraphicsDeviceContext &context,float r,float g,float b,float a,float depth);
			//! Clear the colour buffers if present.
			virtual void ClearColour(crossplatform::GraphicsDeviceContext &context,float r,float g,float b,float a);
			//! Clear the depth buffers if present.
			virtual void ClearDepth(crossplatform::GraphicsDeviceContext &context,float depth);
			//! Set the size of the framebuffer.
			virtual void SetWidthAndHeight(int w,int h,int num_mips=1);
			//! Set this to be a cubemap framebuffer, so that its texture object will be a cubemap. Equivalent to SetWidthAndHeight.
			virtual void SetAsCubemap(int face_size,int num_mips=1,crossplatform::PixelFormat f=RGBA_MAX_FLOAT);
			virtual void SetCubeFace(int f);
			//! Some hardware has fast RAM that's good for framebuffers.
			virtual void SetUseFastRAM(bool /*colour*/,bool /*depth*/){};
			virtual void SetAntialiasing(int s)=0;
			//! Get the texture for the colour buffer target.
			inline Texture *GetTexture()
			{
				return buffer_texture;
			}
			//! Get the texture for the depth buffer target.
			inline Texture *GetDepthTexture()
			{
				return buffer_depth_texture;
			}
			//! Get the dimension of the surface
			inline int GetWidth()
			{
				return Width;
			}
			//! Get the dimension of the surface
			inline int GetHeight()
			{
				return Height;
			}
		//protected:
			//! The size of the buffer
			int Width,Height;
			int mips;
			int numAntialiasingSamples;
			bool depth_active, colour_active;
			crossplatform::RenderPlatform *renderPlatform;
			vec4 DefaultClearColour;
			float DefaultClearDepth;
			uint DefaultClearStencil;
		protected:
			crossplatform::TargetsAndViewport targetsAndViewport;
			//! The depth buffer.
			crossplatform::Texture				*buffer_texture=nullptr;
			crossplatform::Texture				*buffer_depth_texture=nullptr;
			bool GenerateMips;
			bool is_cubemap;
			int	current_face;
			crossplatform::PixelFormat target_format;
			crossplatform::PixelFormat depth_format;
			int activate_count=0;

			bool external_texture=false;
			bool external_depth_texture=false;
			std::string name;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif