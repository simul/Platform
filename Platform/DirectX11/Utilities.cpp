
#include "Utilities.h"
#include "MacrosDX1x.h"

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