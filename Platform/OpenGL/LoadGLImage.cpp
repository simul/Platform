
#include <GL/glew.h>
#include <string>
#include <vector>
#include "LoadGLImage.h"
#include "FreeImage.h"
#include "SimulGLUtilities.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"

#ifdef _MSC_VER
#pragma comment(lib,"freeImage.lib")
#endif


static std::string texturePathUtf8="";
static std::vector<std::string> fallbackTexturePathsUtf8;

namespace simul
{
	namespace opengl
	{
		void SetTexturePath(const char *path_utf8)
		{
			texturePathUtf8=path_utf8;
		}
		void PushTexturePath(const char *path_utf8)
		{
			fallbackTexturePathsUtf8.push_back(path_utf8);
		}
		void PopTexturePath()
		{ 
			fallbackTexturePathsUtf8.pop_back();
		}
	}
}
static bool FileExists(const std::string& filename_utf8)
{
	FILE* pFile = NULL;
#ifdef _MSC_VER
	std::wstring wstr=simul::base::Utf8ToWString(filename_utf8.c_str());
	_wfopen_s(&pFile,wstr.c_str(),L"r");
#else
	pFile=fopen(filename_utf8.c_str(),"r");
#endif
	bool bExists = (pFile != NULL);
	if (pFile)
		fclose(pFile);
	return bExists;
}

unsigned char *LoadBitmap(const char *filename_utf8,unsigned &bpp,unsigned &width,unsigned &height)
{
#ifdef _MSC_VER
	std::string fn			=filename_utf8;
	FREE_IMAGE_FORMAT fif	=FIF_UNKNOWN;

	std::wstring wstr		=simul::base::Utf8ToWString(fn);
	fif						=FreeImage_GetFileTypeU(wstr.c_str(), 0);
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

GLuint LoadTexture(const char *filename_utf8,unsigned wrap)
{
#ifdef _MSC_VER
	unsigned bpp=0;
	unsigned width,height;
	BYTE *pixels=(BYTE*)LoadBitmap(filename_utf8,bpp,width,height);
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

GLuint LoadGLImage(const char *filename_utf8,unsigned wrap)
{
	std::string fn=texturePathUtf8+"/";
	fn+=filename_utf8;
	// Is it an absolute path? If so use the given file name.
	// Otherwise use the relative path relative to the texture path.
	// Failing that, use the bare filename relative to any of the paths on the stack
	if(!FileExists(fn.c_str()))
	{
		std::string name_only_utf8=filename_utf8;
		// Try the file in different directories.
		// First, if the path is relative, try appending the relative path to each directory. If this fails, we will use just the filename.
		if(name_only_utf8.find(":")>=name_only_utf8.length())
		{
			for(int i=0;i<(int)fallbackTexturePathsUtf8.size();i++)
			{
				fn=fallbackTexturePathsUtf8[i]+"/";
				fn+=filename_utf8;
				if(FileExists(fn.c_str()))
					break;
			}
		}
		if(!FileExists(fn.c_str()))
		{
			int slash=(int)name_only_utf8.find_last_of("/");
			slash=std::max(slash,(int)name_only_utf8.find_last_of("\\"));
			if(slash>0)
				name_only_utf8=name_only_utf8.substr(slash+1,name_only_utf8.length()-slash-1);
			for(int i=0;i<(int)fallbackTexturePathsUtf8.size();i++)
			{
				fn=fallbackTexturePathsUtf8[i]+"/";
				fn+=name_only_utf8;
				if(FileExists(fn.c_str()))
					break;
			}
		}
	}
	if(!FileExists(fn.c_str()))
		return 0;
	return LoadTexture(fn.c_str(),wrap);
}

void SaveGLImage(const char *filename_utf8,GLuint tex)
{
	FREE_IMAGE_FORMAT fif	=FIF_UNKNOWN;
GL_ERROR_CHECK
	std::wstring wstr		=simul::base::Utf8ToWString(filename_utf8);
	fif						=FreeImage_GetFIFFromFilenameU(wstr.c_str());
	BYTE* pixels = NULL;
	int bytes_per_pixel=0;
	GLint width,height;
	{
		GLint internalFormat;
		GLint targetFormat=GL_RGB;
		glBindTexture(GL_TEXTURE_2D, tex);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPONENTS, &internalFormat); // get internal format type of GL texture
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width); // get width of GL texture
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height); // get height of GL texture
GL_ERROR_CHECK
		// GL_TEXTURE_COMPONENTS and GL_INTERNAL_FORMAT are the same.
		// just work with RGB8 and RGBA8
		GLint numBytes = 0;
		GLenum type=GL_FLOAT;
		switch(internalFormat)
		{
		case GL_RGB:
			numBytes = width * height * 3;
			type=GL_UNSIGNED_BYTE;
			bytes_per_pixel=24;
		break;
		case GL_RGBA:
			numBytes = width * height * 4;
			type=GL_UNSIGNED_BYTE;
			bytes_per_pixel=24;
		break;
		case GL_RGBA32F:
			numBytes = width * height * 4*sizeof(float);
			type=GL_FLOAT;
			bytes_per_pixel=4*4*sizeof(float);
			bytes_per_pixel=24;
		break;
		case GL_RGB32F:
			numBytes = width * height * 3*sizeof(float);
			type=GL_FLOAT;
			bytes_per_pixel=4*3*sizeof(float);
			bytes_per_pixel=24;
			break;
		default: // unsupported type
		break;
		}
		if(numBytes)
		{
			GL_ERROR_CHECK
			pixels = (unsigned char*)malloc(numBytes); // allocate image data into RAM
			glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, pixels);
	GL_ERROR_CHECK
		}
	}
	FIBITMAP* image = FreeImage_ConvertFromRawBits(pixels, width, height, 3 * width, bytes_per_pixel, 0x00FF00, 0xFF0000,0x0000FF , false);

	FreeImage_SaveU(fif,image, wstr.c_str());
	FreeImage_Unload(image);
	delete [] pixels;
}

unsigned char *LoadGLBitmap(const char *filename_utf8,unsigned &bpp,unsigned &width,unsigned &height)
{
	std::string fn=texturePathUtf8+"/";
	fn+=filename_utf8;
	return LoadBitmap(fn.c_str(),bpp,width,height);
}