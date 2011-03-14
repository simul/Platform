#ifndef SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H
#define SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H
extern void SetOrthoProjection(int w,int h);
extern void SetPerspectiveProjection(int w,int h,float field_of_view);
extern void RenderString(float x, float y, void *font, const char* string);
extern void SetVSync(int vsync);
extern void DrawQuad(int w,int h);
extern float GetFramerate();
#endif