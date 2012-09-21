
#include <GL/glew.h>
#include <string>
#include "LoadGLImage.h"
#include "FreeImage.h"
#include "SimulGLUtilities.h"

static std::string image_path="";

namespace simul
{
	namespace opengl
	{
		void SetTexturePath(const char *path)
		{
			image_path=path;
		}
	}
}
unsigned char * LoadBitmap(const char *filename,unsigned &bpp,unsigned &width,unsigned &height)
{
	std::string fn=filename;
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	fif = FreeImage_GetFileType(fn.c_str(), 0);
	if(fif == FIF_UNKNOWN)
	{
		// no signature ?
		// try to guess the file format from the file extension
		fif = FreeImage_GetFIFFromFilename(fn.c_str());
	}
	// check that the plugin has reading capabilities ...
	if((fif == FIF_UNKNOWN) ||!FreeImage_FIFSupportsReading(fif))
		return 0;

	
		// ok, let's load the file
	FIBITMAP *dib = FreeImage_Load(fif,fn.c_str());
	
	width  = FreeImage_GetWidth(dib),
	height = FreeImage_GetHeight(dib);

	bpp=FreeImage_GetBPP(dib);
	//if(bpp!=24)
	//	return 0;
	BYTE *pixels = (BYTE*)FreeImage_GetBits(dib);
	return pixels;
}

GLuint LoadGLImage(const char *filename,unsigned wrap)
{
	std::string fn=image_path+"/";
	fn+=filename;
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
}

unsigned char *LoadGLBitmap(const char *filename,unsigned &bpp,unsigned &width,unsigned &height)
{
	std::string fn=image_path+"/";
	fn+=filename;
	return LoadBitmap(fn.c_str(),bpp,width,height);
}

void SaveGLImage(const char *filename,GLuint tex)
{
	std::string fn=image_path+"/";
	fn+=filename;
/*	FIBITMAP *bitmap2 = FreeImage_Allocate(w, h, 32);
	FreeImage_Paste(bitmap2, bitmap, 0, 0, 255);
    FreeImage_Save(FIF_PNG, bitmap2, "mybitmap.png", 0);*/
}