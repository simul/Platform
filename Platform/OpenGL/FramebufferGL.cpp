#include "FramebufferGL.h"
#include <iostream>
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"
std::stack<GLuint> FramebufferGL::fb_stack;

FramebufferGL::FramebufferGL(int w, int h, GLenum target, int samples, int coverageSamples):
    m_width(w)
	,m_height(h)
	,m_target(target)
	,m_samples(samples)
	,m_coverageSamples(coverageSamples)
	,m_tex_depth(0)
	,m_rb_depth(0)
	,exposure(1.f)
	,gamma(0.45f)
	,m_fb(0)
{
    for(int i = 0; i < num_col_buffers; i++)
	{
        m_rb_col[i] = 0;
        m_tex_col[i] = 0;
    }
	if(fb_stack.size()==0)
		fb_stack.push((GLuint)0);
}

void FramebufferGL::SetShader(int i)
{
	if(i==0)
		tonemap_program=0;
}

void FramebufferGL::InitShader()
{
	tonemap_vertex_shader	=glCreateShader(GL_VERTEX_SHADER);
	tonemap_fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);

	tonemap_program			=glCreateProgram();

    tonemap_vertex_shader	=LoadProgram(tonemap_vertex_shader,"tonemap.vert");
    tonemap_fragment_shader	=LoadProgram(tonemap_fragment_shader,"tonemap.frag");
	glAttachShader(tonemap_program, tonemap_vertex_shader);
	glAttachShader(tonemap_program, tonemap_fragment_shader);
	glLinkProgram(tonemap_program);
	glUseProgram(tonemap_program);
	printProgramInfoLog(tonemap_program);
    exposure_param=glGetUniformLocation(tonemap_program,"exposure");
    gamma_param=glGetUniformLocation(tonemap_program,"gamma");
    buffer_tex_param=glGetUniformLocation(tonemap_program,"sceneTex");
}

FramebufferGL::~FramebufferGL()
{
    int i;
    for(i = 0; i < num_col_buffers; i++)
	{
        if(m_tex_col[i])
		{
			glDeleteTextures(1,&m_tex_col[i]);
		}
        if(m_rb_col[i])
		{
			glDeleteRenderbuffersEXT(1,&m_rb_col[i]);
		}
    }
    if(m_tex_depth)
	{
		glDeleteTextures(1,&m_tex_depth);
	}
	if(m_rb_depth)
	{
		glDeleteRenderbuffersEXT(1,&m_rb_depth);
	}
	glDeleteFramebuffersEXT(1,&m_fb);
}

void FramebufferGL::SetWidthAndHeight(int w,int h)
{
	m_width=w;
	m_height=h;
}
// In order to use a color buffer, either
// InitColor_RB or InitColor_Tex needs to be called.
void FramebufferGL::InitColor_RB(int index, GLenum iformat)
{
	if(!m_width||!m_height)
		return;
	if(!m_fb)
	{
		InitShader();
		glGenFramebuffersEXT(1, &m_fb);
	}
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb);
    
	ERROR_CHECK
        glGenRenderbuffersEXT(1, &m_rb_col[index]);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_rb_col[index]);
		if (m_samples > 0) {
            if ((m_coverageSamples > 0) && glRenderbufferStorageMultisampleCoverageNV)
			{
                glRenderbufferStorageMultisampleCoverageNV(
                    GL_RENDERBUFFER_EXT, m_coverageSamples, m_samples, iformat, m_width, m_height);
            }
			else
			{
		        glRenderbufferStorageMultisampleEXT(
                    GL_RENDERBUFFER_EXT, m_samples, iformat, m_width, m_height);
            }
		}
		else
		{
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, iformat, m_width, m_height);
		}
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                GL_COLOR_ATTACHMENT0_EXT + index, GL_RENDERBUFFER_EXT, m_rb_col[index]);
    
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb); 
	ERROR_CHECK
}

void FramebufferGL::InitColor_Tex(int index, GLenum iformat)
{
	if(!m_width||!m_height)
		return;
	if(!m_fb)
	{
		InitShader();
		glGenFramebuffersEXT(1, &m_fb);
	}
	glGenTextures(1, &m_tex_col[index]);
	ERROR_CHECK
	glBindTexture(m_target, m_tex_col[index]);
	ERROR_CHECK
	glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(m_target, 0, iformat, m_width, m_height, 0,GL_RGBA, GL_INT, NULL);
	ERROR_CHECK
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT + index, m_target, m_tex_col[index], 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	ERROR_CHECK
}
void FramebufferGL::InitColor_None()
{
	if(!m_fb)
	{
		InitShader();
		glGenFramebuffersEXT(1, &m_fb);
	}
    // turn the color buffer off in case this is a z only fbo
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb); 
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); 
}

// In order to use a depth buffer, either
// InitDepth_RB or InitDepth_Tex needs to be called.
void FramebufferGL::InitDepth_RB(GLenum iformat)
{
	if(!m_fb)
	{
		InitShader();
		glGenFramebuffersEXT(1, &m_fb);
	}
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb); 
	ERROR_CHECK
    
        glGenRenderbuffersEXT(1, &m_rb_depth);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_rb_depth);
		if (m_samples > 0) {
            if ((m_coverageSamples > 0) && glRenderbufferStorageMultisampleCoverageNV)
			{
                glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER_EXT, m_coverageSamples, m_samples, iformat, m_width, m_height);
            }
			else
			{
			    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, m_samples, iformat, m_width, m_height);
            }
		}
		else
		{
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, iformat, m_width, m_height);
		}
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_rb_depth);
    
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb); 
	ERROR_CHECK
}

void FramebufferGL::InitDepth_Tex(GLenum iformat)
{
	if(!m_fb)
	{
		InitShader();
		glGenFramebuffersEXT(1, &m_fb);
	}
	glGenTextures(1, &m_tex_depth);
	glBindTexture(m_target, m_tex_depth);
	glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(m_target, 0, iformat, m_width, m_height, 0,
            GL_DEPTH_COMPONENT, GL_INT, NULL);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
            GL_DEPTH_ATTACHMENT_EXT, m_target, m_tex_depth, 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}


// Activate / deactivate the FBO as a render target
// The FBO needs to be deactivated when using the associated textures.
void FramebufferGL::Activate()
{
	//glFlush(); 
	CheckFramebufferStatus();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb); 
	ERROR_CHECK
#ifdef _DEBUG
	CheckFramebufferStatus();
#endif
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	ERROR_CHECK
	glViewport(0,0,m_width,m_height);
	ERROR_CHECK
	glClearColor(0.f,0.f,0.f,1.f);
	ERROR_CHECK
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	ERROR_CHECK
	fb_stack.push(m_fb);
}

void FramebufferGL::DrawQuad(int w,int h)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0.f,1.f);
	glVertex2f(0.f,(float)h);
	glTexCoord2f(1.f,1.f);
	glVertex2f((float)w,(float)h);
	glTexCoord2f(1.0,0.f);
	glVertex2f((float)w,0.f);
	glTexCoord2f(0.f,0.f);
	glVertex2f(0.f,0.f);
	glEnd();
}
void FramebufferGL::DeactivateAndRender(bool blend)
{
	Deactivate();
	Render(blend);
}


void FramebufferGL::Render(bool blend)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    SetOrthoProjection(main_viewport[2],main_viewport[3]);

    // bind textures
    glActiveTexture(GL_TEXTURE0);
    Bind();

	glUseProgram(tonemap_program);

	if(tonemap_program)
	{
		glUniform1f(exposure_param,exposure);

		glUniform1f(gamma_param,gamma);
		glUniform1i(buffer_tex_param,0);
	}
	else
	{
		glDisable(GL_TEXTURE_1D);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_3D);
	}
    glDisable(GL_ALPHA_TEST);
	if(!blend)
	{
		glDisable(GL_DEPTH_TEST);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_SRC_ALPHA);
	}
	glDepthMask(GL_FALSE);
	ERROR_CHECK
    DrawQuad(main_viewport[2],main_viewport[3]);

    glUseProgram(NULL);
	glPopAttrib();
	ERROR_CHECK
}

void FramebufferGL::Deactivate() 
{
	// remove m_fb from the stack and...
	fb_stack.pop();
	// .. restore the next one down.
	GLuint last_fb=fb_stack.top();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,last_fb);
	ERROR_CHECK
	glViewport(0,0,main_viewport[2],main_viewport[3]);
	ERROR_CHECK
}


void FramebufferGL::CheckFramebufferStatus()
{
    GLenum status;
    status = (GLenum) glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    switch(status) {
        case GL_FRAMEBUFFER_COMPLETE_EXT:
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			std::cerr<<("Unsupported framebuffer format\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
            std::cerr<<("Framebuffer incomplete attachment\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            std::cerr<<("Framebuffer incomplete, missing attachment\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            std::cerr<<("Framebuffer incomplete, attached images must have same dimensions\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            std::cerr<<("Framebuffer incomplete, attached images must have same format\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            std::cerr<<("Framebuffer incomplete, missing draw buffer\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            std::cerr<<("Framebuffer incomplete, missing read buffer\n");
            break;
        default:
			std::cerr<<"Unknown error "<<(int)status<<std::endl;
    }
}
