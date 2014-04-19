#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include <vector>
#include <map>
#include <string>

namespace simul
{
	namespace opengl
	{
		extern SIMUL_OPENGL_EXPORT void PushShaderPath(const char *path_utf8);
		extern SIMUL_OPENGL_EXPORT GLint CreateEffect(const char *filename,const std::map<std::string,std::string>&defines);

		extern SIMUL_OPENGL_EXPORT GLuint MakeProgram(const char *root_filename);
		extern SIMUL_OPENGL_EXPORT GLuint MakeProgram(const char *root_filename,const std::map<std::string,std::string> &defines);
		extern SIMUL_OPENGL_EXPORT GLuint MakeProgramWithGS(const char *filename);
		extern SIMUL_OPENGL_EXPORT GLuint MakeProgramWithGS(const char *filename,const std::map<std::string,std::string> &defines);
		extern SIMUL_OPENGL_EXPORT GLuint SetShaders(const char *vert_src,const char *frag_src);
		extern SIMUL_OPENGL_EXPORT GLuint SetShaders(const char *vert_src,const char *frag_src,const std::map<std::string,std::string> &defines);
		extern SIMUL_OPENGL_EXPORT GLuint MakeProgram(const char *vert_filename,const char *geom_filename,const char *frag_filename);
		extern SIMUL_OPENGL_EXPORT GLuint MakeProgram(const char *vert_filename,const char *geom_filename,const char *frag_filename,const std::map<std::string,std::string> &defines);

		extern SIMUL_OPENGL_EXPORT GLuint LoadShader(const char *filename,const std::map<std::string,std::string> &defines);

		extern SIMUL_OPENGL_EXPORT void printProgramInfoLog(GLuint obj);
	}
}
