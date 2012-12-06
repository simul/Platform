#include "FramebufferGL.h"
#include <iostream>
#include <string>
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"
std::stack<GLuint> FramebufferGL::fb_stack;

FramebufferGL::FramebufferGL(int w, int h,GLenum target,const char *shader,int samples, int coverageSamples):
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
	,shader_filename(shader)
	,tonemap_program(0)
	,initialized(false)
	,depth_iformat(0)
	,colour_iformat(0)
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

void FramebufferGL::RecompileShaders()
{
	if(!shader_filename)
		return;
	tonemap_vertex_shader	=glCreateShader(GL_VERTEX_SHADER);
ERROR_CHECK
	tonemap_fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);
ERROR_CHECK
	tonemap_program			=glCreateProgram();
ERROR_CHECK
	std::string str1=std::string(shader_filename)+".vert";
	tonemap_vertex_shader	=LoadShader(tonemap_vertex_shader,str1.c_str());
    tonemap_fragment_shader	=LoadShader(tonemap_fragment_shader,(std::string(shader_filename)+std::string(".frag")).c_str());
	glAttachShader(tonemap_program, tonemap_vertex_shader);
	glAttachShader(tonemap_program, tonemap_fragment_shader);
	glLinkProgram(tonemap_program);
	glUseProgram(tonemap_program);
	ERROR_CHECK
	printProgramInfoLog(tonemap_program);
    exposure_param=glGetUniformLocation(tonemap_program,"exposure");
    gamma_param=glGetUniformLocation(tonemap_program,"gamma");
    buffer_tex_param=glGetUniformLocation(tonemap_program,"image_texture");
	ERROR_CHECK
}

FramebufferGL::~FramebufferGL()
{
	InvalidateDeviceObjects();
}

void FramebufferGL::InvalidateDeviceObjects()
{
    int i;
    for(i = 0; i < num_col_buffers; i++)
	{
        if(m_tex_col[i])
		{
			glDeleteTextures(1,&m_tex_col[i]);
		}
		m_tex_col[i]=0;
        if(m_rb_col[i])
		{
			glDeleteRenderbuffersEXT(1,&m_rb_col[i]);
		}
		m_rb_col[i]=0;
    }
    if(m_tex_depth)
	{
		glDeleteTextures(1,&m_tex_depth);
		m_tex_depth=0;
	}
	if(m_rb_depth)
	{
		glDeleteRenderbuffersEXT(1,&m_rb_depth);
		m_rb_depth=0;
	}
	glDeleteFramebuffersEXT(1,&m_fb);
	m_fb=0;
}

void FramebufferGL::SetWidthAndHeight(int w,int h)
{
	if(w!=m_width||h!=m_height)
	{
		m_width=w;
		m_height=h;
		SAFE_DELETE_TEXTURE(m_tex_col[0]);
		SAFE_DELETE_RENDERBUFFER(m_rb_depth);
		SAFE_DELETE_FRAMEBUFFER(m_fb);
	}
}

bool FramebufferGL::InitColor_Tex(int index, GLenum iformat,GLenum format,GLint wrap_clamp)
{
	if(!m_width||!m_height)
		return true;
	bool ok=true;
	initialized=true;
	if(!m_fb)
	{
		RecompileShaders();
		glGenFramebuffersEXT(1, &m_fb);
	}
	if(!m_tex_col[index]||iformat!=colour_iformat)
	{
		colour_iformat=iformat;
		glGenTextures(1, &m_tex_col[index]);
		glBindTexture(m_target, m_tex_col[index]);
		glTexParameteri(m_target, GL_TEXTURE_WRAP_S, wrap_clamp);
		glTexParameteri(m_target, GL_TEXTURE_WRAP_T, wrap_clamp);
		glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(m_target, 0, colour_iformat, m_width, m_height, 0,GL_RGBA, format, NULL);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT + index, m_target, m_tex_col[index], 0);
		GLenum status = (GLenum) glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		if(status!=GL_FRAMEBUFFER_COMPLETE_EXT)
			ok=false;
	    
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}
	return ok;
}
// In order to use a depth buffer, either
// InitDepth_RB or InitDepth_Tex needs to be called.
void FramebufferGL::InitDepth_RB(GLenum iformat)
{
	if(!m_width||!m_height)
		return;
	initialized=true;
	if(!m_fb)
	{
		RecompileShaders();
		glGenFramebuffersEXT(1, &m_fb);
	}
	depth_iformat=iformat;
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb); 
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
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,m_rb_depth);
	//Also attach as a stencil
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_rb_depth);
	CheckFramebufferStatus();
	ERROR_CHECK
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); 
	ERROR_CHECK
}

void FramebufferGL::InitDepth_Tex(GLenum iformat)
{
	initialized=true;
	if(!m_fb)
	{
		RecompileShaders();
		glGenFramebuffersEXT(1, &m_fb);
	}
	depth_iformat=iformat;
	glGenTextures(1, &m_tex_depth);
	glBindTexture(m_target, m_tex_depth);
	glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(m_target, 0, iformat, m_width, m_height, 0,GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,m_target,m_tex_depth,0);
	CheckFramebufferStatus();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}


// Activate / deactivate the FBO as a render target
// The FBO needs to be deactivated when using the associated textures.
void FramebufferGL::Activate()
{
	glFlush(); 
	CheckFramebufferStatus();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fb); 
	ERROR_CHECK
	CheckFramebufferStatus();
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	ERROR_CHECK
	glViewport(0,0,m_width,m_height);
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
	ERROR_CHECK
	Deactivate();
	Render(tonemap_program,blend);
}

void FramebufferGL::Render1(GLuint prog,bool blend)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
    SetOrthoProjection(main_viewport[2],main_viewport[3]);

    // bind textures
    glActiveTexture(GL_TEXTURE0);
    Bind();

	glUseProgram(prog);

	if(prog)
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
		// retain background based on alpha in overlay
		glBlendFunc(GL_ONE,GL_SRC_ALPHA);
	}
	glDepthMask(GL_FALSE);
	ERROR_CHECK
    DrawQuad(main_viewport[2],main_viewport[3]);

    glUseProgram(NULL);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
	ERROR_CHECK
	Release();
}

void FramebufferGL::Render(GLuint ,bool blend)
{
	Render(blend);
}

void FramebufferGL::Render(bool blend)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
    SetOrthoProjection(main_viewport[2],main_viewport[3]);

    // bind textures
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
		// retain background based on alpha in overlay
		//glBlendFunc(GL_ONE,GL_SRC_ALPHA);
	}
	glDepthMask(GL_FALSE);
	ERROR_CHECK
    DrawQuad(main_viewport[2],main_viewport[3]);

  //  glUseProgram(NULL);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
	ERROR_CHECK
	Release();
}

void FramebufferGL::Deactivate() 
{
	ERROR_CHECK
	glFlush(); 
	ERROR_CHECK
	CheckFramebufferStatus();
	ERROR_CHECK
	// remove m_fb from the stack and...
	fb_stack.pop();
	// .. restore the next one down.
	GLuint last_fb=fb_stack.top();
	ERROR_CHECK
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,last_fb);
	ERROR_CHECK
	glViewport(0,0,main_viewport[2],main_viewport[3]);
	ERROR_CHECK
}

void FramebufferGL::Clear(float r,float g,float b,float a,int mask)
{
	if(!mask)
		mask=GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT;
	glClearColor(r,g,b,a);
	
	glClear(mask);
}


void FramebufferGL::CheckFramebufferStatus()
{
    GLenum status;
    status = (GLenum) glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    switch(status)
    {
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
