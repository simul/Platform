#define _CRT_SECURE_NO_WARNINGS

#include "SaveTextureDX1x.h"
#include "MacrosDX1x.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/FileLoader.h"
#include <string>
#include <d3dx11.h>
#include <dxerr.h>
using namespace simul;
using namespace dx11;

namespace simul
{
	namespace dx11
	{
		void SaveTexture(ID3D11Device *pd3dDevice,ID3D11Texture2D *texture,const char *filename_utf8)
		{
			std::string fn_utf8=filename_utf8;
			bool as_dds=false;
			if(fn_utf8.find(".dds")<fn_utf8.length())
				as_dds=true;
			int number=0;
			while(simul::base::FileLoader::GetFileLoader()->FileExists(fn_utf8.c_str()))
			{
				number++;
				char number_text[10];
				sprintf_s(number_text,10,"%d",number);
				std::string nstr(number_text);
				int pos=(int)fn_utf8.find_last_of(".");
				fn_utf8=fn_utf8.replace(fn_utf8.begin()+pos,fn_utf8.begin()+pos+1,nstr+".");
			}
			std::wstring wfilename=simul::base::Utf8ToWString(fn_utf8);
			ID3D11DeviceContext*			m_pImmediateContext;
			pd3dDevice->GetImmediateContext(&m_pImmediateContext);
			D3DX11SaveTextureToFileW(m_pImmediateContext,texture,as_dds?D3DX11_IFF_DDS:D3DX11_IFF_PNG,wfilename.c_str());
			SAFE_RELEASE(m_pImmediateContext);
		}
	}
}
/*
extern void SaveScreenshot(ID3D11Device* pd3dDevice,const char *txt);
{
	HRESULT hr=S_OK;
    if(!SUCCEEDED(pd3dDevice->BeginScene()))
		return;
	LPDIRECT3DSURFACE9 backBuffer;
	pd3dDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backBuffer);
	D3DSURFACE_DESC backBufferDesc;
	backBuffer->GetDesc(&backBufferDesc);
	SAFE_RELEASE(backBuffer);
	LPDIRECT3DTEXTURE9 ldr_buffer_texture;
	hr=(pd3dDevice->CreateTexture(	backBufferDesc.Width,
									backBufferDesc.Height,
									1,
									D3DUSAGE_RENDERTARGET,
									D3DFMT_A8R8G8B8,
									D3DPOOL_DEFAULT,
									&ldr_buffer_texture,
									NULL));

	LPDIRECT3DSURFACE9 pRenderTarget,m_pOldRenderTarget;
	ldr_buffer_texture->GetSurfaceLevel(0,&pRenderTarget);
	hr=pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget);
	pd3dDevice->SetRenderTarget(0,pRenderTarget);

	RenderScene(pd3dDevice);
	//SetViewPortHelper(0,0,backBufferDesc.Width,backBufferDesc.Height,pd3dDevice);
	pd3dDevice->EndScene();
	pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
	SaveTexture(ldr_buffer_texture,txt,false);
	SAFE_RELEASE(ldr_buffer_texture);
	SAFE_RELEASE(pRenderTarget);
	SAFE_RELEASE(m_pOldRenderTarget);
}
*/