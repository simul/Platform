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

ESRAMTextureData::ESRAMTextureData():
				m_pESRAMTexture2D(NULL)
				,m_pESRAMSRV(NULL)
				,m_pESRAMUAV(NULL)
				,m_pESRAMRTV(NULL)
				,m_pESRAMDSV(NULL)
{
}

ESRAMTextureData::~ESRAMTextureData()
{
	SAFE_RELEASE(m_pESRAMTexture2D)
	SAFE_RELEASE(m_pESRAMSRV)
	SAFE_RELEASE(m_pESRAMUAV)
	SAFE_RELEASE(m_pESRAMRTV)
	SAFE_RELEASE(m_pESRAMDSV)
}

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
	:in_esram(false)
{
}


ESRAMTexture::~ESRAMTexture()
{
}

ID3D11Texture2D *ESRAMTexture::AsD3D11Texture2D()
{
	eSRAMManager->InsertGPUWait( esramTextureData.m_esramResource );    
	if(in_esram)
	{
		return esramTextureData.m_pESRAMTexture2D;
	}
	else
		return (ID3D11Texture2D*)texture;
}
ID3D11ShaderResourceView *ESRAMTexture::AsD3D11ShaderResourceView(crossplatform::ShaderResourceType t,int index,int mip)
{
	eSRAMManager->InsertGPUWait( esramTextureData.m_esramResource );   
	if(in_esram)
	{
		return esramTextureData.m_pESRAMSRV;
	}
	return dx11::Texture::AsD3D11ShaderResourceView(t,index,mip);
}

ID3D11UnorderedAccessView *ESRAMTexture::AsD3D11UnorderedAccessView(int index,int mip)
{
	if(mip<0)
		mip=0;
	if(mip>=mips)
		return NULL;
	eSRAMManager->InsertGPUWait( esramTextureData.m_esramResource );   
	if(in_esram)
	{
		return esramTextureData.m_pESRAMUAV;
	}
	return dx11::Texture::AsD3D11UnorderedAccessView(index,mip);
}

ID3D11DepthStencilView *ESRAMTexture::AsD3D11DepthStencilView()
{
	eSRAMManager->InsertGPUWait( esramTextureData.m_esramResource );   
	if(in_esram)
	{
		return esramTextureData.m_pESRAMDSV;
	}
	return depthStencilView;
}

ID3D11RenderTargetView *ESRAMTexture::AsD3D11RenderTargetView(int index,int mip)
{
	eSRAMManager->InsertGPUWait( esramTextureData.m_esramResource );   
	if(in_esram)
	{
		return esramTextureData.m_pESRAMRTV;
	}
	return dx11::Texture::AsD3D11RenderTargetView(index, mip);
}
bool ESRAMTexture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform
												 ,int w,int l
												 ,crossplatform::PixelFormat pixelFormat
												 ,bool computable,bool rendertarget,bool depthstencil
												 ,int num_samples,int aa_quality,bool wrap
	,vec4 clear, float clearDepth , uint clearStencil )
{
	bool res=dx11::Texture::ensureTexture2DSizeAndFormat(renderPlatform
												 , w, l
												 , pixelFormat
												 , computable, rendertarget, depthstencil
												 , num_samples, aa_quality,wrap);
	if(in_esram)
		MoveToFastRAM();
	return res;
}

void ESRAMTexture::MoveToFastRAM()
{
	DiscardFromFastRAM();
	if(texture)
		eSRAMManager->Prefetch((ID3D11Texture2D*)texture,esramTextureData);
	in_esram=true;
}

void ESRAMTexture::MoveToSlowRAM()
{
	if(texture)
	{
		eSRAMManager->Writeback(esramTextureData,(ID3D11Texture2D*)texture);
	}
	in_esram=false;
}

void ESRAMTexture::DiscardFromFastRAM()
{
	eSRAMManager->Discard(esramTextureData);
}
#endif