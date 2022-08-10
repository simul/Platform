#// Copyright (c) 2007-2017 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.cpp Create a DirectX .fx effect and report errors.

#include "CreateEffectDX1x.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/DefaultFileLoader.h"
#include "Platform/Math/Matrix4x4.h"
#include "Platform/DirectX11/Utilities.h"
#include "Platform/DirectX11/CompileShaderDX1x.h"
#include "Platform/DirectX11/Effect.h"
#include <string>

#include <vector>
#include <iostream>
#include <assert.h>
#include <fstream>
#include "MacrosDX1x.h"
#include <DirectXTex.h>
typedef struct D3DX11_IMAGE_LOAD_INFO {
  UINT              Width;
  UINT              Height;
  UINT              Depth;
  UINT              FirstMipLevel;
  UINT              MipLevels;
  D3D11_USAGE       Usage;
  UINT              BindFlags;
  UINT              CpuAccessFlags;
  UINT              MiscFlags;
  DXGI_FORMAT       Format;
  UINT              Filter;
  UINT              MipFilter;
  void *pSrcInfo;
} D3DX11_IMAGE_LOAD_INFO, *LPD3DX11_IMAGE_LOAD_INFO;
enum {D3DX11_FROM_FILE=(UINT)-3};
enum {D3DX11_FILTER_NONE=(1 << 0)};

#ifdef _XBOX_ONE
	#pragma comment(lib,"d3d11_x.lib")
	#pragma comment(lib,"d3dcompiler.lib")
#else
	#pragma comment(lib,"dxguid.lib")
	#pragma comment(lib,"dxgi.lib")
	#pragma comment(lib,"d3d11.lib")
	#if 0//defined(SIMUL_WIN8_SDK) && defined(WIN64)
		#pragma comment(lib,"d3dcompiler_xdk.lib")
	#else
		#pragma comment(lib,"d3dcompiler.lib")
	#endif
#endif

// winmm.lib comctl32.lib
static bool pipe_compiler_output=true;

//static ID3D11Device		*pd3dDevice		=NULL;
using namespace platform;
using namespace dx11;
using namespace core;

#pragma optimize("",off)

namespace platform
{
	namespace dx11
	{
		void PipeCompilerOutput(bool p)
		{
			pipe_compiler_output=p;
		}
	}
}

#if WINVER>=0x602
HRESULT D3DX11CreateTextureFromFileW(ID3D11Device* pd3dDevice,const wchar_t *filename,D3DX11_IMAGE_LOAD_INFO *loadInfo
									, void *, ID3D11Resource** tex, HRESULT *hr )
{
	int flags=0;
	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratchImage;
	DirectX::LoadFromWICFile( filename,flags,&metadata,scratchImage);
	const DirectX::Image *image=scratchImage.GetImage(0,0,0);
    *hr=CreateTexture(  pd3dDevice,image, 1, metadata,tex );
	return *hr;
}
#endif

ID3D11Texture2D* platform::dx11::LoadTexture(ID3D11Device* pd3dDevice,const char *filename,const std::vector<std::string> &texturePathsUtf8)
{
	ERRNO_BREAK
	ID3D11Texture2D* tex=NULL;
	std::string str;
	int idx= platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(filename,texturePathsUtf8);
	if(idx<-1||idx>=(int)texturePathsUtf8.size())
	{
		SIMUL_CERR<<"Texture file not found: "<<filename<<std::endl;
			return NULL;
	}
	str=filename;
	if(idx>=0)
		str=(texturePathsUtf8[idx]+"/")+str;
	void *ptr=NULL;
	unsigned bytes=0;
	platform::core::FileLoader::GetFileLoader()->AcquireFileContents(ptr,bytes,str.c_str(),false);
#if WINVER<0x602
	D3DX11_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory(&loadInfo,sizeof(D3DX11_IMAGE_LOAD_INFO));
	loadInfo.BindFlags	=D3D11_BIND_SHADER_RESOURCE;
	loadInfo.Format		=DXGI_FORMAT_FROM_FILE;
	loadInfo.MipLevels=0;
	HRESULT hr			= D3DX11CreateTextureFromMemory(
									pd3dDevice
									,ptr
									,bytes
									,&loadInfo
									,NULL
									,&tex
									,&hr);
#else
	int flags=0;
	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratchImage;
	HRESULT hr=S_OK;
	if(str.find(".hdr")==str.length()-4)
	{
		hr= DirectX::LoadFromHDRMemory( ptr, _In_ bytes,&metadata,scratchImage );

	}
	else
	{
		hr=DirectX::LoadFromWICMemory( ptr, bytes, flags,&metadata,scratchImage );
	}

	const DirectX::Image *image=scratchImage.GetImage(0,0,0);
	if(!image)
		return NULL;
	ID3D11Resource *res = tex;
    hr=CreateTexture(pd3dDevice,image, 1, metadata, &res );
	if (res&&res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex) == S_OK)
	{
		// SHould have one ref:
		int numrefs=res->Release();
	//	std::cout << numrefs << std::endl;
	}
#endif
	platform::core::FileLoader::GetFileLoader()->ReleaseFileContents(ptr);
	return tex;
}

ID3D11Texture2D* platform::dx11::LoadStagingTexture(ID3D11Device* pd3dDevice,const char *filename,const std::vector<std::string> &texturePathsUtf8)
{
	D3DX11_IMAGE_LOAD_INFO loadInfo;

    ZeroMemory(&loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO));
	//loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	loadInfo.MipLevels=0;

    loadInfo.Width          = (UINT)D3DX11_FROM_FILE;
    loadInfo.Height         = (UINT)D3DX11_FROM_FILE;
    loadInfo.Depth          = (UINT)D3DX11_FROM_FILE;
	loadInfo.Usage          = D3D11_USAGE_STAGING;
    loadInfo.Format         = (DXGI_FORMAT) D3DX11_FROM_FILE;
    loadInfo.CpuAccessFlags = D3D11_CPU_ACCESS_READ;
	loadInfo.FirstMipLevel  = (UINT)D3DX11_FROM_FILE;
    loadInfo.MipLevels      = (UINT)D3DX11_FROM_FILE;
    loadInfo.MiscFlags      = 0;
    loadInfo.MipFilter      = (UINT)D3DX11_FROM_FILE;
    loadInfo.pSrcInfo       = NULL;
    loadInfo.Filter         = D3DX11_FILTER_NONE;

	//if(!texturePathsUtf8.size())
	//	texturePathsUtf8.push_back("media/textures");
	ID3D11Texture2D *tex=NULL;
	HRESULT res = S_OK;
	for(int i=0;i<(int)texturePathsUtf8.size();i++)
	{
		std::string str		=texturePathsUtf8[i];
		if(str.length())
			str+="/";
		str+=filename;
		std::wstring wstr	=platform::core::Utf8ToWString(str);
		res					= D3DX11CreateTextureFromFileW(pd3dDevice,wstr.c_str(), &loadInfo, NULL, ( ID3D11Resource** )&tex, &res );

		if(res == S_OK)
			break;
	}
	// Report error if none of the paths worked
	if (res != S_OK)
	{
#ifdef DXTRACE_ERR
		DXTRACE_ERR(L"LoadStagingTexture", res);
#else
		std::cerr<<"Failed to load texture: "<<filename<<std::endl;
#endif
	}
	return tex;
}
		

struct d3dMacro
{
	std::string name;
	std::string define;
};

static double GetNewestIncludeFileDate(std::string text_filename_utf8,const std::vector<std::string> &shaderPathsUtf8
	,void *textData,size_t textSize,D3D_SHADER_MACRO *macros,double binary_date_jdn,bool &missing)
{
	ID3DBlob *binaryBlob=NULL;
	ID3DBlob *errorMsgs=NULL;
	int pos=(int)text_filename_utf8.find_last_of("/");
	int bpos=(int)text_filename_utf8.find_last_of("\\");
	if(pos<0||bpos>pos)
		pos=bpos;
	std::string path_utf8=text_filename_utf8.substr(0,pos);
	DetectChangesIncludeHandler detectChangesIncludeHandler(path_utf8.c_str(),shaderPathsUtf8,binary_date_jdn);
	HRESULT hr=D3DPreprocess(	textData	
						,textSize
						,text_filename_utf8.c_str()		//in   LPCSTR pSourceName,
						,macros							//in   const D3D_SHADER_MACRO *pDefines,
						,&detectChangesIncludeHandler	//in   ID3DInclude pInclude,
						,&binaryBlob					//ID3DBlob **ppCodeText,
						,&errorMsgs						//ID3DBlob **ppErrorMsgs
						);
	// Don't V_CHECK here as we'll often expect a S_FAIL result for early-out.
	double newestFileTime=detectChangesIncludeHandler.GetNewestIncludeDateJDN();
	if(binaryBlob)
		binaryBlob->Release();
	if(errorMsgs)
		errorMsgs->Release();
	missing=(newestFileTime==0.0&&hr!=S_OK);
	return newestFileTime;
}

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

void BreakIfDebugging()
{
	BREAK_IF_DEBUGGING;
}

int platform::dx11::ByteSizeOfFormatElement( DXGI_FORMAT format )
{
    switch( format )
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 16;
               
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 12;
               
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return 8;
       
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            return 4;
               
        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            return 2;
               
        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return 1;
       
        // Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
            return 16;
               
        // Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
        case DXGI_FORMAT_R1_UNORM:
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 8;
       
        // Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
            return 4;
       
        // These are compressed, but bit-size information is unclear.
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
            return 4;

        case DXGI_FORMAT_UNKNOWN:
        default:
            return 0;
    }
}