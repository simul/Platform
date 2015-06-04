
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
using namespace std;

namespace simul
{
	namespace opengl
	{
	}
}
static bool FileExists(const string& filename_utf8)
{
	FILE* pFile = NULL;
#ifdef _MSC_VER
	wstring wstr=simul::base::Utf8ToWString(filename_utf8.c_str());
	_wfopen_s(&pFile,wstr.c_str(),L"r");
#else
	pFile=fopen(filename_utf8.c_str(),"r");
#endif
	bool bExists = (pFile != NULL);
	if (pFile)
		fclose(pFile);
	return bExists;
}

unsigned char *LoadBitmap(const char *filename_utf8,unsigned &bpp,int &width,int &height)
{
ERRNO_CHECK
#ifdef _MSC_VER
	string fn			=filename_utf8;
	FREE_IMAGE_FORMAT fif	=FIF_UNKNOWN;
	GL_ERROR_CHECK
	wstring wstr		=simul::base::Utf8ToWString(fn);
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
		throw simul::base::RuntimeError(string("Can't determine bitmap type from filename: ")+string(filename_utf8));
	}
	GL_ERROR_CHECK
ERRNO_CHECK
	// ok, let's load the file
	FIBITMAP *dib = FreeImage_LoadU(fif,wstr.c_str());
	GL_ERROR_CHECK
	if(!dib)
	{
		SIMUL_CERR<<"Failed to load bitmap "<<filename_utf8<<std::endl;
		return 0;
	}
	GL_ERROR_CHECK

	width  = FreeImage_GetWidth(dib),
	height = FreeImage_GetHeight(dib);
	GL_ERROR_CHECK
	// We flip the image because we want texcoords to be zero at the top, to match HLSL,DirectX
	if(!FreeImage_FlipVertical(dib))
	{
		SIMUL_CERR<<"Failed to flip bitmap "<<filename_utf8<<std::endl;
	}
	bpp=FreeImage_GetBPP(dib);
	//if(bpp!=24)
	//	return 0;
    unsigned int byte_size = width * height*(bpp/8);
	BYTE *pixels = (BYTE*)FreeImage_GetBits(dib);
	unsigned char *data=new unsigned char[byte_size];
	memcpy_s(data,byte_size,pixels,byte_size);
    FreeImage_Unload(dib);
ERRNO_CHECK
	GL_ERROR_CHECK
	return data;
#else
	return NULL;
#endif
}

#include "Simul/Base/FileLoader.h"
GLuint LoadGLImage(const char *filename_utf8,const std::vector<std::string> &texturePathsUtf8,unsigned wrap,int *w,int *h,GLint *internal_format)
{
	GL_ERROR_CHECK
	string fn=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_utf8,texturePathsUtf8);
	GL_ERROR_CHECK
	if(!FileExists(fn.c_str()))
		return 0;
#ifdef _MSC_VER
	unsigned bpp=0;
	int width,height;
	BYTE *pixels=(BYTE*)LoadBitmap(fn.c_str(),bpp,width,height);
	if(!pixels)
		return 0;
	GL_ERROR_CHECK
	GLuint image_tex=0;
    glGenTextures(1,&image_tex);
    glBindTexture(GL_TEXTURE_2D,image_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_R,wrap);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
	GL_ERROR_CHECK
	*internal_format=GL_RGBA;
	GLint external_format=GL_RGBA;
	if(bpp==1||bpp==8)
	{
		*internal_format=GL_RGB8;
		external_format=GL_LUMINANCE;
	}
	else if(bpp==24)
	{
		*internal_format=GL_RGB8;
		external_format=GL_BGR;
	}
	else if(bpp==32)
	{
		*internal_format=GL_RGBA8;
		external_format=GL_RGBA;
	}
	else
		SIMUL_CERR<<"Unkown bits-per-pixel value: "<<bpp<<std::endl;
	glTexImage2D(GL_TEXTURE_2D,0,*internal_format,width,height,0,external_format,GL_UNSIGNED_BYTE,pixels);
	if(w)
		*w=width;
	if(h)
		*h=height;
	GL_ERROR_CHECK
	return image_tex;
#else
	return 0;
#endif
}

void SaveGLImage(const char *filename_utf8,GLuint tex)
{
	FREE_IMAGE_FORMAT fif	=FIF_UNKNOWN;
GL_ERROR_CHECK
	wstring wstr		=simul::base::Utf8ToWString(filename_utf8);
	fif						=FreeImage_GetFIFFromFilenameU(wstr.c_str());
	BYTE* pixels = NULL;
	int bytes_per_pixel=0;
	GLint width,height;
	{
		GLint internalFormat;
//		GLint targetFormat=GL_RGB;
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
#include "Simul/Base/FileLoader.h"
unsigned char *LoadGLBitmap(const char *filename_utf8,const std::vector<std::string> &pathsUtf8,unsigned &bpp,int &width,int &height)
{
ERRNO_CHECK
	string fn=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_utf8,pathsUtf8);
ERRNO_CHECK
	return LoadBitmap(fn.c_str(),bpp,width,height);
}