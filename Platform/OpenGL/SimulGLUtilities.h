#ifndef SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H
#define SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H

#include "Simul/Platform/OpenGL/Export.h"
#include <assert.h>
#include <GL/glew.h>

SIMUL_OPENGL_EXPORT_CLASS Utilities
{
	static int instance_count;
public:
	Utilities();
	~Utilities();
	static void RestoreDeviceObjects(void *);
	static void SetScreenSize(int w,int h);
	static void InvalidateDeviceObjects();
	static int screen_width;
	static int screen_height;
	static GLuint linedraw_program;
};

//! A wrapper around glewIsSupported() that checks for errors.
extern bool SIMUL_OPENGL_EXPORT CheckExtension(const char *txt);

extern bool IsExtensionSupported(const char *name);
//! A wrapper around glOrtho() that also resets the GL matrices, and stores window height for later use.
extern SIMUL_OPENGL_EXPORT void SetOrthoProjection(int w,int h);
extern SIMUL_OPENGL_EXPORT void SetTopDownOrthoProjection(int w,int h);
//! A wrapper around gluPerspective() that also stores window height for later use.
extern SIMUL_OPENGL_EXPORT void SetPerspectiveProjection(int w,int h,float field_of_view);
//! Print the given string at position x,y onscreen (use sparingly, not very fast).
extern SIMUL_OPENGL_EXPORT void RenderString(float x, float y, void *font, const char* string);
//! Controls vertical syncing - 0 means do not sync, 1 means sync with the monitor, 2 means sync at half speed, etc.
extern SIMUL_OPENGL_EXPORT void SetVSync(int vsync);
//! Draw a simple onscreen quad from 0,0 to w,h.
extern SIMUL_OPENGL_EXPORT void DrawQuad(int x,int y,int w,int h);
//! Get the current framerate - must be called once every frame to work correctly.
extern SIMUL_OPENGL_EXPORT float GetFramerate();
//! Check for a GL error, and halt the program if found.
extern SIMUL_OPENGL_EXPORT void CheckGLError(const char *filename,int line_number);
//! Check the given error code, and halt the program if it is non-zero.
extern SIMUL_OPENGL_EXPORT void CheckGLError(const char *filename,int line_number,int err);
#define ERROR_CHECK CheckGLError(__FILE__,__LINE__);
#define SAFE_DELETE_PROGRAM(prog)	if(prog){GLuint shaders[2];GLsizei count;glGetAttachedShaders(prog,2,&count,shaders);for(int i=0;i<count;i++) glDeleteShader(shaders[i]); glDeleteProgram(prog);prog=0;}
#define SAFE_DELETE_TEXTURE(tex)	glDeleteTextures(1,&tex);tex=0;
extern SIMUL_OPENGL_EXPORT bool RenderAngledQuad(const float *dir,float half_angle_radians);
extern SIMUL_OPENGL_EXPORT void PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);

struct VertexXyzRgba
{
	float x,y,z;
	float r,g,b,a;
};
extern SIMUL_OPENGL_EXPORT void DrawLines(VertexXyzRgba *lines,int vertex_count,bool strip);
extern void CalcCameraPosition(float *cam_pos,float *cam_dir=0);
extern SIMUL_OPENGL_EXPORT void FixGlProjectionMatrix(float required_distance);
#endif
