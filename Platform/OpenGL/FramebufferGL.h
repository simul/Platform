// Simple render to texture class
//
// Author: Simon Green
// Email: sdkfeedback@nvidia.com
//
// Copyright (c) NVIDIA Corporation. All rights reserved.

#ifndef RENDER_TEXTURE_FBO__H
#define RENDER_TEXTURE_FBO__H

#include <GL/glew.h>
#include <stack>
#include "LoadGLImage.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Clouds/BaseFramebuffer.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

SIMUL_OPENGL_EXPORT_CLASS FramebufferGL:public BaseFramebuffer
{
public:
	FramebufferGL(int w, int h, GLenum target = GL_TEXTURE_RECTANGLE_NV,const char *shader=0,
			int samples = 0, int coverageSamples = 0);

	~FramebufferGL();
	void InvalidateDeviceObjects();
	void SetExposure(float e)
	{
		exposure = e;
	}
	void SetGamma(float g)
	{
		gamma = g;
	}
	void SetShader(int i);
	void SetWidthAndHeight(int w,int h);
	// In order to use a color buffer, either
	// InitColor_RB or InitColor_Tex needs to be called.
	//void InitColor_RB(int index, GLenum internal_format);
	void InitColor_Tex(int index, GLenum internal_format,GLenum format);
	// In order to use a depth buffer, either
	// InitDepth_RB or InitDepth_Tex needs to be called.
	void InitDepth_RB(GLenum iformat = GL_DEPTH_COMPONENT24);
	void InitDepth_Tex(GLenum iformat = GL_DEPTH_COMPONENT24);
	// Activate / deactivate the FBO as a render target
	// The FBO needs to be deactivated when using the associated textures.
	void Activate();
	void Deactivate();
	void Clear(float r,float g,float b,float a);
	void DeactivateAndRender(bool blend);
	void Render(bool blend);
	void Render(GLuint prog,bool blend);
void Render1(GLuint prog,bool blend);
	void DrawQuad(int w, int h);
	// Get the dimension of the surface
	inline int GetWidth()
	{
		return m_width;
	}
	inline int GetHeight()
	{
		return m_height;
	}
	// Get the internal texture object IDs.
	inline GLuint GetColorTex(int index = 0)
	{
		return m_tex_col[index];
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
	void RecompileShaders();
	GLuint GetProgram() const 
	{
		return tonemap_program;
	}
private:
	const char *shader_filename;
	static std::stack<GLuint> fb_stack;
	void CheckFramebufferStatus();
	// Bind the internal textures
	void BindColor(int index = 0)
	{
		glBindTexture(m_target, m_tex_col[index]);
	}
	inline void Bind(int index = 0)
	{
		BindColor(index);
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
	const static int num_col_buffers = 16;
	int main_viewport[4];
	int m_width, m_height;
	GLenum m_target;
	int m_samples; // 0 if not multisampled
	int m_coverageSamples; // for CSAA
	GLuint m_fb;
	GLuint m_tex_col[num_col_buffers], m_rb_col[num_col_buffers];
	GLuint m_tex_depth, m_rb_depth;
	// shaders
	GLuint tonemap_vertex_shader;
	GLuint tonemap_fragment_shader;
	GLuint tonemap_program;
	GLint exposure_param;
	GLint gamma_param;
	GLint buffer_tex_param;
	float exposure, gamma;
	bool initialized;
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#endif // RENDER_TEXTURE_FBO__H
