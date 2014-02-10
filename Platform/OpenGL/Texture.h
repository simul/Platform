#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/Timer.h"
#include "Simul/Scene/Texture.h"
#include <string>
#include <map>
#pragma warning(disable:4251)
namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT Texture:public simul::scene::Texture
		{
		public:
			Texture();
			~Texture();
			void LoadFromFile(const char *pFilePathUtf8);
			GLuint pTextureObject;
		};
	}
}
