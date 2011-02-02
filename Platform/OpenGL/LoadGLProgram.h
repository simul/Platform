#pragma once
extern void SetShaderPath(const char *path);
extern GLuint LoadProgram(GLuint prog,const char *filename,const char *defines=0);
extern void printProgramInfoLog(GLuint obj);
#ifdef SIMULWEATHER_X_PLANE
#ifdef _MSC_VER
	#define IBM 1
#else
	#define LIN 1
#endif
#include "XPLMGraphics.h"
#define glGenTextures(a,b) XPLMGenerateTextureNumbers((int*)b,(int)a)
#endif

#define ERROR_CHECK \
	if(int err=glGetError()!=0) \
	{ \
		const char *c=(const char*)gluErrorString(err); \
		if(c) std::cerr<<std::endl<<c<<std::endl; else std::cerr<<std::endl<<"unknown error: "<<err<<std::endl;\
	}