#pragma once
extern void SetShaderPath(const char *path);
extern GLuint LoadProgram(GLuint prog,const char *filename,const char *defines=NULL);
extern void printProgramInfoLog(GLuint obj);
#include "LoadGLImage.h"