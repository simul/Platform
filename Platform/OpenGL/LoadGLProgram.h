#pragma once
extern void SetShaderPath(const char *path);
extern GLuint LoadProgram(GLuint prog,const char *filename,const char *defines=NULL);
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
