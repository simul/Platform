#pragma once
#ifdef _XBOX_ONE
#include "Simul\Platform\DirectX11\Texture.h"
#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		class ESRAMTexture :public Texture
		{
		public:
			ESRAMTexture();
			virtual ~ESRAMTexture();
			virtual void ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0) override;
		};
	}
}
#endif