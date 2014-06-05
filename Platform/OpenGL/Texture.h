#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include <string>
#include <map>
#pragma warning(disable:4251)
namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT Texture:public simul::crossplatform::Texture
		{
		public:
			Texture();
			~Texture();
			void LoadFromFile(const char *pFilePathUtf8);
			bool IsValid() const;
			void InvalidateDeviceObjects();
			void *AsVoidPointer()
			{
				return (void*)pTextureObject;
			}
			GLuint pTextureObject;
		};
	}
}
