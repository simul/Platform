
#include <GL/glew.h>
#include <string>
#include "LoadGLImage.h"
#include "FreeImage.h"
#include "SimulGLUtilities.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"

static std::string texturePathUtf8="";

namespace simul
{
	namespace opengl
	{
		void SetTexturePath(const char *path_utf8)
		{
			texturePathUtf8=path_utf8;
		}
	}
}
unsigned char * LoadBitmap(const char *filename_utf8,unsigned &bpp,unsigned &width,unsigned &height)
{
#ifdef WIN32
	std::string fn=filename_utf8;
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	std::wstring wstr=simul::base::Utf8ToWString(fn);
	fif = FreeImage_GetFileTypeU(wstr.c_str(), 0);
	if(fif == FIF_UNKNOWN)
	{
		// no signature ?
		// try to guess the file format from the file extension
		fif = FreeImage_GetFIFFromFilenameU(wstr.c_str());
	}
	// check that the plugin has reading capabilities ...
	if((fif == FIF_UNKNOWN) ||!FreeImage_FIFSupportsReading(fif))
	{
		throw simul::base::RuntimeError(std::string("Can't determine bitmap type from filename: ")+std::string(filename_utf8));
	}
	// ok, let's load the file
	FIBITMAP *dib = FreeImage_LoadU(fif,wstr.c_str());
	if(!dib)
	{
		throw simul::base::RuntimeError(std::string("Failed to load bitmap ")+std::string(filename_utf8));
	}

	width  = FreeImage_GetWidth(dib),
	height = FreeImage_GetHeight(dib);

	bpp=FreeImage_GetBPP(dib);
	//if(bpp!=24)
	//	return 0;
	BYTE *pixels = (BYTE*)FreeImage_GetBits(dib);
	return pixels;
#else
	return NULL;
#endif
}

GLuint LoadGLImage(const char *filename_utf8,unsigned wrap)
{
#ifdef WIN32
	std::string fn=texturePathUtf8+"/";
	fn+=filename_utf8;
	unsigned bpp=0;
	unsigned width,height;
	BYTE *pixels=(BYTE*)LoadBitmap(fn.c_str(),bpp,width,height);
	GLuint image_tex=0;
    glGenTextures(1,&image_tex);
    glBindTexture(GL_TEXTURE_2D,image_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_R,wrap);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
	if(bpp==24)
		glTexImage2D(GL_TEXTURE_2D,0, GL_RGB8,width,height,0,GL_RGB,GL_UNSIGNED_BYTE,pixels);
	if(bpp==32)
		glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
	return image_tex;
#else
	return 0;
#endif
}

unsigned char *LoadGLBitmap(const char *filename_utf8,unsigned &bpp,unsigned &width,unsigned &height)
{
	std::string fn=texturePathUtf8+"/";
	fn+=filename_utf8;
	return LoadBitmap(fn.c_str(),bpp,width,height);
}