#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include <vector>
#include <map>
#include <string>

namespace simul
{
	namespace opengl
	{
		extern SIMUL_OPENGL_EXPORT void printProgramInfoLog(GLuint obj);
		extern SIMUL_OPENGL_EXPORT void printShaderInfoLog(const std::string &info_log,const std::vector<std::string> &sourceFilesUtf8);

		extern SIMUL_OPENGL_EXPORT void printEffectLog(GLint effect);
	}
}
