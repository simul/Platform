// Copyright (c) 2007-2014 Simul Software Ltd
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
#include "Simul/Sky/Float4.h"
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
	#pragma comment(lib,"d3dx9.lib")
	#pragma comment(lib,"d3d9.lib")
	#pragma comment(lib,"d3dx11.lib")
	#pragma comment(lib,"dxerr.lib")
	#pragma comment(lib,"Effects11_DXSDK.lib")
#else
	#ifndef _XBOX_ONE
		#pragma comment(lib,"Effects11_Win8SDK.lib")
	#else
		#pragma comment(lib,"Effects11.lib")
	#endif
	#pragma comment(lib,"directxtex.lib")
#endif
#ifdef _XBOX_ONE
	#pragma comment(lib,"d3d11_x.lib")
	#pragma comment(lib,"d3dcompiler.lib")
#else
	#pragma comment(lib,"dxgi.lib")
	#pragma comment(lib,"d3d11.lib")
	#pragma comment(lib,"dxguid.lib")
#if defined(SIMUL_WIN8_SDK) && defined(WIN64)
	#pragma comment(lib,"d3dcompiler_xdk.lib")
#else
	#pragma comment(lib,"d3dcompiler.lib")
#endif
#endif

// winmm.lib comctl32.lib
static bool pipe_compiler_output=false;

//static ID3D11Device		*pd3dDevice		=NULL;
using namespace simul;
using namespace dx11;
using namespace base;
ShaderBuildMode shaderBuildMode=BUILD_IF_CHANGED;


namespace simul
{
	namespace dx11
	{
		std::vector<std::string> shaderPathsUtf8;
		std::vector<std::string> texturePathsUtf8;
		std::string shaderbinPathUtf8="shaderbin\\";
		void GetCameraPosVector(const float *v,float *dcam_pos,float *view_dir,bool y_vertical)
		{
			D3DXMATRIX tmp1,view(v);
			D3DXMatrixInverse(&tmp1,NULL,&view);
			
			dcam_pos[0]=tmp1._41;
			dcam_pos[1]=tmp1._42;
			dcam_pos[2]=tmp1._43;
			if(view_dir)
			{
				if(y_vertical)
				{
					view_dir[0]=view._13;
					view_dir[1]=view._23;
					view_dir[2]=view._33;
				}
				else
				{
					view_dir[0]=-view._13;
					view_dir[1]=-view._23;
					view_dir[2]=-view._33;
				}
			}
		}
		const float *GetCameraPosVector(const float *v,bool y_vertical)
		{
			D3DXMATRIX view(v);
			static float cam_pos[4],view_dir[4];
			GetCameraPosVector(&view._11,(float*)cam_pos,(float*)view_dir,y_vertical);
			return cam_pos;
		}
		void PipeCompilerOutput(bool p)
		{
			pipe_compiler_output=p;
		}
		void SetShaderBuildMode( ShaderBuildMode b)
		{
			shaderBuildMode=b;
		}
		void PushShaderPath(const char *path_utf8)
		{
			shaderPathsUtf8.push_back(std::string(path_utf8)+"/");
		}
		void PopShaderPath()
		{
			shaderPathsUtf8.pop_back();
		}
		void SetShaderBinaryPath(const char *path_utf8)
		{
			shaderbinPathUtf8=path_utf8;
			shaderbinPathUtf8+='\\';
		}
		void PushTexturePath(const char *path_utf8)
		{
			texturePathsUtf8.push_back(path_utf8);
		}
		void PopTexturePath()
		{ 
			texturePathsUtf8.pop_back();
		}
		std::vector<std::string> GetTexturePathsUtf8()
		{
			return texturePathsUtf8;
		}
		D3DXMATRIX ConvertReversedToRegularProjectionMatrix(const D3DXMATRIX &proj)
		{
			D3DXMATRIX p=proj;
			if(proj._43>0)
			{
				float zF=proj._43/proj._33;
				float zN=proj._43*zF/(zF+proj._43);
				p._33=-zF/(zF-zN);
				p._43=-zN*zF/(zF-zN);
			}
			return p;
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

ID3D11ShaderResourceView* simul::dx11::LoadTexture(ID3D11Device* pd3dDevice,const char *filename)
{
	ID3D11ShaderResourceView* tex=NULL;
	
	if(!texturePathsUtf8.size())
		texturePathsUtf8.push_back("media/textures");
	std::string str		=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename,texturePathsUtf8);
	std::wstring wstr	=simul::base::Utf8ToWString(str);
#if WINVER<0x602
	D3DX11_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory(&loadInfo,sizeof(D3DX11_IMAGE_LOAD_INFO));
	loadInfo.BindFlags	=D3D11_BIND_SHADER_RESOURCE;
	loadInfo.Format		=DXGI_FORMAT_FROM_FILE;
	loadInfo.MipLevels=0;
	HRESULT hr			=D3DX11CreateShaderResourceViewFromFileW(
									pd3dDevice,
									wstr.c_str(),
									&loadInfo,
									NULL,
									&tex,
									&hr);
#else
	int flags=0;
	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratchImage;
	DirectX::LoadFromWICFile(wstr.c_str(),flags,&metadata,scratchImage);
	const DirectX::Image *image=scratchImage.GetImage(0,0,0);
	if(!image)
		return NULL;
    HRESULT hr=CreateShaderResourceView(  pd3dDevice,image, 1, metadata,&tex );
#endif
	return tex;
}


ID3D11Texture2D* simul::dx11::LoadStagingTexture(ID3D11Device* pd3dDevice,const char *filename)
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

	ID3D11Texture2D *tex=NULL;
	if(!texturePathsUtf8.size())
		texturePathsUtf8.push_back("media/textures");
	for(int i=0;i<(int)texturePathsUtf8.size();i++)
	{
		std::wstring wstr	=simul::base::Utf8ToWString((texturePathsUtf8[i]+"/")+filename);
		HRESULT hr=D3DX11CreateTextureFromFileW(pd3dDevice,wstr.c_str(),&loadInfo, NULL, ( ID3D11Resource** )&tex, &hr );
		if(hr==S_OK)
			break;
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"LoadStagingTexture", hr );
#else
		std::cerr<<"Failed to load texture: "<<wstr.c_str()<<std::endl;
#endif
	}
	return tex;
}

ID3D1xTexture1D* simul::dx11::make1DTexture(
							ID3D11Device			*m_pd3dDevice
							,int w
							,DXGI_FORMAT format
							,const float *src)
{
	ID3D1xTexture1D*	tex;
	D3D11_TEXTURE1D_DESC textureDesc=
	{
		w,
		1,
		1,
		format,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		(UINT)(w*ByteSizeOfFormatElement(format)),
		(UINT)(w*ByteSizeOfFormatElement(format))
	};

	m_pd3dDevice->CreateTexture1D(&textureDesc,&init,&tex);
	return tex;
}

ID3D11Texture2D* simul::dx11::make2DTexture(
							ID3D11Device			*m_pd3dDevice
							,int w,int h
							,DXGI_FORMAT format
							,const float *src)
{
	ID3D1xTexture2D*	tex;
	D3D11_TEXTURE2D_DESC textureDesc=
	{
		w,h,
		1,
		1,
		format,
		{1,0}
		,D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		(UINT)w*ByteSizeOfFormatElement(format),
		(UINT)w*h*ByteSizeOfFormatElement(format)
	};
	m_pd3dDevice->CreateTexture2D(&textureDesc,&init,&tex);
	return tex;
}


ID3D1xTexture3D* simul::dx11::make3DTexture(
							ID3D11Device			*m_pd3dDevice
							,int w,int l,int d
							,DXGI_FORMAT format
							,const void *src)
{
	ID3D1xTexture3D*	tex;
	D3D11_TEXTURE3D_DESC textureDesc=
	{
		w,l,d
		,1
		,format
		,D3D11_USAGE_DYNAMIC
		,D3D11_BIND_SHADER_RESOURCE
		,D3D11_CPU_ACCESS_WRITE
		,0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		w*ByteSizeOfFormatElement(format),
		w*l*ByteSizeOfFormatElement(format)
	};
	m_pd3dDevice->CreateTexture3D(&textureDesc,src?&init:NULL,&tex);
	return tex;
}
							
void simul::dx11::Ensure3DTextureSizeAndFormat(
							ID3D11Device			*m_pd3dDevice
							,ID3D1xTexture3D* &tex
							,ID3D11ShaderResourceView* &srv
							,int w,int l,int d
							,DXGI_FORMAT format)
{
	if(tex)
	{
		D3D11_TEXTURE3D_DESC desc;
		tex->GetDesc(&desc);
		if((int)desc.Width!=w||(int)desc.Height!=l||(int)desc.Depth!=d||desc.Format!=format)
		{
			SAFE_RELEASE(tex);
			SAFE_RELEASE(srv);
		}
	}

	if(!tex)
	{
		tex=make3DTexture(	m_pd3dDevice,w,l,d,format,NULL);
		m_pd3dDevice->CreateShaderResourceView(tex,NULL,&srv);
		return;
	}
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
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetResource(value);
	if(var->IsValid()!=0)
		return true;
	return false;
}

void simul::dx11::applyPass(ID3D11DeviceContext *pContext,ID3DX11Effect *effect,const char *name,int pass_num)
{
	ID3DX11EffectTechnique *tech	=effect->GetTechniqueByName(name);
	if(!tech)
		SIMUL_THROW("Technique not found");
	ID3DX11EffectPass *pass			=tech->GetPassByIndex(pass_num);
	if(!pass->IsValid())
		SIMUL_THROW("Pass not found");
	HRESULT hr=pass->Apply(0,pContext);
	V_CHECK(hr);
}

void simul::dx11::applyPass(ID3D11DeviceContext *pContext,ID3DX11Effect *effect,const char *name,const char *passname)
{
	ID3DX11EffectTechnique *tech	=effect->GetTechniqueByName(name);
	if(!tech)
		SIMUL_THROW("Technique not found");
	ID3DX11EffectPass *pass			=tech->GetPassByName(passname);
	if(!pass->IsValid())
		SIMUL_THROW("Pass not found");
	HRESULT hr=pass->Apply(0,pContext);
	V_CHECK(hr);
}

bool simul::dx11::setUnorderedAccessView(ID3DX11Effect *effect,const char *name	,ID3D11UnorderedAccessView * value)
{
	ID3DX11EffectUnorderedAccessViewVariable*	var	=effect->GetVariableByName(name)->AsUnorderedAccessView();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetUnorderedAccessView(value);
	if(value&&var->IsValid()!=0)
		return true;
	return false;
}

void simul::dx11::setStructuredBuffer(ID3DX11Effect *effect,const char *name,ID3D11ShaderResourceView * value)
{
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetResource(value);
}

void simul::dx11::setTextureArray(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView *value)
{
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetResource(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float value)
{
	ID3DX11EffectScalarVariable*	var	=effect->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
	var->SetFloat(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float x,float y)
{
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

void simul::dx11::setConstantBuffer(ID3DX11Effect *effect	,const char *name	,ID3D11Buffer *b)
{
	ID3DX11EffectConstantBuffer*	pD3DX11EffectConstantBuffer=effect->GetConstantBufferByName(name);
	SIMUL_ASSERT_WARN(pD3DX11EffectConstantBuffer->IsValid()!=0,(std::string("Invalid constant buffer: ")+name).c_str());
	pD3DX11EffectConstantBuffer->SetConstantBuffer(b);
}

void simul::dx11::unbindTextures(ID3DX11Effect *effect)
{
	D3DX11_EFFECT_DESC desc;
	effect->GetDesc(&desc);
	for(unsigned i=0;i<desc.GlobalVariables;i++)
	{
		ID3DX11EffectVariable *var	=effect->GetVariableByIndex(i);
		D3DX11_EFFECT_VARIABLE_DESC desc;
		var->GetDesc(&desc);
		ID3DX11EffectType *s=var->GetType();
		//if(var->IsShaderResource())
		{
			ID3DX11EffectShaderResourceVariable*	srv	=var->AsShaderResource();
			if(srv->IsValid())
				srv->SetResource(NULL);
		}
		//if(var->IsUnorderedAccessView())
		{
			ID3DX11EffectUnorderedAccessViewVariable*	uav	=effect->GetVariableByIndex(i)->AsUnorderedAccessView();
			if(uav->IsValid())
				uav->SetUnorderedAccessView(NULL);
		}
	}
//	effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0,
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
		hr=D3DX11CreateEffectFromMemory(pData,sz,FXFlags,pDevice,ppEffect);
		if(hr!=S_OK)
			std::cerr<<"D3DX11CreateEffectFromBinaryFile error "<<(int)hr<<std::endl;
	}
	else
		std::cerr<<"D3DX11CreateEffectFromBinaryFile cannot find file "<<binary_filename_utf8<<std::endl;
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(pData);
	return hr;
}

static double GetNewestIncludeFileDate(std::string text_filename_utf8,void *textData,size_t textSize,D3D_SHADER_MACRO *macros,double binary_date_jdn=0.0)
{
	ID3DBlob *binaryBlob=NULL;
	ID3DBlob *errorMsgs=NULL;
	int pos=(int)text_filename_utf8.find_last_of("/");
	int bpos=(int)text_filename_utf8.find_last_of("\\");
	if(pos<0||bpos>pos)
		pos=bpos;
	std::string path_utf8=text_filename_utf8.substr(0,pos);
	DetectChangesIncludeHandler detectChangesIncludeHandler(path_utf8.c_str(),binary_date_jdn);
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
	return newestFileTime;
}

HRESULT WINAPI D3DX11CreateEffectFromFileUtf8(std::string text_filename_utf8,D3D_SHADER_MACRO *macros,UINT ShaderFlags,UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
ERRNO_CHECK
	HRESULT hr=S_OK;
	int pos=(int)text_filename_utf8.find_last_of("/");
	int bpos=(int)text_filename_utf8.find_last_of("\\");
	if(pos<0||bpos>pos)
		pos=bpos;
	std::string path_utf8=text_filename_utf8.substr(0,pos);
	int dot=(int)text_filename_utf8.find_last_of(".");
	if(dot<0)
		dot=(int)text_filename_utf8.length();
	std::string name_utf8=text_filename_utf8.substr(pos+1,dot-pos-1);
	std::string binary_filename_utf8=(shaderbinPathUtf8)+name_utf8;
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
	if(shaderBuildMode!=NEVER_BUILD)
		simul::base::FileLoader::GetFileLoader()->AcquireFileContents(textData,textSize,text_filename_utf8.c_str(),true);
ERRNO_CHECK
	// See if there's a binary that's newer than the file date.
	bool changes_detected=(shaderBuildMode==ALWAYS_BUILD);
	double binary_date_jdn=0.0;
	//std::cout<<"Checking DX11 shader "<<text_filename_utf8.c_str()<<std::endl;
	if(shaderBuildMode==BUILD_IF_CHANGED)
	{
		double text_date_jdn	=simul::base::FileLoader::GetFileLoader()->GetFileDate(text_filename_utf8.c_str());
		binary_date_jdn			=simul::base::FileLoader::GetFileLoader()->GetFileDate(binary_filename_utf8.c_str());
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
			double newest_included_file=GetNewestIncludeFileDate(text_filename_utf8,textData,textSize,macros,binary_date_jdn);
			if(hr!=S_OK||newest_included_file>binary_date_jdn)
				changes_detected=true;
		}
	}
	if(shaderBuildMode==NEVER_BUILD||!changes_detected&&binary_date_jdn>0)
	{
		hr=D3DX11CreateEffectFromBinaryFileUtf8(binary_filename_utf8.c_str(),FXFlags,pDevice,ppEffect);
		if(hr==S_OK)
			return S_OK;
		if(shaderBuildMode==NEVER_BUILD)
			return S_FALSE;
	}
	ID3DBlob *binaryBlob	=NULL;
	ID3DBlob *errorMsgs		=NULL;
ERRNO_CHECK
	ShaderIncludeHandler shaderIncludeHandler(path_utf8.c_str(),"");
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
	if(shaderBuildMode!=NEVER_BUILD)
		simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(textData);
	if(hr==S_OK)
	{
		hr=D3DX11CreateEffectFromMemory(binaryBlob->GetBufferPointer(),binaryBlob->GetBufferSize(),FXFlags,pDevice,ppEffect);
		if(hr==S_OK)
		{
			simul::base::FileLoader::GetFileLoader()->Save(binaryBlob->GetBufferPointer(),(unsigned int)binaryBlob->GetBufferSize(),binary_filename_utf8.c_str(),false);
		}
	}
	else if(errorMsgs)
	{
		char *errs=(char*)errorMsgs->GetBufferPointer();
		std::string err(errs);
		int pos=0;
		while(pos>=0&&pos<(int)err.length())
		{
			int last=pos;
			pos=(int)err.find("\n",pos+1);
			std::string line=err.substr(last,pos-last);
			if(line.find(":")>3)
			{
				std::string path=path_utf8;
				char full[_MAX_PATH];
				if( _fullpath( full, path_utf8.c_str(), _MAX_PATH ) != NULL )
					path=full;
				path+="/";
				line=path+line;
			}
			std::cerr<<line.c_str()<<std::endl;
		};//text_filename_utf8.c_str()<<
	}
	if(binaryBlob)
		binaryBlob->Release();
	if(errorMsgs)
		errorMsgs->Release();
	return hr;
}

HRESULT simul::dx11::CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const char *filename)
{
	std::map<std::string,std::string> defines;
	return simul::dx11::CreateEffect(d3dDevice,effect,filename,defines);
}

ID3D11ComputeShader *simul::dx11::LoadComputeShader(ID3D11Device *pd3dDevice,const char *filename_utf8)
{
	if(!shaderPathsUtf8.size())
		shaderPathsUtf8.push_back(std::string("media/hlsl/dx11"));
	std::string fn=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_utf8,shaderPathsUtf8);
	if(!simul::base::FileLoader::GetFileLoader()->FileExists(fn.c_str()))
		return NULL;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	LPCSTR pProfile = (pd3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ) ? "cs_5_0" : "cs_4_0";

	ID3DBlob* pErrorBlob = NULL;
	ID3DBlob* pBlob = NULL;
#if WINVER<0x602
	HRESULT hr = D3DX11CompileFromFileW(simul::base::Utf8ToWString(fn.c_str()).c_str(), NULL, NULL, "main", pProfile, dwShaderFlags, NULL, NULL, &pBlob, &pErrorBlob, NULL );
#else
	HRESULT hr=D3DCompileFromFile(simul::base::Utf8ToWString(fn.c_str()).c_str(), NULL, NULL,"main", pProfile, dwShaderFlags, NULL, &pBlob,&pErrorBlob);
#endif
	if ( FAILED(hr) )
	{
		if ( pErrorBlob )
			OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
		if(pErrorBlob)
			pErrorBlob->Release();
		if(pBlob)
			pBlob->Release();

		return NULL;
	}
	else
	{
		ID3D11ComputeShader *computeShader;
		hr = pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL,&computeShader );
		if(pErrorBlob)
			pErrorBlob->Release();
		if(pBlob)
			pBlob->Release();

		return computeShader;
	}
}

HRESULT simul::dx11::CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const char *filenameUtf8,const std::map<std::string,std::string>&defines,unsigned int shader_flags)
{
	SIMUL_ASSERT_WARN(d3dDevice!=NULL,"Null device");
	HRESULT hr=S_OK;
	std::string filename_utf8=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filenameUtf8,shaderPathsUtf8);
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
											effect);
		if(hr==S_OK)
			break;
		std::string err="";
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateEffect", hr );
#endif
		BREAK_IF_DEBUGGING;
 	}
	assert((*effect)->IsValid());

	// Name stuff:
#ifdef _DEBUG
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
					simul::dx11::SetDebugObjectName(vertexShader,filename_utf8.c_str());
					vertexShader->Release();
				}
			}
		}
	}
#endif
	delete [] macros;
	return hr;
}

ID3DX11Effect *LoadEffect(ID3D11Device *d3dDevice,const char *filename_utf8)
{
	std::map<std::string,std::string> defines;
	return LoadEffect(d3dDevice,filename_utf8,defines);
}

ID3DX11Effect *LoadEffect(ID3D11Device *d3dDevice,const char *filename_utf8,const std::map<std::string,std::string>&defines)
{
	ID3DX11Effect *effect=NULL;
	simul::dx11::CreateEffect(d3dDevice,&effect,filename_utf8,defines);
	return effect;
}

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif


HRESULT Map2D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp)
{
	return pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
}

HRESULT Map3D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp)
{
	return pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
}

HRESULT Map1D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp)
{
	return pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
}

void Unmap2D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture2D *tex)
{
	pImmediateContext->Unmap(tex,0);
}

void Unmap3D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture3D *tex)
{
	pImmediateContext->Unmap(tex,0);
}

void Unmap1D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture1D *tex)
{
	pImmediateContext->Unmap(tex,0);
}

HRESULT MapBuffer(ID3D11DeviceContext *pImmediateContext,ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE*vert)
{
	return pImmediateContext->Map(vertexBuffer,0,D3D1x_MAP_WRITE_DISCARD,0,vert);
}

void UnmapBuffer(ID3D11DeviceContext *pImmediateContext,ID3D1xBuffer *vertexBuffer)
{
	pImmediateContext->Unmap(vertexBuffer,0);
}

HRESULT ApplyPass(ID3D11DeviceContext *pImmediateContext,ID3DX11EffectPass *pass)
{
	return pass->Apply(0,pImmediateContext);
}

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