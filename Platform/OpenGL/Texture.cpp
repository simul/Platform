#include "GL/glew.h"
#include "Texture.h"
#include "LoadGLImage.h"

#include <string>

using namespace simul;
#if defined(_MSC_VER) && (defined(WIN32) || defined(WIN64))
#else
#include <stdio.h>
#include <strings.h>
#define _stricmp strcasecmp
#endif

opengl::Texture::Texture()
{
}

opengl::Texture::~Texture()
{
	glDeleteTextures(1,&pTextureObject);
}

// Load a texture file
void opengl::Texture::LoadFromFile(const char *pFilePathUtf8)
{
	std::string filename(pFilePathUtf8);
	int dot_pos	=(int)filename.find_last_of(".");
	std::string extension;
	pTextureObject		=0;
	if(dot_pos>=0&&dot_pos<(int)filename.length())
		extension		=filename.substr(dot_pos+1,filename.length()-dot_pos-1);
	pTextureObject		=LoadGLImage(pFilePathUtf8,GL_REPEAT);
	return ;
}
