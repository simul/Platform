#define _CRT_SECURE_NO_WARNINGS

#include "SaveTexture.h"
#include "Macros.h"
#include "Simul/Base/inifile.h"
#include "Simul/Base/StringToWString.h"
simul::base::IniFile ini("atmospherics.ini");
#include <string>
#include <d3dx9.h>
#include <dxerr.h>
void (*RenderScene)(IDirect3DDevice9* pd3dDevice)=NULL;

static bool FileExists(const std::string& filename_utf8)
{
    FILE* pFile = NULL;
	std::wstring wstr=simul::base::Utf8ToWString(filename_utf8.c_str());
	_wfopen_s(&pFile,wstr.c_str(),L"r");
    bool bExists = (pFile != NULL);
    if (pFile)
        fclose(pFile);
    return bExists;
}

void SaveTexture(LPDIRECT3DTEXTURE9 texture,const char *filename_utf8)
{
	D3DXIMAGE_FILEFORMAT ff=D3DXIFF_PNG;
	std::string path=ini.GetValue("Options","ScreenshotPath");
	if(path=="")
	{
		path=".";
	}
	std::string filename=filename_utf8;
	if(filename.find(':')<=filename.length())
	{
	}
	else
	{
		filename=path+"\\";
		filename+=filename_utf8;
	}
	if(filename.find(".dds")<=filename.length())
		ff=D3DXIFF_DDS;
	else if(filename.find(".hdr")<=filename.length())
		ff=D3DXIFF_HDR;

	int pos=filename.find_last_of('.');
	std::string ext=filename.substr(pos,filename.length()-pos);
	std::string root=filename.substr(0,pos);
	int number=0;
	while(FileExists(filename))
	{
		number++;
		filename=path+"\\";
		filename+=root;
		char number_text[10];
		sprintf_s(number_text,10,"%d",number);
		filename+=number_text;
		filename+=ext;
	}
	std::wstring wstr=simul::base::Utf8ToWString(filename_utf8);
	D3DXSaveTextureToFileW(wstr.c_str(),ff,texture,NULL);
//	V_CHECK(hr);
}

void Screenshot(IDirect3DDevice9* pd3dDevice,const char *txt)
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
	SaveTexture(ldr_buffer_texture,txt);
	SAFE_RELEASE(ldr_buffer_texture);
	SAFE_RELEASE(pRenderTarget);
	SAFE_RELEASE(m_pOldRenderTarget);
}
