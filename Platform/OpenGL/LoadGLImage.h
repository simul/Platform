#pragma once

#include "Simul/Platform/OpenGL/Export.h"

namespace simul
{
	namespace opengl
	{
		extern SIMUL_OPENGL_EXPORT void SetTexturePath(const char *path);
	}
}
extern SIMUL_OPENGL_EXPORT GLuint LoadGLImage(const char *filename,unsigned wrap=GL_CLAMP);
