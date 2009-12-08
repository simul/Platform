#define _CRT_SECURE_NO_WARNINGS

#include "SaveTexture.h"
#include "Macros.h"
#include "Simul/Base/inifile.h"
#include "Simul/Base/StringToWString.h"
simul::base::IniFile ini("atmospherics.ini");
#include <string>
#include <d3dx9.h>
#include <dxerr9.h>
typedef std::basic_string<TCHAR> tstring;
extern void RenderScene(IDirect3DDevice9* pd3dDevice);

static bool FileExists(const tstring& strFilename)
{
    FILE* pFile = _tfopen(strFilename.c_str( ),_T("r"));
    bool bExists = (pFile != NULL);
    if (pFile)
        fclose(pFile);
    return bExists;
}
void SaveTexture(LPDIRECT3DTEXTURE9 ldr_buffer_texture,const TCHAR *txt)
{
	tstring path;//=simul::base::StringToWString(ini.GetValue("Options","ScreenshotPath"));
	if(path==_T(""))
	{
		path=_T(".");
	}
	tstring filename=path+_T("\\");
	filename+=txt;
	filename+=_T(".jpg");
	int number=0;
	while(FileExists(filename))
	{
		number++;
		filename=path+_T("\\");
		filename+=txt;
		TCHAR number_text[10];
		_stprintf_s(number_text,10,_T("%d"),number);
		filename+=number_text;
		filename+=_T(".jpg");
	}
	HRESULT hr=D3DXSaveTextureToFile(filename.c_str(),D3DXIFF_JPG,
										ldr_buffer_texture,
										NULL);
	V_CHECK(hr);
}

void Screenshot(IDirect3DDevice9* pd3dDevice,const _TCHAR *txt)
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

	//RenderScene(pd3dDevice);
	//SetViewPortHelper(0,0,backBufferDesc.Width,backBufferDesc.Height,pd3dDevice);
	pd3dDevice->EndScene();
	pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
	SaveTexture(ldr_buffer_texture,txt);
	SAFE_RELEASE(ldr_buffer_texture);
	SAFE_RELEASE(pRenderTarget);
	SAFE_RELEASE(m_pOldRenderTarget);
}