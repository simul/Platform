#ifdef _XBOX_ONE
#include "ESRAMTexture.h"
#include <xg.h>
#include <algorithm>
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/ESRAMManager.h"

#include <string>

using namespace simul;
using namespace dx11;
extern ESRAMManager *eSRAMManager;

template< typename t_A, typename t_B >
t_A RoundUpToNextMultiple( const t_A& a, const t_B& b )
{
    return ( ( a - 1 ) / b + 1 ) * b;
}

XG_FORMAT ToXgFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case R_16_FLOAT:
		return XG_FORMAT_R16_FLOAT;
	case RGBA_16_FLOAT:
		return XG_FORMAT_R16G16B16A16_FLOAT;
	case RGBA_32_FLOAT:
		return XG_FORMAT_R32G32B32A32_FLOAT;
	case RGB_32_FLOAT:
		return XG_FORMAT_R32G32B32_FLOAT;
	case RG_32_FLOAT:
		return XG_FORMAT_R32G32_FLOAT;
	case R_32_FLOAT:
		return XG_FORMAT_R32_FLOAT;
	case LUM_32_FLOAT:
		return XG_FORMAT_R32_FLOAT;
	case RGBA_8_UNORM:
		return XG_FORMAT_R8G8B8A8_UNORM;
	case RGBA_8_SNORM:
		return XG_FORMAT_R8G8B8A8_SNORM;
	case R_8_UNORM:
		return XG_FORMAT_R8_UNORM;
	case R_8_SNORM:
		return XG_FORMAT_R8_SNORM;
	case R_32_UINT:
		return XG_FORMAT_R32_UINT;
	case RG_32_UINT:
		return XG_FORMAT_R32G32_UINT;
	case RGB_32_UINT:
		return XG_FORMAT_R32G32B32_UINT;
	case RGBA_32_UINT:
		return XG_FORMAT_R32G32B32A32_UINT;
	case D_32_FLOAT:
		return XG_FORMAT_D32_FLOAT;
	default:
		return XG_FORMAT_UNKNOWN;
	};
}
static UINT64 iCurrentESRAMOffset = 0; // We allow this to grow beyond ESRAM_SIZE

ESRAMTexture::ESRAMTexture()
{
}


ESRAMTexture::~ESRAMTexture()
{
}

void dx11::ESRAMTexture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform
												 ,int w,int l
												 ,crossplatform::PixelFormat pixelFormat
												 ,bool computable,bool rendertarget,bool depthstencil
												 ,int num_samples,int aa_quality)
{
	dx11::Texture::ensureTexture2DSizeAndFormat(renderPlatform
												 , w, l
												 , pixelFormat
												 , computable, rendertarget, depthstencil
												 , num_samples, aa_quality)
		eSRAMManager->Create(textureDesc,*this);
}
#endif