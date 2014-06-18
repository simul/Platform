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
			void LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			bool IsValid() const;
			void InvalidateDeviceObjects();
			ID3D11ShaderResourceView *AsD3D11ShaderResourceView()
			{
				return NULL;
			}
			GLuint AsGLuint()
			{
				return pTextureObject;
			}
			void ensureTexture2DSizeAndFormat(simul::crossplatform::RenderPlatform *renderPlatform,int w,int l
				,unsigned f,bool computable=false,bool rendertarget=false,int num_samples=1,int aa_quality=0);
			void activateRenderTarget(simul::crossplatform::DeviceContext &deviceContext);
			void deactivateRenderTarget();
			int GetLength() const;
			int GetWidth() const;
			int GetDimension() const;
			int GetSampleCount() const;
			GLuint pTextureObject;

			// Former Texture functions
			void setTexels(void *,const void *src,int x,int y,int z,int w,int l,int d);
			void ensureTexture3DSizeAndFormat(void *,int w,int l,int d,int frmt,bool computable=false);
		};
	}
}
