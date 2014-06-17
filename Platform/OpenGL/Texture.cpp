#include "GL/glew.h"
#include "Texture.h"
#include "LoadGLImage.h"
#include "SimulGLUtilities.h"

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
	GL_ERROR_CHECK
}
// Load a texture file
void opengl::Texture::LoadFromFile(crossplatform::RenderPlatform *,const char *pFilePathUtf8)
{
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
	GLuint tex=0;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F_ARB,w,l,0,GL_RGBA,GL_FLOAT,NULL);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D,0);
}

void Texture::activateRenderTarget(simul::crossplatform::DeviceContext &deviceContext)
{
}

void Texture::deactivateRenderTarget()
{
}

int Texture::GetLength() const
{
	return 0;
}

int Texture::GetWidth() const
{
	return 0;
}

int Texture::GetSampleCount() const
{
	return 0;
}