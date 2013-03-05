#ifndef LOADGLIMAGE_H
#define LOADGLIMAGE_H

#include "Simul/Platform/OpenGL/Export.h"

namespace simul
{
	namespace opengl
	{
		extern SIMUL_OPENGL_EXPORT void SetTexturePath(const char *path);
	}
}
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
// Load a texture from the predefined texture path.
extern SIMUL_OPENGL_EXPORT GLuint LoadGLImage(const char *filename,unsigned wrap=GL_CLAMP_TO_EDGE);
extern SIMUL_OPENGL_EXPORT void SaveGLImage(const char *filename,GLuint tex);
extern SIMUL_OPENGL_EXPORT unsigned char *LoadGLBitmap(const char *filename,unsigned &bpp,unsigned &width,unsigned &height);
#endif