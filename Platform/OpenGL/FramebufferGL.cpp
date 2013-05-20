
#include <GL/glew.h>
#include "FramebufferGL.h"
#include <iostream>
#include <string>
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"
std::stack<GLuint> FramebufferGL::fb_stack;

FramebufferGL::FramebufferGL(int w,int h,GLenum target,int samples,int coverageSamples)
	:BaseFramebuffer(w,h)
	,m_target(target)
	,m_samples(samples)
	,m_coverageSamples(coverageSamples)
	,m_tex_depth(0)
	,m_rb_depth(0)
	,m_fb(0)
	,initialized(false)
	,depth_iformat(0)
	,colour_iformat(0)
{
    for(int i = 0; i < num_col_buffers; i++)
        m_tex_col[i] = 0;
	if(fb_stack.size()==0)
		fb_stack.push((GLuint)0);
}

FramebufferGL::~FramebufferGL()
{
	InvalidateDeviceObjects();
}

void FramebufferGL::RestoreDeviceObjects(void*)
{
}

void FramebufferGL::InvalidateDeviceObjects()
{
    int i;
    for(i = 0; i < num_col_buffers; i++)
	{
        if(m_tex_col[i])
			glDeleteTextures(1,&m_tex_col[i]);
		m_tex_col[i]=0;
    }
    if(m_tex_depth)
	{
		glDeleteTextures(1,&m_tex_depth);
		m_tex_depth=0;
	}
	if(m_rb_depth)
	{
		glDeleteRenderbuffers(1,&m_rb_depth);
		m_rb_depth=0;
	}
	glDeleteFramebuffers(1,&m_fb);
	m_fb=0;
}

void FramebufferGL::SetWidthAndHeight(int w,int h)
{
	if(w!=Width||h!=Height)
	{
		Width=w;
		Height=h;
		SAFE_DELETE_TEXTURE(m_tex_col[0]);
		SAFE_DELETE_RENDERBUFFER(m_rb_depth);
		SAFE_DELETE_FRAMEBUFFER(m_fb);
		if(initialized)
			InitColor_Tex(0,colour_iformat,format,wrap_clamp);
	}
}

void FramebufferGL::SetFormat(int f)
{
	colour_iformat=f;
}


bool FramebufferGL::InitColor_Tex(int index, GLenum iformat,GLenum f,GLint wc)
{
	if(!Width||!Height)
		return true;
ERROR_CHECK
	bool ok=true;
	initialized=true;
	if(!m_fb)
	{
		glGenFramebuffers(1, &m_fb);
	}
	format=f;
	wrap_clamp=wc;
	if(!m_tex_col[index]||iformat!=colour_iformat)
	{
		colour_iformat=iformat;
		SAFE_DELETE_TEXTURE(m_tex_col[index]);
		glGenTextures(1, &m_tex_col[index]);
		glBindTexture(m_target, m_tex_col[index]);
		glTexParameteri(m_target,GL_TEXTURE_WRAP_S,wrap_clamp);
		glTexParameteri(m_target,GL_TEXTURE_WRAP_T,wrap_clamp);
		glTexParameteri(m_target,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(m_target,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		//glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexImage2D(m_target,0, colour_iformat, Width, Height,0,GL_RGBA, format, NULL);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0 + index, m_target, m_tex_col[index], 0);
		GLenum status = (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status!=GL_FRAMEBUFFER_COMPLETE)
			ok=false;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
ERROR_CHECK
	return ok;
}
// In order to use a depth buffer, either
// InitDepth_RB or InitDepth_Tex needs to be called.
void FramebufferGL::InitDepth_RB(GLenum iformat)
{
	if(!Width||!Height)
		return;
	initialized=true;
	if(!m_fb)
	{
		glGenFramebuffers(1, &m_fb);
	}
	depth_iformat=iformat;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fb); 
    glGenRenderbuffers(1, &m_rb_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rb_depth);
	if (m_samples > 0) {
        if ((m_coverageSamples > 0) && glRenderbufferStorageMultisampleCoverageNV)
		{
            glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER, m_coverageSamples, m_samples, iformat, Width, Height);
        }
		else
		{
		    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, iformat, Width, Height);
        }
	}
	else
	{
		glRenderbufferStorage(GL_RENDERBUFFER, iformat, Width, Height);
	}
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,m_rb_depth);
	//Also attach as a stencil
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rb_depth);
	CheckFramebufferStatus();
	ERROR_CHECK
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
	ERROR_CHECK
}

void FramebufferGL::NoDepth()
{
}

void FramebufferGL::InitDepth_Tex(GLenum iformat)
{
	initialized=true;
	if(!m_fb)
	{
		glGenFramebuffers(1, &m_fb);
	}
	depth_iformat=iformat;
	glGenTextures(1, &m_tex_depth);
	glBindTexture(m_target, m_tex_depth);
	glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexImage2D(m_target, 0, iformat, Width, Height, 0,GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,m_target,m_tex_depth,0);
	CheckFramebufferStatus();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// Activate / deactivate the FBO as a render target
// The FBO needs to be deactivated when using the associated Textures.
void FramebufferGL::Activate(void *context)
{
	Activate(context,0,0,Width,Height);
}

void FramebufferGL::Activate(void *context,int x,int y,int w,int h)
{
	//glFlush(); 
	CheckFramebufferStatus();
    glBindFramebuffer(GL_FRAMEBUFFER, m_fb); 
	ERROR_CHECK
	CheckFramebufferStatus();
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	ERROR_CHECK
	glViewport(x,y,w,h);
	ERROR_CHECK
	fb_stack.push(m_fb);
}

void FramebufferGL::Deactivate(void *context) 
{
	ERROR_CHECK
	//glFlush(); 
	ERROR_CHECK
	CheckFramebufferStatus();
	ERROR_CHECK
	// remove m_fb from the stack and...
	fb_stack.pop();
	// ..restore the n one down.
	GLuint last_fb=fb_stack.top();
	ERROR_CHECK
    glBindFramebuffer(GL_FRAMEBUFFER,last_fb);
	ERROR_CHECK
	glViewport(0,0,main_viewport[2],main_viewport[3]);
	ERROR_CHECK
}
void FramebufferGL::CopyDepthFromFramebuffer() 
{
	glBindFramebuffer (GL_READ_FRAMEBUFFER, m_fb);
	ERROR_CHECK
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, fb_stack.top());
	ERROR_CHECK
	glBlitFramebuffer (0, 0, Width, Height, 0, 0, Width, Height,
			   GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	ERROR_CHECK
	glBindFramebuffer (GL_FRAMEBUFFER, fb_stack.top());
	ERROR_CHECK
}

void FramebufferGL::DrawQuad(void *context)
{
	::DrawQuad(0,0,1,1);
}
void FramebufferGL::DeactivateAndRender(void *context,bool blend)
{
	ERROR_CHECK
	Deactivate(context);
	Render(context,blend);
}

void FramebufferGL::Render(void *,bool blend)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	Ortho();
   // SetOrthoProjection(main_viewport[2],main_viewport[3]);
    glActiveTexture(GL_TEXTURE0);
    Bind();
    glDisable(GL_ALPHA_TEST);
	if(!blend)
	{
		glDisable(GL_DEPTH_TEST);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
	}
	glDepthMask(GL_FALSE);
ERROR_CHECK
	::DrawQuad(0,0,1,1);//main_viewport[2],main_viewport[3]);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
ERROR_CHECK
	Release();
}


void FramebufferGL::Clear(void*,float r,float g,float b,float a,float depth,int mask)
{
	if(!mask)
		mask=GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT;
	glClearColor(r,g,b,a);
	glClearDepth(depth);
	glClear(mask);
}


void FramebufferGL::CheckFramebufferStatus()
{
    GLenum status;
    status = (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch(status)
    {
        case GL_FRAMEBUFFER_COMPLETE:
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
			std::cerr<<("Unsupported framebuffer format\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr<<("Framebuffer incomplete attachment\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr<<("Framebuffer incomplete, missing attachment\n");
            break;
    /*    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            std::cerr<<("Framebuffer incomplete, attached images must have same dimensions\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
            std::cerr<<("Framebuffer incomplete, attached images must have same format\n");
            break;*/
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            std::cerr<<("Framebuffer incomplete, missing draw buffer\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            std::cerr<<("Framebuffer incomplete, missing read buffer\n");
            break;
        default:
			std::cerr<<"Unknown error "<<(int)status<<std::endl;
    }
}
