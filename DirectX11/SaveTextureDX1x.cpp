#define _CRT_SECURE_NO_WARNINGS

#include "SaveTextureDX1x.h"
#include "MacrosDX1x.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/FileLoader.h"
#include "Simul/Base/RuntimeError.h"
#include <string>
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#endif
using namespace simul;
using namespace dx11;
#ifdef SIMUL_WIN8_SDK

#include <DirectXTex.h>
#include <wincodec.h>
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
			std::wstring wfilename=simul::base::Utf8ToWString(fn_utf8);
			ID3D11DeviceContext*			m_pImmediateContext;
			pd3dDevice->GetImmediateContext(&m_pImmediateContext);
			//int flags=0;
//			DirectX::TexMetadata metadata;
			DirectX::ScratchImage scratchImage;
			HRESULT hr=DirectX::CaptureTexture( pd3dDevice, m_pImmediateContext, texture, scratchImage );
			if(hr!=S_OK)
			{
				SIMUL_CERR<<"Failed to save texture "<<filename_utf8<<std::endl;
				return;
			}
			const DirectX::Image *image=scratchImage.GetImage(0,0,0);
			DirectX::SaveToWICFile(*image,DirectX::WIC_FLAGS_NONE,GUID_ContainerFormatPng,wfilename.c_str(),&GUID_WICPixelFormat24bppBGR);
			SAFE_RELEASE(m_pImmediateContext);
		}
	}
}
#else

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
			std::wstring wfilename=simul::base::Utf8ToWString(fn_utf8);
			ID3D11DeviceContext*			m_pImmediateContext;
			pd3dDevice->GetImmediateContext(&m_pImmediateContext);
			D3DX11SaveTextureToFileW(m_pImmediateContext,texture,as_dds?D3DX11_IFF_DDS:D3DX11_IFF_PNG,wfilename.c_str());
			SAFE_RELEASE(m_pImmediateContext);
		}
	}
}
#endif

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