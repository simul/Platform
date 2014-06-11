#include <GL/glew.h>
#include "FramebufferGL.h"
#include <iostream>
#include <string>
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"
#include "Simul/Base/RuntimeError.h"
#ifdef _MSC_VER
#include <windows.h>
#else
#define DebugBreak() 
#endif

using namespace simul;
using namespace opengl;
std::stack<GLuint> FramebufferGL::fb_stack;

FramebufferGL::FramebufferGL(int w,int h,GLenum target,int samples,int coverageSamples)
	:crossplatform::BaseFramebuffer(w,h)
	,m_target(target)
	,m_samples(samples)
	,m_coverageSamples(coverageSamples)
//	,m_tex_depth(0)
//	,m_rb_depth(0)
	,m_fb(0)
	,initialized(false)
	,depth_iformat(0)
	,colour_iformat(0)
	,wrap_clamp(GL_CLAMP_TO_EDGE)
{
 //   for(int i = 0; i < num_col_buffers; i++)
//        m_tex_col[i] = 0;
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
	GL_ERROR_CHECK
	buffer_texture.InvalidateDeviceObjects();
	GL_ERROR_CHECK
	buffer_depth_texture.InvalidateDeviceObjects();
	GL_ERROR_CHECK
	//SAFE_DELETE_TEXTURE(m_tex_col[0]);
	//SAFE_DELETE_TEXTURE(m_tex_depth);
	//SAFE_DELETE_RENDERBUFFER(m_rb_depth);
	SAFE_DELETE_FRAMEBUFFER(m_fb);
	GL_ERROR_CHECK
}

void FramebufferGL::SetWidthAndHeight(int w,int h)
{
	if(w!=Width||h!=Height)
	{
		Width=w;
		Height=h;
		InvalidateDeviceObjects();
		if(initialized)
		{
			InitColor_Tex(0,colour_iformat);
			if(depth_iformat)
				InitDepth_RB(depth_iformat);
		}
	}
}

void FramebufferGL::SetFormat(int f)
{
	if((GLenum)f!=colour_iformat)
		InvalidateDeviceObjects();
	colour_iformat=f;
}

void FramebufferGL::SetDepthFormat(int f)
{
	if((GLenum)f!=depth_iformat)
		InvalidateDeviceObjects();
	depth_iformat=f;
}

void FramebufferGL::SetWrapClampMode(GLint wr)
{
	wrap_clamp=wr;
}

bool FramebufferGL::InitColor_Tex(int , GLenum iformat)
{
	SetFormat(iformat);
	return Init();
}

bool FramebufferGL::Init()
{
	if(!Width||!Height)
		return false;
GL_ERROR_CHECK
	initialized=true;
	if(!m_fb)
	{
		glGenFramebuffers(1, &m_fb);
	}
	buffer_texture.InvalidateDeviceObjects();//SAFE_DELETE_TEXTURE(m_tex_col[0]);
	buffer_depth_texture.InvalidateDeviceObjects();//SAFE_DELETE_TEXTURE(m_tex_depth);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
	if(colour_iformat)
	{
		glGenTextures(1, &buffer_texture.pTextureObject);//m_tex_col[0]);
		glBindTexture(GL_TEXTURE_2D, buffer_texture.pTextureObject);//m_tex_col[0]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap_clamp);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap_clamp);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexImage2D(GL_TEXTURE_2D,0, colour_iformat, Width, Height,0,GL_RGBA, GL_UNSIGNED_INT, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer_texture.pTextureObject, 0);
		
		GLenum status= (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status!=GL_FRAMEBUFFER_COMPLETE)
			return false;
	}
	if(depth_iformat)
	{
		glGenTextures(1, &buffer_depth_texture.pTextureObject);
		glBindTexture(GL_TEXTURE_2D, buffer_depth_texture.pTextureObject);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_clamp);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_clamp);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		//glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexImage2D(GL_TEXTURE_2D, 0, depth_iformat, Width, Height, 0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT, NULL);

		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,buffer_depth_texture.pTextureObject,0);
		
		GLenum status= (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status!=GL_FRAMEBUFFER_COMPLETE)
			return false;
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
GL_ERROR_CHECK
	return true;
}

// In order to use a depth buffer, either
// InitDepth_RB or InitDepth_Tex needs to be called.
void FramebufferGL::InitDepth_RB(GLenum iformat)
{
	SetDepthFormat(iformat);
}

void FramebufferGL::NoDepth()
{
}

void FramebufferGL::InitDepth_Tex(GLenum iformat)
{
	SetDepthFormat(iformat);
}

void FramebufferGL::Activate(crossplatform::DeviceContext &)
{
	if(!m_fb)
		Init();
	CheckFramebufferStatus();
    glBindFramebuffer(GL_FRAMEBUFFER, m_fb); 
	GL_ERROR_CHECK
	CheckFramebufferStatus();
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	GL_ERROR_CHECK
	glViewport(0,0,Width,Height);
	GL_ERROR_CHECK
	fb_stack.push(m_fb);
}

void FramebufferGL::ActivateDepth(crossplatform::DeviceContext &)
{
}

void FramebufferGL::ActivateColour(crossplatform::DeviceContext &,const float /*viewportXYWH*/[4])
{
	SIMUL_THROW("ActivateColour does not yet work for FramebufferGL");
	if(!m_fb)
		Init();
	CheckFramebufferStatus();
    glBindFramebuffer(GL_FRAMEBUFFER, m_fb); 
	GL_ERROR_CHECK
	CheckFramebufferStatus();
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	GL_ERROR_CHECK
	glViewport(0,0,Width,Height);
	GL_ERROR_CHECK
	fb_stack.push(m_fb);
}
// Activate the FBO as a render target
// The FBO needs to be deactivated when using the associated Textures.

void FramebufferGL::ActivateViewport(crossplatform::DeviceContext &,float viewportX, float viewportY, float viewportW, float viewportH)
{
	if(!m_fb)
		Init();
	CheckFramebufferStatus();
    glBindFramebuffer(GL_FRAMEBUFFER, m_fb); 
	GL_ERROR_CHECK
	CheckFramebufferStatus();
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	GL_ERROR_CHECK
	glViewport((int)((float)Width*viewportX),(int)((float)Height*viewportY)
			,(int)((float)Width*viewportW),(int)((float)Height*viewportH));
	GL_ERROR_CHECK
	fb_stack.push(m_fb);
}

void FramebufferGL::Deactivate(void *) 
{
	GL_ERROR_CHECK
	//glFlush(); 
	GL_ERROR_CHECK
	CheckFramebufferStatus();
	GL_ERROR_CHECK
	// remove m_fb from the stack and...
	fb_stack.pop();
	// ..restore the n one down.
	GLuint last_fb=fb_stack.top();
	GL_ERROR_CHECK
    glBindFramebuffer(GL_FRAMEBUFFER,last_fb);
	GL_ERROR_CHECK
	glViewport(0,0,main_viewport[2],main_viewport[3]);
	GL_ERROR_CHECK
}
void FramebufferGL::CopyDepthFromFramebuffer() 
{
	glBindFramebuffer (GL_READ_FRAMEBUFFER, m_fb);
	GL_ERROR_CHECK
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, fb_stack.top());
	GL_ERROR_CHECK
	glBlitFramebuffer (0, 0, Width, Height, 0, 0, Width, Height,
			   GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	GL_ERROR_CHECK
	glBindFramebuffer (GL_FRAMEBUFFER, fb_stack.top());
	GL_ERROR_CHECK
}

void FramebufferGL::DeactivateAndRender(void *context,bool blend)
{
	GL_ERROR_CHECK
	Deactivate(context);
	Render(context,blend);
}

void FramebufferGL::Render(void *,bool blend)
{
GL_ERROR_CHECK
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	Ortho();
  
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
GL_ERROR_CHECK
	::DrawQuad(0,0,1,1);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
GL_ERROR_CHECK
	Release();
}


void FramebufferGL::Clear(void *context,float r,float g,float b,float a,float depth,int mask)
{
	if(!mask)
		mask=GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT;
	ClearColour(context,r,g,b,a);
	if(mask&GL_COLOR_BUFFER_BIT)
		 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	// AMAZINGLY, OpenGL requires depth mask to be set to clear the depth buffer.
	if(mask&GL_DEPTH_BUFFER_BIT)
		glDepthMask(GL_TRUE);
	glClearDepth(depth);
	glClear(mask);
}

void FramebufferGL::ClearColour(void*,float r,float g,float b,float a)
{
	glClearColor(r,g,b,a);
}

void FramebufferGL::CopyToMemory(void *,void * /*target*/,int /*start_texel*/,int /*num_texels*/)
{

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
			DebugBreak();
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr<<("Framebuffer incomplete attachment\n");
			DebugBreak();
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr<<("Framebuffer incomplete, missing attachment\n");
			DebugBreak();
            break;
    /*    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            std::cerr<<("Framebuffer incomplete, attached images must have same dimensions\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
            std::cerr<<("Framebuffer incomplete, attached images must have same format\n");
            break;*/
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            std::cerr<<("Framebuffer incomplete, missing draw buffer\n");
			DebugBreak();
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            std::cerr<<("Framebuffer incomplete, missing read buffer\n");
			DebugBreak();
            break;
        default:
			std::cerr<<"Unknown error "<<(int)status<<std::endl;
    }
}
