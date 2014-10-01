#ifndef LOADGLIMAGE_H
#define LOADGLIMAGE_H

#include "Simul/Platform/OpenGL/Export.h"

namespace simul
{
	namespace opengl
	{
		extern SIMUL_OPENGL_EXPORT void PushTexturePath(const char *path_utf8);
		extern SIMUL_OPENGL_EXPORT void PopTexturePath();
	}
}
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
// Load a texture from the predefined texture path.
extern SIMUL_OPENGL_EXPORT GLuint LoadTexture(const char *filename_utf8,unsigned wrap=GL_CLAMP_TO_EDGE);
extern SIMUL_OPENGL_EXPORT GLuint LoadGLImage(const char *filename_utf8,unsigned wrap=GL_CLAMP_TO_EDGE,int *w=NULL,int *h=NULL);
extern SIMUL_OPENGL_EXPORT void SaveGLImage(const char *filename_utf8,GLuint tex);
extern SIMUL_OPENGL_EXPORT unsigned char *LoadGLBitmap(const char *filename_utf8,unsigned &bpp,unsigned &width,unsigned &height);
#endif