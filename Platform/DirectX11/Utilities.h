#pragma once
#include <d3d11.h>

struct ComputableTexture
{
	ComputableTexture();
	~ComputableTexture();

	ID3D11Texture2D*            g_pTex_Output;
	ID3D11UnorderedAccessView*  g_pUAV_Output;
	ID3D11ShaderResourceView*   g_pSRV_Output;

	void release();
	void init(ID3D11Device *pd3dDevice,int w,int h);
};