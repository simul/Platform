#ifndef SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H
#define SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H

#include "Simul/Platform/OpenGL/Export.h"
#include <assert.h>

namespace simul
{
	namespace sky
	{
		struct float4;
	}
}

SIMUL_OPENGL_EXPORT_CLASS Utilities
{
	static int instance_count;
	static Utilities *ut;
	static int screen_width;
	static int screen_height;
	Utilities();
	~Utilities();
public:
	static Utilities &GetSingleton();
	void RestoreDeviceObjects(void *);
	void SetScreenSize(int w,int h);
	void InvalidateDeviceObjects();
	static void Kill();
	GLuint linedraw_program;
	GLuint simple_program;
};

extern void RenderTexture(int x,int y,int w,int h);
//! A wrapper around glewIsSupported() that checks for errors.
extern bool SIMUL_OPENGL_EXPORT CheckExtension(const char *txt);

extern bool IsExtensionSupported(const char *name);
extern SIMUL_OPENGL_EXPORT void Ortho();
//! A wrapper around glOrtho() that also resets the GL matrices, and stores window height for later use.
extern SIMUL_OPENGL_EXPORT void SetOrthoProjection(int w,int h);
extern SIMUL_OPENGL_EXPORT void SetTopDownOrthoProjection(int w,int h);
//! A wrapper around gluPerspective() that also stores window height for later use.
extern SIMUL_OPENGL_EXPORT void SetPerspectiveProjection(int w,int h,float field_of_view);
//! Print the given string at position x,y onscreen (use sparingly, not very fast).
extern SIMUL_OPENGL_EXPORT void RenderString(float x, float y, void *font, const char* string);
//! Controls vertical syncing - 0 means do not sync, 1 means sync with the monitor, 2 means sync at half speed, etc.
extern SIMUL_OPENGL_EXPORT void SetVSync(int vsync);
extern SIMUL_OPENGL_EXPORT void DrawQuad(int x,int y,int w,int h);
//! Draw a simple onscreen quad from 0,0 to w,h.
extern SIMUL_OPENGL_EXPORT void DrawQuad(float x,float y,float w,float h);
//! Get the current framerate - must be called once every frame to work correctly.
extern SIMUL_OPENGL_EXPORT float GetFramerate();
//! Check for a GL error, and halt the program if found.
extern SIMUL_OPENGL_EXPORT void CheckGLError(const char *filename,int line_number);
//! Check the given error code, and halt the program if it is non-zero.
extern SIMUL_OPENGL_EXPORT void CheckGLError(const char *filename,int line_number,int err);
#define ERROR_CHECK CheckGLError(__FILE__,__LINE__);
#define SAFE_DELETE_PROGRAM(prog)		if(prog){GLuint shaders[2];GLsizei count;glGetAttachedShaders(prog,2,&count,shaders);for(int i=0;i<count;i++) glDeleteShader(shaders[i]); glDeleteProgram(prog);prog=0;}
#define SAFE_DELETE_TEXTURE(tex)		if(tex) glDeleteTextures(1,&tex);tex=0;
#define SAFE_DELETE_FRAMEBUFFER(fb)		if(fb) glDeleteFramebuffers(1,&fb);fb=0;
#define SAFE_DELETE_RENDERBUFFER(rb)	if(rb) glDeleteRenderbuffers(1,&rb);rb=0;
extern SIMUL_OPENGL_EXPORT bool RenderAngledQuad(const float *dir,float half_angle_radians);
extern SIMUL_OPENGL_EXPORT void PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);

extern SIMUL_OPENGL_EXPORT void DrawLines(struct VertexXyzRgba *lines,int vertex_count,bool strip);
extern SIMUL_OPENGL_EXPORT void CalcCameraPosition(float *cam_pos,float *cam_dir=0);
extern SIMUL_OPENGL_EXPORT void FixGlProjectionMatrix(float required_distance);
extern SIMUL_OPENGL_EXPORT void OrthoMatrices();

extern void setParameter(GLuint program,const char *name,float value);
extern void setParameter(GLuint program,const char *name,float value1,float value2);
extern void setParameter(GLuint program,const char *name,float value1,float value2,float value3);
extern void setParameter(GLuint program,const char *name,int value);
extern void setParameter(GLuint program,const char *name,const simul::sky::float4 &value);
extern void setParameter2(GLuint program,const char *name,const simul::sky::float4 &value);
extern void setParameter3(GLuint program,const char *name,const simul::sky::float4 &value);
extern void setMatrix(GLuint program,const char *name,const float *value);
extern void setMatrixTranspose(GLuint program,const char *name,const float *value);
extern void setTexture(GLuint program,const char *name,int texture_number,GLuint texture);

extern void setParameter(GLint,int value);
extern void setParameter(GLint,float value);
extern void setParameter2(GLint,const simul::sky::float4 &value);
extern void setParameter3(GLint,const simul::sky::float4 &value);
extern void setParameter(GLint loc,const float *value);

#endif
