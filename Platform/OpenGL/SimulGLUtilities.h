#ifndef SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H
#define SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H

#include "Simul/Platform/OpenGL/Export.h"
#include <assert.h>


extern void SIMUL_OPENGL_EXPORT CheckExtension(const char *txt);



extern SIMUL_OPENGL_EXPORT void SetOrthoProjection(int w,int h);
extern SIMUL_OPENGL_EXPORT void SetPerspectiveProjection(int w,int h,float field_of_view);
extern SIMUL_OPENGL_EXPORT void RenderString(float x, float y, void *font, const char* string);
extern SIMUL_OPENGL_EXPORT void SetVSync(int vsync);
extern SIMUL_OPENGL_EXPORT void DrawQuad(int w,int h);
extern SIMUL_OPENGL_EXPORT float GetFramerate();
extern void CheckGLError();
#define ERROR_CHECK CheckGLError();
#endif