#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		public:
			virtual ~Texture();
			virtual void LoadFromFile(const char *pFilePathUtf8)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual void *AsVoidPointer()=0;
			virtual void ensureTexture2DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l
				,unsigned f,bool computable=false,bool rendertarget=false,int num_samples=1,int aa_quality=0)=0;
			virtual void activateRenderTarget(DeviceContext &deviceContext)=0;
			virtual void deactivateRenderTarget()=0;
			virtual int GetLength() const=0;
			virtual int GetWidth() const=0;
			
		};
	}
}
