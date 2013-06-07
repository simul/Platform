#ifndef FRAMEBUFFERGL_H
#define FRAMEBUFFERGL_H

#include <stack>
#include "LoadGLImage.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Clouds/BaseFramebuffer.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

SIMUL_OPENGL_EXPORT_CLASS FramebufferGL:public BaseFramebuffer
{
public:
	FramebufferGL(int w=0, int h=0, GLenum target = GL_TEXTURE_2D,
			int samples = 0, int coverageSamples = 0);

	~FramebufferGL();
	void RestoreDeviceObjects(void*);
	void InvalidateDeviceObjects();
	void SetShader(int i);
	void SetWidthAndHeight(int w,int h);
	void SetFormat(int);
	// In order to use a color buffer, either
	// InitColor_RB or InitColor_Tex needs to be called.
	//void InitColor_RB(int index, GLenum internal_format);
	bool InitColor_Tex(int index, GLenum internal_format,GLenum format,GLint wrap_clamp=GL_CLAMP_TO_EDGE);
	// In order to use a depth buffer, either
	// InitDepth_RB or InitDepth_Tex needs to be called.
	void InitDepth_RB(GLenum iformat = GL_DEPTH_COMPONENT24);
	void InitDepth_Tex(GLenum iformat = GL_DEPTH_COMPONENT24);
	/// Use the existing depth buffer
	void NoDepth();
	/// Activate / deactivate the FBO as a render target
	/// The FBO needs to be deactivated when using the associated textures.
	void Activate(void *context);
	/// Activate rendering to a viewport
	void Activate(void *context,int x,int y,int w,int h);
	void Deactivate(void *context);
	void CopyDepthFromFramebuffer();
	void Clear(void*,float r,float g,float b,float a,float depth,int mask=0);
	void DeactivateAndRender(void *,bool blend);
	void Render(void *,bool blend);
	// Get the dimension of the surface
	inline int GetWidth()
	{
		return Width;
	}
	inline int GetHeight()
	{
		return Height;
	}
	// Get the internal texture object IDs.
	void* GetColorTex()
	{
		return (void*) m_tex_col[0];
	}
	inline GLenum GetDepthTex()
	{
		return m_tex_depth;
	}
	// Get the target texture format (texture2d or texture_rectangle)
	inline GLenum GetTarget()
	{
		return m_target;
	}
	inline GLuint GetFramebuffer()
	{
		return m_fb;
	}
	void CopyToMemory(void *context,void *target,int start_texel,int num_texels);
private:
	static std::stack<GLuint> fb_stack;
	void CheckFramebufferStatus();
	// Bind the internal textures
	void BindColor()
	{
		glBindTexture(m_target, m_tex_col[0]);
	}
	inline void Bind()
	{
		BindColor();
	}
	// aliased to BindColor.  this reduces app code changes while migrating
	// from the pbuffer implementation.
	void BindDepth()
	{
		glBindTexture(m_target, m_tex_depth);
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
	GLuint m_tex_col[num_col_buffers];//, m_rb_col[num_col_buffers];
	GLuint m_tex_depth, m_rb_depth;
	GLenum colour_iformat,depth_iformat;
	GLenum format;
	bool initialized;
	GLint wrap_clamp;
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#endif // RENDER_TEXTURE_FBO__H
