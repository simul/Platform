#// Copyright (c) 2007-2017 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.cpp Create a DirectX .fx effect and report errors.

#include "CreateEffectDX1x.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/CompileShaderDX1x.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include <string>

#include <vector>
#include <iostream>
#include <assert.h>
#include <fstream>
#include "MacrosDX1x.h"
#include "D3dx11effect.h"
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#else
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
#endif

#ifndef SIMUL_WIN8_SDK
	#pragma comment(lib,"Effects11_DXSDK.lib")
	#pragma comment(lib,"dxerr.lib")
	#pragma comment(lib,"d3dx11.lib")
#endif
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
using namespace simul;
using namespace dx11;
using namespace base;

#pragma optimize("",off)

namespace simul
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

ID3D11Texture2D* simul::dx11::LoadTexture(ID3D11Device* pd3dDevice,const char *filename,const std::vector<std::string> &texturePathsUtf8)
{
	ERRNO_BREAK
	ID3D11Texture2D* tex=NULL;
	std::string str;
	int idx=simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(filename,texturePathsUtf8);
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
	simul::base::FileLoader::GetFileLoader()->AcquireFileContents(ptr,bytes,str.c_str(),false);
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
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(ptr);
	return tex;
}

ID3D11Texture2D* simul::dx11::LoadStagingTexture(ID3D11Device* pd3dDevice,const char *filename,const std::vector<std::string> &texturePathsUtf8)
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
		std::wstring wstr	=simul::base::Utf8ToWString(str);
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
		
void simul::dx11::setDepthState(ID3DX11Effect *effect,const char *name		,ID3D11DepthStencilState * value)
{
	ID3DX11EffectDepthStencilVariable*	var	=effect->GetVariableByName(name)->AsDepthStencil();
	var->SetDepthStencilState(0,value);
}

void simul::dx11::setSamplerState(ID3DX11Effect *effect,const char *name	,ID3D11SamplerState * value)
{
	ID3DX11EffectSamplerVariable*	var	=effect->GetVariableByName(name)->AsSampler();
	var->SetSampler(0,value);
}

bool simul::dx11::setTexture(ID3DX11Effect *effect,const char *name			,ID3D11ShaderResourceView * value)
{
	if(!effect)
		return false;
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	if(!var->IsValid())
	{
		SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader texture ")+name).c_str());
	}
	var->SetResource(value);
	if(var->IsValid()!=0)
		return true;
	return false;
}
bool simul::dx11::setUnorderedAccessView(ID3DX11Effect *effect,const char *name	,ID3D11UnorderedAccessView * value)
{
	if (!effect)
	{
		return false;
	}
	ID3DX11EffectUnorderedAccessViewVariable*	var	=effect->GetVariableByName(name)->AsUnorderedAccessView();
	if (!var->IsValid())
	{
		SIMUL_ASSERT_WARN(var->IsValid() != 0, (std::string("Invalid shader variable ") + name).c_str());
	}
	var->SetUnorderedAccessView(value);
	if(value&&var->IsValid()!=0)
		return true;
	return false;
}

void simul::dx11::setStructuredBuffer(ID3DX11Effect *effect,const char *name,ID3D11ShaderResourceView * value)
{
	if (!effect)
	{
		return;
	}
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetResource(value);
}

void simul::dx11::setTextureArray(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView *value)
{
	if (!effect)
	{
		return;
	}
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetResource(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float value)
{
	if (!effect)
	{
		return;
	}
	ID3DX11EffectScalarVariable*	var	=effect->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetFloat(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float x,float y)
{
	if (!effect)
	{
		return;
	}
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	float V[]={x,y,0.f,0.f};
	var->SetFloatVector(V);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float x,float y,float z,float w)
{
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	float V[]={x,y,z,w};
	var->SetFloatVector(V);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,int value)
{
	ID3DX11EffectScalarVariable*	var	=effect->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetInt(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,const float *value)
{
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetFloatVector(value);
}

void simul::dx11::setMatrix(ID3DX11Effect *effect,const char *name	,const float *value)
{
	ID3DX11EffectMatrixVariable*	var	=effect->GetVariableByName(name)->AsMatrix();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetMatrix(value);
}


struct d3dMacro
{
	std::string name;
	std::string define;
};

HRESULT WINAPI D3DX11CreateEffectFromBinaryFileUtf8(const char *binary_filename_utf8, UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	HRESULT hr=(HRESULT)(-1);
	void *pData=NULL;
	unsigned sz=0;
	simul::base::FileLoader::GetFileLoader()->AcquireFileContents(pData,sz,binary_filename_utf8,false);
	if(sz>0)
	{
		hr=D3DX11CreateEffectFromMemory(pData,sz,FXFlags,pDevice,ppEffect,binary_filename_utf8);
		if(hr!=S_OK)
			std::cerr<<"D3DX11CreateEffectFromBinaryFile error "<<(int)hr<<std::endl;
	}
	else
		SIMUL_CERR<<"D3DX11CreateEffectFromBinaryFile cannot find file "<<binary_filename_utf8<<std::endl;
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(pData);
	return hr;
}

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

HRESULT WINAPI D3DX11CreateEffectFromFileUtf8(std::string text_filename_utf8,D3D_SHADER_MACRO *macros,UINT ShaderFlags,UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect,
crossplatform::ShaderBuildMode shaderBuildMode, const std::vector<std::string>& shaderPathsUtf8,const std::vector<std::string> &shadeBinPathsUtf8)
{
ERRNO_CHECK
	HRESULT hr=S_OK;
	int pos=(int)text_filename_utf8.find_last_of("/");
	int bpos=(int)text_filename_utf8.find_last_of("\\");
	if(pos<0||bpos>pos)
		pos=bpos;
	std::string path_utf8=text_filename_utf8.substr(0,pos>0?pos:0);
	int dot=(int)text_filename_utf8.find_last_of(".");
	if(dot<0)
		dot=(int)text_filename_utf8.length();
	std::string name_utf8=text_filename_utf8.substr(pos+1,dot-pos-1);
	std::string binary_filename_utf8=name_utf8;
	// Modify the binary file with the macros so the output is unique to the specified values.
	int def=0;
	while(macros&&macros[def].Name!=0)
	{
		binary_filename_utf8+=macros[def].Name;
		binary_filename_utf8+=macros[def].Definition;
		def++;
	}
	binary_filename_utf8+=".fxo";
	void *textData=NULL;
	unsigned textSize=0;
ERRNO_CHECK
	// See if there's a binary that's newer than the file date.
	bool changes_detected=(shaderBuildMode&crossplatform::ALWAYS_BUILD)!=0;
	double binary_date_jdn=0.0;
	double newest_included_file=0.0;
	std::string binaryPathUtf8;
	//std::cout<<"Checking DX11 shader "<<text_filename_utf8.c_str()<<std::endl;
	for (const auto& binPath : shadeBinPathsUtf8)
	{
		double bin_date = simul::base::FileLoader::GetFileLoader()->GetFileDate(((binPath+"/")+binary_filename_utf8).c_str());
		if (bin_date > binary_date_jdn)
		{
			binary_date_jdn = bin_date;
			binaryPathUtf8 = binPath;
		}
	}
	if ((shaderBuildMode&crossplatform::BUILD_IF_CHANGED) != 0)
	{
		if(shaderBuildMode!=crossplatform::NEVER_BUILD)
			simul::base::FileLoader::GetFileLoader()->AcquireFileContents(textData,textSize,text_filename_utf8.c_str(),true);
	ERRNO_CHECK
		if(!textData)
		{
			SIMUL_COUT<<"No source file "<<text_filename_utf8.c_str()<<", defaulting to binary.\n";
			changes_detected=false;
		}
		else
		{
			double text_date_jdn = simul::base::FileLoader::GetFileLoader()->GetFileDate(text_filename_utf8.c_str());
			if(text_date_jdn>binary_date_jdn||!binary_date_jdn)
				changes_detected=true;
			else if(text_date_jdn>0)	// maybe some of the includes have changed?
			{
				if(base::GetFeatureLevel()>=FeatureLevel::EXPERIMENTAL)
				{
					// Xbox One shader build:
					//	/*
					//	C:\Program Files (x86)\Microsoft Durango XDK\xdk\FXC\amd64\Fxc cs.hlsl –T cs_5_0 –D__XBOX_CONTROL_NONIEEE=0
					//	*/
				}
				bool missing=false;
				newest_included_file=GetNewestIncludeFileDate(text_filename_utf8,shaderPathsUtf8,textData,textSize,macros,binary_date_jdn,missing);
				if(missing)
				{
					changes_detected=false;
					SIMUL_CERR<<"Can't tell if source is out of date for "<<text_filename_utf8<<" as not all includes are present."<<std::endl;
				}
				else if(hr!=S_OK||newest_included_file>binary_date_jdn)
					changes_detected=true;
			}
		}
	}
	crossplatform::ShaderBuildMode anyBuild=crossplatform::ALWAYS_BUILD|crossplatform::BUILD_IF_CHANGED;
	if((shaderBuildMode&anyBuild)==0||(!changes_detected&&binary_date_jdn>0))
	{
		hr=D3DX11CreateEffectFromBinaryFileUtf8(((binaryPathUtf8 + "/") + binary_filename_utf8).c_str(),FXFlags,pDevice,ppEffect);
		if(hr==S_OK)
			return S_OK;
		std::cout<<"Shader binary path is "<< binaryPathUtf8 <<std::endl;
		if((shaderBuildMode&anyBuild)==0)
			return S_FALSE;
	}
	ID3DBlob *binaryBlob	=NULL;
	ID3DBlob *errorMsgs		=NULL;
	ERRNO_CHECK
	binaryPathUtf8 = shadeBinPathsUtf8.back();
	std::string fullBinaryFilename=(binaryPathUtf8 + "/") + binary_filename_utf8;
	ShaderIncludeHandler shaderIncludeHandler(path_utf8.c_str(),"",shaderPathsUtf8);
	std::cout<<"Rebuilding DX11 shader "<<text_filename_utf8.c_str()<<" to target "<<binary_filename_utf8.c_str()<<std::endl;
	hr=D3DCompile(		textData
						,textSize
						,text_filename_utf8.c_str()	//LPCSTR pSourceName,
						,macros						//const D3D_SHADER_MACRO *pDefines,
						,&shaderIncludeHandler		//ID3DInclude *pInclude,
						,NULL						//LPCSTR pEntrypoint,
						,"fx_5_0"					//LPCSTR pTarget,
						,ShaderFlags				//UINT Flags1,
						,FXFlags					//UINT Flags2,
						,&binaryBlob				//ID3DBlob **ppCode,
						,&errorMsgs					//ID3DBlob **ppErrorMsgs
						);
ERRNO_CHECK
	if(shaderBuildMode!=crossplatform::NEVER_BUILD)
		simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(textData);
	if(hr==S_OK)
	{
		hr=D3DX11CreateEffectFromMemory(binaryBlob->GetBufferPointer(),binaryBlob->GetBufferSize(),FXFlags,pDevice,ppEffect);
		if(hr==S_OK)
		{
			if(simul::base::FileLoader::GetFileLoader()->Save(binaryBlob->GetBufferPointer(),(unsigned int)binaryBlob->GetBufferSize(), fullBinaryFilename.c_str(),false))
			{
				double new_binary_date_jdn			=simul::base::FileLoader::GetFileLoader()->GetFileDate(fullBinaryFilename.c_str());
				if(new_binary_date_jdn<newest_included_file)
				{
					SIMUL_CERR<<"Newly created file "<<binary_filename_utf8.c_str()<<" is older than newest include file."<<std::endl;
				}
			}
			else
			{
				SIMUL_CERR<<"Performance warning: failed to save "<<binary_filename_utf8.c_str()<<" to disk. Check file permissions?"<<std::endl;
			}
		}
	}
	if(errorMsgs)
	{
		char *errs=(char*)errorMsgs->GetBufferPointer();
		std::string err(errs);
		pos = 0;
		int last = pos;
		pos = (int)err.find("\n");
		while(pos>=0&&pos<(int)err.length())
		{
			std::string line=err.substr(last,pos-last);
			if(line.find(":")>3)
			{
				std::string path=path_utf8;
				char full[_MAX_PATH];
				if( _fullpath( full, path_utf8.c_str(), _MAX_PATH )!=NULL)
					path=full;
				path+="/";
				line=path+line;
			}
			std::cerr << line.c_str() << std::endl;
			last = pos + 1;
			pos = (int)err.find("\n", pos + 1);
		};//text_filename_utf8.c_str()<<
	}
	if(hr!=S_OK&&(shaderBuildMode&crossplatform::TRY_AGAIN_ON_FAIL)!=crossplatform::TRY_AGAIN_ON_FAIL&&binary_date_jdn!=0.0)
	{
		SIMUL_CERR<<"Compile failed, loading binary as fallback: "<<binary_filename_utf8<<std::endl;
		SIMUL_BREAK_ONCE("Shader compile failure!");
		// if we're not planning to rebuild, load the binary.
		hr=D3DX11CreateEffectFromBinaryFileUtf8(fullBinaryFilename.c_str(),FXFlags,pDevice,ppEffect);
		if(hr==S_OK)
			return S_OK;
	}
	if(binaryBlob)
		binaryBlob->Release();
	if(errorMsgs)
		errorMsgs->Release();
	return hr;
}

HRESULT simul::dx11::CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const char *filename,crossplatform::ShaderBuildMode shaderBuildMode,const std::vector<std::string> &shaderPathsUtf8, const std::vector<std::string> &shaderBinPathsUtf8)
{
	std::map<std::string,std::string> defines;
	return simul::dx11::CreateEffect(d3dDevice,effect,filename,defines,0,shaderBuildMode,shaderPathsUtf8, shaderBinPathsUtf8);
}

HRESULT simul::dx11::CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const char *filenameUtf8,const std::map<std::string,std::string>&defines
	,unsigned int shader_flags,crossplatform::ShaderBuildMode shaderBuildMode,const std::vector<std::string> &shaderPathsUtf8, const std::vector<std::string>& shaderBinPathsUtf8)
{
	SIMUL_ASSERT_WARN(d3dDevice!=NULL,"Null device");
	HRESULT hr=S_OK;
	int index=simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(filenameUtf8,shaderPathsUtf8);
	std::string filename_utf8;
	if(index<0)
		filename_utf8=filenameUtf8;
	else if(index>=0&&index<shaderPathsUtf8.size())
		filename_utf8=(shaderPathsUtf8[index]+"/")+filenameUtf8;
	else
	{
		filename_utf8=filenameUtf8;
	}
	if(!simul::base::FileLoader::GetFileLoader()->FileExists(filename_utf8.c_str()))
	{
		filename_utf8=filenameUtf8;
		// This will fail later unless we can use the binary.
	}
	D3D_SHADER_MACRO *macros=NULL;
	{
		size_t num_defines=defines.size();
		if(num_defines)
		{
			macros=new D3D_SHADER_MACRO[num_defines+1];
			macros[num_defines].Definition=0;
			macros[num_defines].Name=0;
		}
		size_t def=0;
		for(std::map<std::string,std::string>::const_iterator i=defines.begin();i!=defines.end();i++)
		{
			macros[def].Name=i->first.c_str();
			macros[def].Definition=i->second.c_str();
			def++;
		}
	}
static const DWORD default_effect_flags=0;
	DWORD flags=default_effect_flags;
	SAFE_RELEASE(*effect);
	hr=1;
	while(hr!=S_OK)
	{
		hr=D3DX11CreateEffectFromFileUtf8(	filename_utf8,
											macros,
											shader_flags,
											flags,
											d3dDevice,
											effect,shaderBuildMode,shaderPathsUtf8, shaderBinPathsUtf8);
		if(hr==S_OK)
			break;
		// Turn off optimization in case of D3D optimization bugs, e.g. for groupshared "race condition"
		shader_flags=D3DCOMPILE_SKIP_OPTIMIZATION;
		std::string err="";
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateEffect", hr );
#endif
		if((shaderBuildMode&crossplatform::BREAK_ON_FAIL) == crossplatform::BREAK_ON_FAIL)
		{
			BREAK_IF_DEBUGGING
		}
 		if(!IsDebuggerPresent()||(shaderBuildMode&crossplatform::TRY_AGAIN_ON_FAIL)!=crossplatform::TRY_AGAIN_ON_FAIL)
			break;
 	}
	SIMUL_ASSERT(effect&&*effect&&(*effect)->IsValid()==TRUE);

	// Name stuff:
// this is disabled because of lazy loading: the D3D objects are not created yet, so assertions would fail.
#if 0//def _DEBUG
	if(effect)
	{
		ID3DX11Effect *e=*effect;
		if(e)
		{ 
			D3DX11_EFFECT_DESC effect_desc;
			e->GetDesc(&effect_desc);
			for(int i=0;i<(int)effect_desc.Techniques;i++)
			{
				ID3DX11EffectTechnique * pTech = e->GetTechniqueByIndex(i);
				D3DX11_TECHNIQUE_DESC techDesc;
				pTech->GetDesc(&techDesc);
				for(int j=0;j<(int)techDesc.Passes;j++)
				{
					ID3DX11EffectPass * pPass = pTech->GetPassByIndex(j);
					D3DX11_PASS_DESC passDesc;
					pPass->GetDesc(&passDesc);

					D3DX11_PASS_SHADER_DESC vsPassDesc;

					pPass->GetVertexShaderDesc(&vsPassDesc);
					ID3DX11EffectShaderVariable * pVs;
					pVs = vsPassDesc.pShaderVariable->AsShader();
					D3DX11_EFFECT_SHADER_DESC vsDesc;
					pVs->GetShaderDesc(vsPassDesc.ShaderIndex, &vsDesc);
					ID3D11VertexShader *vertexShader=NULL;
					pVs->GetVertexShader(vsPassDesc.ShaderIndex,&vertexShader);
					if(vertexShader)
					{
					//	simul::dx11::SetDebugObjectName(vertexShader,filename_utf8.c_str());
						vertexShader->Release();
					}
				}
			}
		}
	}
#endif
	delete [] macros;
	return hr;
}

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

void BreakIfDebugging()
{
	BREAK_IF_DEBUGGING;
 }

int simul::dx11::ByteSizeOfFormatElement( DXGI_FORMAT format )
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