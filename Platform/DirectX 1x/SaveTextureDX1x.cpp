#define _CRT_SECURE_NO_WARNINGS

//#include "SaveTexture.h"
#include "Macros.h"
#include "Simul/Base/inifile.h"
#include "Simul/Base/StringToWString.h"
simul::base::IniFile ini("atmospherics.ini");
#include <string>
#include <d3dx10.h>
#include <dxerr.h>
extern void RenderScene(ID3D10Device* pd3dDevice);

static bool FileExists(const std::string& strFilename)
{
    FILE* pFile = fopen(strFilename.c_str( ), "r");
    bool bExists = (pFile != NULL);
    if (pFile)
        fclose(pFile);
    return bExists;
}
void SaveTexture(ID3D10Texture *ldr_buffer_texture,const char *txt,bool as_dds)
{
	std::string path=ini.GetValue("Options","ScreenshotPath");
	if(path=="")
	{
		path=".";
	}
	std::string filename=path+"\\";
	filename+=txt;
	if(as_dds)
		filename+=".dds";
	else
		filename+=".jpg";
	int number=0;
	while(FileExists(filename))
	{
		number++;
		filename=path+"\\";
		filename+=txt;
		char number_text[10];
		sprintf_s(number_text,10,"%d",number);
		filename+=number_text;
		if(as_dds)
			filename+=".dds";
		else
			filename+=".jpg";
	}
#ifdef UNICODE
	std::wstring filename2=simul::base::StringToWString(filename);
#else
	std::string filename2=filename;
#endif
	D3DXSaveTextureToFile(filename2.c_str(),
												as_dds?D3DXIFF_DDS:D3DXIFF_JPG,
												ldr_buffer_texture,
												NULL);
//	V_CHECK(hr);
}

void Screenshot(ID3D10Device* pd3dDevice,const char *txt)
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
