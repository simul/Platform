#include "GL/glew.h"
#include "Texture.h"
#include "LoadGLImage.h"

#include "targa.h"
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

// Load a texture file (TGA only now) into GPU and return the texture object name
void opengl::Texture::LoadFromFile(const char *pFilePathUtf8)
{
	std::string filename(pFilePathUtf8);
	unsigned dot_pos	=filename.find_last_of(".");
	std::string extension;
	pTextureObject		=0;
	if(dot_pos<filename.length())
		extension		=filename.substr(dot_pos+1,filename.length()-dot_pos-1);
	pTextureObject		=LoadGLImage(pFilePathUtf8,GL_REPEAT);
/*	if (extension.length()==3&&_stricmp(extension.c_str(),"TGA")==0)
	{
		tga_image lTGAImage;

		if (tga_read(&lTGAImage, pFilePathUtf8) == TGA_NOERR)
		{
			// Make sure the image is left to right
			if (tga_is_right_to_left(&lTGAImage))
				tga_flip_horiz(&lTGAImage);

			// Make sure the image is bottom to top
			if (tga_is_top_to_bottom(&lTGAImage))
				tga_flip_vert(&lTGAImage);

			// Make the image BGR 24
			tga_convert_depth(&lTGAImage, 24);

			// Transfer the texture date into GPU
			glGenTextures(1, &pTextureObject);
			glBindTexture(GL_TEXTURE_2D, pTextureObject);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glTexImage2D(GL_TEXTURE_2D, 0, 3, lTGAImage.width, lTGAImage.height, 0, GL_BGR,
				GL_UNSIGNED_BYTE, lTGAImage.image_data);
			glBindTexture(GL_TEXTURE_2D, 0);

			tga_free_buffers(&lTGAImage);
			return;
		}
	}*/
	return ;
}
