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
			void ensureTexture2DSizeAndFormat(simul::crossplatform::RenderPlatform *renderPlatform,int w,int l
				,unsigned f,bool computable=false,bool rendertarget=false,int num_samples=1,int aa_quality=0);
			void activateRenderTarget(simul::crossplatform::DeviceContext &deviceContext);
			void deactivateRenderTarget();
			int GetLength() const;
			int GetWidth() const;
			int GetSampleCount() const;
			GLuint pTextureObject;
		};
	}
}
