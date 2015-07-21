#ifndef LOADGLIMAGE_H
#define LOADGLIMAGE_H

#include <vector>
#include <string>
#include "Simul/Platform/OpenGL/Export.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
extern SIMUL_OPENGL_EXPORT GLuint LoadGLImage(const char *filename_utf8,const std::vector<std::string> &pathsUtf8,unsigned wrap=GL_CLAMP_TO_EDGE,int *w=NULL,int *h=NULL,GLint *internal_format=0);
extern SIMUL_OPENGL_EXPORT void SaveGLImage(const char *filename_utf8,GLuint tex,bool flip_vertical);
extern SIMUL_OPENGL_EXPORT unsigned char *LoadGLBitmap(const char *filename_utf8,const std::vector<std::string> &pathsUtf8,unsigned &bpp,int &width,int &height);
#endif