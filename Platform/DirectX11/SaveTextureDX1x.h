#pragma once

#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include "Simul/Platform/DirectX11/Export.h"

namespace simul
{
	namespace dx11
	{
		//extern void SaveScreenshot(ID3D11Device* pd3dDevice,const char *txt);
		extern SIMUL_DIRECTX11_EXPORT void SaveTexture(ID3D11Device *pd3dDevice,ID3D11Texture2D *texture,const char *filename_utf8);
	}
}