
#include "Utilities.h"
#include "MacrosDX1x.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/DirectX11/Effect.h"
#include "Platform/DirectX11/DX11Exception.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Math/Vector3.h"
#if WINVER<0x602
#include <d3dx11.h>
#endif
#include <algorithm>			// for std::min / max

using namespace platform;
using namespace dx11;

const char *GetErrorText(HRESULT hr)
{
	const char *err = DXGetErrorStringA(hr); 
	return err;
}

bool platform::dx11::IsTypeless( DXGI_FORMAT fmt, bool partialTypeless )
{
    switch( static_cast<int>(fmt) )
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC7_TYPELESS:
        return true;

    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return partialTypeless;

    case 119 /* DXGI_FORMAT_R16_UNORM_X8_TYPELESS */:
    case 120 /* DXGI_FORMAT_X16_TYPELESS_G8_UINT */:
        // These are Xbox One platform specific types
        return partialTypeless;

    default:
        return false;
    }
}

DXGI_FORMAT platform::dx11::TypelessToSrvFormat(DXGI_FORMAT fmt)
{
	if (!IsTypeless(fmt, true))
		return fmt;
	int u=fmt+ 1;
	switch(fmt)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_BC1_TYPELESS:
		return DXGI_FORMAT_BC1_UNORM;
	case DXGI_FORMAT_BC2_TYPELESS:
		return DXGI_FORMAT_BC2_UNORM;
	case DXGI_FORMAT_BC3_TYPELESS:
		return DXGI_FORMAT_BC3_UNORM;
	case DXGI_FORMAT_BC6H_TYPELESS:
		return DXGI_FORMAT_BC6H_UF16;
	case DXGI_FORMAT_BC7_TYPELESS:
		return DXGI_FORMAT_BC7_UNORM;
	default:break;
	};
	return (DXGI_FORMAT)u;
}

DXGI_FORMAT platform::dx11::ResourceToDsvFormat(DXGI_FORMAT fmt)
{
	int u=fmt+ 1;
	switch(fmt)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT;
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_D16_UNORM;
	default:break;
	};
	return (DXGI_FORMAT)u;
}

ComputableTexture::ComputableTexture()
	:g_pTex_Output(NULL)
	,g_pUAV_Output(NULL)
	,g_pSRV_Output(NULL)
{
}

ComputableTexture::~ComputableTexture()
{
}

void ComputableTexture::release()
{
	SAFE_RELEASE(g_pTex_Output);
	SAFE_RELEASE(g_pUAV_Output);
	SAFE_RELEASE(g_pSRV_Output);
}

void ComputableTexture::init(ID3D11Device *pd3dDevice,int w,int h)
{
	release();
    D3D11_TEXTURE2D_DESC tex_desc;
	ZeroMemory(&tex_desc, sizeof(D3D11_TEXTURE2D_DESC));
	tex_desc.ArraySize			= 1;
    tex_desc.BindFlags			= D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    tex_desc.Usage				= D3D11_USAGE_DEFAULT;
    tex_desc.Width				= w;
    tex_desc.Height				= h;
    tex_desc.MipLevels			= 1;
    tex_desc.SampleDesc.Count	= 1;
	tex_desc.Format				= DXGI_FORMAT_R32_UINT;
    pd3dDevice->CreateTexture2D(&tex_desc, NULL, &g_pTex_Output);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	uav_desc.Format				= tex_desc.Format;
	uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE2D;
	uav_desc.Texture2D.MipSlice	= 0;
	pd3dDevice->CreateUnorderedAccessView(g_pTex_Output, &uav_desc, &g_pUAV_Output);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    srv_desc.Format						= tex_desc.Format;
    srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels		= 1;
    srv_desc.Texture2D.MostDetailedMip	= 0;
    pd3dDevice->CreateShaderResourceView(g_pTex_Output, &srv_desc, &g_pSRV_Output);
}

/*
create the associated Texture2D resource as a Texture2D array resource, and then create a shader resource view for that resource.

1. Load all of the textures using D3DX10CreateTextureFromFile, with a D3DX10_IMAGE_LOAD_INFO specifying that you want D3D10_USAGE_STAGING.
		all of your textures need to be the same size, have the same format, and have the same number of mip levels
2. Map every mip level of every texture
3. Set up an array of D3D10_SUBRESOURCE_DATA's with number of elements == number of textures * number of mip levels. 
4. For each texture, set the pSysMem member of a D3D10_SUBRESOURCE_DATA to the pointer you got when you mapped each mip level of each texture. Make sure you also set the SysMemPitch to the pitch you got when you mapped that mip level
5. Call CreateTexture2D with the desired array size, and pass the array of D3D10_SUBRESOURCE_DATA's
6. Create a shader resource view for the texture 
*/
void ArrayTexture::create(ID3D11Device *pd3dDevice,const std::vector<std::string> &texture_files,const std::vector<std::string> &pathsUtf8)
{
	release();
	std::vector<ID3D11Texture2D *> textures;
	for(unsigned i=0;i<texture_files.size();i++)
	{
		textures.push_back(platform::dx11::LoadStagingTexture(pd3dDevice,texture_files[i].c_str(),pathsUtf8));
	}
	D3D11_TEXTURE2D_DESC desc;
//	D3D11_SUBRESOURCE_DATA *subResources=new D3D11_SUBRESOURCE_DATA[textures.size()];
	ID3D11DeviceContext *pContext=NULL;
	pd3dDevice->GetImmediateContext(&pContext);
	for(int i=0;i<(int)textures.size();i++)
	{
		if(!textures[i])
			return;
		textures[i]->GetDesc(&desc);
	}
	static int num_mips=5;
	desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
	desc.Usage=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags=0;
	desc.ArraySize=(UINT)textures.size();
	desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.MipLevels=num_mips;
	pd3dDevice->CreateTexture2D(&desc,NULL,&m_pArrayTexture);

	if(m_pArrayTexture)
	for(unsigned i=0;i<textures.size();i++)
	{
		// Copy the resource directly, no CPU mapping
		pContext->CopySubresourceRegion(
						m_pArrayTexture
						,i*num_mips
						,0
						,0
						,0
						,textures[i]
						,0
						,NULL
						);
		//pContext->UpdateSubresource(m_pArrayTexture,i*num_mips, NULL,subResources[i].pSysMem,subResources[i].SysMemPitch,subResources[i].SysMemSlicePitch);
	}
	pd3dDevice->CreateShaderResourceView(m_pArrayTexture,NULL,&m_pArrayTexture_SRV);
	//delete [] subResources;
	for(unsigned i=0;i<textures.size();i++)
	{
	//	pContext->Unmap(textures[i],0);
		SAFE_RELEASE(textures[i])
	}
	pContext->GenerateMips(m_pArrayTexture_SRV);
	SAFE_RELEASE(pContext)
}

void ArrayTexture::create(ID3D11Device *pd3dDevice,int w,int l,int num,DXGI_FORMAT f,bool computable)
{
	release();
	D3D11_TEXTURE2D_DESC desc;
	static int num_mips		=5;
	desc.Width				=w;
	desc.Height				=l;
	desc.Format				=f;
	desc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_RENDER_TARGET ;
	desc.Usage				=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags		=0;
	desc.ArraySize			=num;
	desc.MiscFlags			=D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.MipLevels			=num_mips;
	desc.SampleDesc.Count	=1;
	desc.SampleDesc.Quality	=0;
	V_CHECK(pd3dDevice->CreateTexture2D(&desc,NULL,&m_pArrayTexture));
	V_CHECK(pd3dDevice->CreateShaderResourceView(m_pArrayTexture,NULL,&m_pArrayTexture_SRV));
	V_CHECK(pd3dDevice->CreateUnorderedAccessView(m_pArrayTexture,NULL,&unorderedAccessView));
	//SAFE_RELEASE(pContext)
}
