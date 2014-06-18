#include "GL/glew.h"
#include "Texture.h"
#include "LoadGLImage.h"
#include "SimulGLUtilities.h"
#include "FramebufferGL.h"
#include <string>

using namespace simul;
using namespace opengl;
#if defined(_MSC_VER) && (defined(WIN32) || defined(WIN64))
#else
#include <stdio.h>
#include <strings.h>
#define _stricmp strcasecmp
#endif

opengl::Texture::Texture()
	:pTextureObject(0)
	,m_fb(0)
{
}

opengl::Texture::~Texture()
{
	InvalidateDeviceObjects();
}

void Texture::InvalidateDeviceObjects()
{
	GL_ERROR_CHECK
	SAFE_DELETE_TEXTURE(pTextureObject);
	SAFE_DELETE_FRAMEBUFFER(m_fb);
	GL_ERROR_CHECK
}
// Load a texture file
void opengl::Texture::LoadFromFile(crossplatform::RenderPlatform *,const char *pFilePathUtf8)
{
	dim=2;
	std::string filename(pFilePathUtf8);
	int dot_pos			=(int)filename.find_last_of(".");
	std::string extension;
	pTextureObject		=0;
	if(dot_pos>=0&&dot_pos<(int)filename.length())
		extension		=filename.substr(dot_pos+1,filename.length()-dot_pos-1);
	pTextureObject		=LoadGLImage(pFilePathUtf8,GL_REPEAT);
	return ;
}

bool opengl::Texture::IsValid() const
{
	return (pTextureObject>0);
}

void Texture::ensureTexture2DSizeAndFormat(simul::crossplatform::RenderPlatform *renderPlatform,int w,int l
	,unsigned f,bool computable,bool rendertarget,int num_samples,int aa_quality)
{
	width=w;
	length=l;
	dim=2;
	glGenTextures(1,&pTextureObject);
	glBindTexture(GL_TEXTURE_2D,pTextureObject);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,w,l,0,GL_RGBA,GL_UNSIGNED_INT,NULL);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

	if(rendertarget)
	{
		SAFE_DELETE_FRAMEBUFFER(m_fb);
		glGenFramebuffers(1, &m_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pTextureObject, 0);
		
		GLenum status= (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if(status!=GL_FRAMEBUFFER_COMPLETE)
		{
			FramebufferGL::CheckFramebufferStatus();
			throw base::RuntimeError("Framebuffer incomplete for rendertarget texture");
		}
	}
	glBindTexture(GL_TEXTURE_2D,0);
}
void Texture::activateRenderTarget(simul::crossplatform::DeviceContext &deviceContext)
{
	if(!m_fb)
		return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fb); 
	GL_ERROR_CHECK
	FramebufferGL::CheckFramebufferStatus();
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	GL_ERROR_CHECK
	glViewport(0,0,width,length);
	GL_ERROR_CHECK
	FramebufferGL::fb_stack.push(m_fb);
}

void Texture::deactivateRenderTarget()
{
	//glFlush(); 
	GL_ERROR_CHECK
	FramebufferGL::CheckFramebufferStatus();
	GL_ERROR_CHECK
	// remove m_fb from the stack and...
	FramebufferGL::fb_stack.pop();
	// ..restore the n one down.
	GLuint last_fb=FramebufferGL::fb_stack.top();
	GL_ERROR_CHECK
    glBindFramebuffer(GL_FRAMEBUFFER,last_fb);
	GL_ERROR_CHECK
	glViewport(0,0,main_viewport[2],main_viewport[3]);
	GL_ERROR_CHECK
}

int Texture::GetLength() const
{
	return length;
}

int Texture::GetWidth() const
{
	return width;
}

int Texture::GetSampleCount() const
{
	return 0;
}

int Texture::GetDimension() const
{
	return dim;
}



void simul::opengl::Texture::setTexels(void *,const void *src,int x,int y,int z,int w,int l,int d)
{
	glTexSubImage3D(	GL_TEXTURE_3D,0,
						x,y,z,
						w,l,d,
						GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
						src);
}

void simul::opengl::Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *,int w,int l,int d,int frmt,bool /*computable*/,int mips)
{
	dim=3;
	width=w;
	length=l;
	depth=d;
	if(pTextureObject)
	{
		int W,L,D;
		glBindTexture(GL_TEXTURE_3D,pTextureObject);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_WIDTH,&W);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_HEIGHT,&L);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_DEPTH,&D);
		if(w!=W||l!=L||d!=D)
		{
			SAFE_DELETE_TEXTURE(pTextureObject);
		}
	}
	if(!pTextureObject)
	{
		glGenTextures(1,&(pTextureObject));
		glBindTexture(GL_TEXTURE_3D,pTextureObject);
		GLenum number_format=GL_RGBA;
		GLenum number_type	=GL_UNSIGNED_INT;
		switch(frmt)
		{
		case GL_RGBA:
			number_format	=GL_RGBA;
			number_type		=GL_UNSIGNED_INT;
			break;
		case GL_RGBA32F:
			number_format	=GL_RGBA;
			number_type		=GL_FLOAT;
			break;
		case GL_LUMINANCE32F_ARB:
			number_format	=GL_LUMINANCE;
			number_type		=GL_FLOAT;
			break;
		//((frmt==GL_RGBA)?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT)
		default:
			break;
		};
		glTexImage3D(GL_TEXTURE_3D,0,(GLint)frmt,w,l,d,0,number_format,number_type,0);
	GL_ERROR_CHECK
	//	glTexImage3D(GL_TEXTURE_3D,0,GL_LUMINANCE32F_ARB:GL_RGBA32F_ARB,w,l,d,0,GL_LUMINANCE:GL_RGBA,GL_FLOAT,src);
		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	}
}

void Texture::copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels)
{
}
