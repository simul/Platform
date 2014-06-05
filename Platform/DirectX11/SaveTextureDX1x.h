#pragma once

#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#else
typedef enum D3DX11_IMAGE_FILE_FORMAT
{
    D3DX11_IFF_BMP         = 0,
    D3DX11_IFF_JPG         = 1,
    D3DX11_IFF_PNG         = 3,
    D3DX11_IFF_DDS         = 4,
    D3DX11_IFF_TIFF		  = 10,
    D3DX11_IFF_GIF		  = 11,
    D3DX11_IFF_WMP		  = 12,
    D3DX11_IFF_FORCE_DWORD = 0x7fffffff

} D3DX11_IMAGE_FILE_FORMAT;
extern HRESULT D3DX11SaveTextureToFileW(ID3D11DeviceContext       *pContext,ID3D11Resource            *pSrcTexture,D3DX11_IMAGE_FILE_FORMAT    DestFormat,LPCWSTR                   pDestFile);
#endif
#include "Simul/Platform/DirectX11/Export.h"

namespace simul
{
	namespace dx11
	{
		//extern void SaveScreenshot(ID3D11Device* pd3dDevice,const char *txt);
		extern SIMUL_DIRECTX11_EXPORT void SaveTexture(ID3D11Device *pd3dDevice,ID3D11Texture2D *texture,const char *filename_utf8);
	}
}