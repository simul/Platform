#ifndef FRAMEBUFFERGL_H
#define FRAMEBUFFERGL_H

#include <stack>
#include "stdint.h" // for uintptr_t
#include "LoadGLImage.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
namespace simul
{
	namespace opengl
	{
		SIMUL_OPENGL_EXPORT_CLASS FramebufferGL:public simul::crossplatform::BaseFramebuffer
		{
		public:
			FramebufferGL(int w=0, int h=0, GLenum target = GL_TEXTURE_2D,
					int samples = 0, int coverageSamples = 0);

			~FramebufferGL();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			bool IsValid() const;
			bool CreateBuffers();
			void InvalidateDeviceObjects();
			void SetWidthAndHeight(int w,int h);
			void SetAsCubemap(int face_size);
			void SetCubeFace(int f);
			void SetAntialiasing(int ){}
			void SetFormat(crossplatform::PixelFormat p);
			void SetDepthFormat(crossplatform::PixelFormat p);
			void SetWrapClampMode(GLint wr);
			// In order to use a color buffer, either
			// InitColor_RB or InitColor_Tex needs to be called.
			//void InitColor_RB(int index, GLenum internal_format);
			bool InitColor_Tex(int index, crossplatform::PixelFormat p);
			/// Use the existing depth buffer
			void NoDepth();
			/// Activate / deactivate the FBO as a render target
			/// The FBO needs to be deactivated when using the associated textures.
			void Activate(crossplatform::DeviceContext &);
			void ActivateDepth(crossplatform::DeviceContext &);
			void ActivateColour(crossplatform::DeviceContext &,const float [4]);
			void ActivateViewport(crossplatform::DeviceContext &,float viewportX, float viewportY, float viewportW, float viewportH);
			void Deactivate(crossplatform::DeviceContext &);
			void CopyDepthFromFramebuffer();
			void Clear(crossplatform::DeviceContext &,float r,float g,float b,float a,float depth,int mask=0);
			void ClearColour(crossplatform::DeviceContext &,float r,float g,float b,float a);
			void DeactivateAndRender(crossplatform::DeviceContext &deviceContext,bool blend);
			void Render(void *,bool blend);
			inline GLuint GetFramebuffer()
			{
				return m_fb;
			}
			static void CheckFramebufferStatus();
			static std::stack<GLuint> fb_stack;
		private:
			// Bind the internal textures
			void BindColor()
			{
				glBindTexture(m_target, buffer_texture->AsGLuint());//m_tex_col[0]);
			}
			inline void Bind()
			{
				BindColor();
			}
			// aliased to BindColor.  this reduces app code changes while migrating
			// from the pbuffer implementation.
			void BindDepth()
			{
				glBindTexture(m_target,buffer_depth_texture->AsGLuint());// m_tex_depth);
			}
			void Release()
			{
				glBindTexture(m_target, 0);
			}
			const static int num_col_buffers = 1;
			int main_viewport[4];
			GLenum m_target;
			int m_samples; // 0 if not multisampled
			int m_coverageSamples; // for CSAA
			GLuint m_fb;
			bool initialized;
			GLint wrap_clamp;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#endif // RENDER_TEXTURE_FBO__H
