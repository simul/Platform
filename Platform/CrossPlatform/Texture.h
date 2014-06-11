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
			//! If the texture is multisampled, this returns the samples per texel. Zero means it is not an MS texture,
			//! while 1 means it is MS, even though the sample count is unity.
			virtual int GetSampleCount() const=0;
			
		};
	}
}
