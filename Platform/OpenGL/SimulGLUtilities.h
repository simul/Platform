#ifndef SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H
#define SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H

#include "Simul/Platform/OpenGL/Export.h"
#include <assert.h>
//! A wrapper around glewIsSupported() that checks for errors.
extern void SIMUL_OPENGL_EXPORT CheckExtension(const char *txt);
//! A wrapper around glOrtho() that also resets the GL matrices, and stores window height for later use.
extern SIMUL_OPENGL_EXPORT void SetOrthoProjection(int w,int h);
//! A wrapper around gluPerspective() that also stores window height for later use.
extern SIMUL_OPENGL_EXPORT void SetPerspectiveProjection(int w,int h,float field_of_view);
//! Print the given string at position x,y onscreen (use sparingly, not very fast).
extern SIMUL_OPENGL_EXPORT void RenderString(float x, float y, void *font, const char* string);
//! Controls vertical syncing - 0 means do not sync, 1 means sync with the monitor, 2 means sync at half speed, etc.
extern SIMUL_OPENGL_EXPORT void SetVSync(int vsync);
//! Draw a simple onscreen quad from 0,0 to w,h.
extern SIMUL_OPENGL_EXPORT void DrawQuad(int w,int h);
//! Get the current framerate - must be called once every frame to work correctly.
extern SIMUL_OPENGL_EXPORT float GetFramerate();
//! Check for a GL error, and halt the program if found.
extern void CheckGLError();
//! Check the given error code, and halt the program if it is non-zero.
extern void CheckGLError(int err);
#define ERROR_CHECK CheckGLError();
#define SAFE_DELETE_PROGRAM(prog)	glDeleteProgram(prog);prog=0;
#define SAFE_DELETE_TEXTURE(tex)	glDeleteTextures(1,&tex);tex=0;
#endif