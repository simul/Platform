// Copyright (c) 2007-2013 Simul Software Ltd
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
#include <tchar.h>
#include "CompileShaderDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/CompileShaderDX1x.h"
#include <string>
static const DWORD default_effect_flags=0;

#include <vector>
#include <iostream>
#include <assert.h>
#include <fstream>
#include <d3dx9.h>
#include "MacrosDX1x.h"
#include <dxerr.h>

#pragma comment(lib,"d3dx9.lib")
#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dx11.lib")
#pragma comment(lib,"Effects11.lib")
#pragma comment(lib,"dxerr.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"d3dcompiler.lib")

// winmm.lib comctl32.lib
static bool pipe_compiler_output=false;

//static ID3D1xDevice		*pd3dDevice		=NULL;
using namespace simul;
using namespace dx11;
using namespace base;
ShaderBuildMode shaderBuildMode=BUILD_IF_CHANGED;

HRESULT __stdcall ShaderIncludeHandler::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
{
	try
	{
		std::string finalPathUtf8;
		switch(IncludeType)
		{
		case D3D_INCLUDE_LOCAL:
			finalPathUtf8	=m_ShaderDirUtf8+"\\"+pFileNameUtf8;
			break;
		case D3D_INCLUDE_SYSTEM:
			finalPathUtf8	=m_SystemDirUtf8+"\\"+pFileNameUtf8;
			break;
		default:
			assert(0);
		}
		void *buf=NULL;
		unsigned fileSize=0;
		simul::base::FileLoader::GetFileLoader()->AcquireFileContents(buf,fileSize,finalPathUtf8.c_str(),false);
		*ppData = buf;
		*pBytes = (UINT)fileSize;
		if(!*ppData)
			return E_FAIL;
		return S_OK;
	}
	catch(std::exception& e)
	{
		std::cerr<<e.what()<<std::endl;
		return E_FAIL;
	}
}

HRESULT __stdcall ShaderIncludeHandler::Close(LPCVOID pData)
{
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents((void*)pData);
	return S_OK;
}

HRESULT __stdcall DetectChangesIncludeHandler::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
{
	try
	{
		std::string finalPathUtf8;
		switch(IncludeType)
		{
		case D3D_INCLUDE_LOCAL:
			finalPathUtf8	=m_ShaderDirUtf8+"\\"+pFileNameUtf8;
			break;
		case D3D_INCLUDE_SYSTEM:
			finalPathUtf8	=m_SystemDirUtf8+"\\"+pFileNameUtf8;
			break;
		default:
			assert(0);
		}
		void *buf=NULL;
		unsigned fileSize=0;
		double dateTimeJdn=simul::base::FileLoader::GetFileLoader()->GetFileDate(finalPathUtf8.c_str());
		if(dateTimeJdn>lastCompileTime)
		{
			anyChanges=true;
			return E_FAIL;
		}
		simul::base::FileLoader::GetFileLoader()->AcquireFileContents(buf,fileSize,finalPathUtf8.c_str(),false);
		*ppData = buf;
		*pBytes = (UINT)fileSize;
		if(!*ppData)
			return E_FAIL;
		return S_OK;
	}
	catch(std::exception& e)
	{
		std::cerr<<e.what()<<std::endl;
		return E_FAIL;
	}
}

HRESULT __stdcall DetectChangesIncludeHandler::Close(LPCVOID pData)
{
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents((void*)pData);
	return S_OK;
}


namespace simul
{
	namespace dx11
	{
		std::vector<std::string> shaderPathsUtf8;
		static std::vector<std::string> texturePathsUtf8;
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
			GetCameraPosVector(view,(float*)cam_pos,(float*)view_dir,y_vertical);
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
		void PushTexturePath(const char *path_utf8)
		{
			texturePathsUtf8.push_back(path_utf8);
		}
		void PopTexturePath()
			{
			texturePathsUtf8.pop_back();
			}
		void MakeInvViewProjMatrix(float *ivp,const float *v,const float *p)
		{
			simul::math::Matrix4x4 view(v),proj(p);
			simul::math::Matrix4x4 vpt;
			simul::math::Matrix4x4 viewproj;
			view(3,0)=view(3,1)=view(3,2)=0;
			simul::math::Multiply4x4(viewproj,view,proj);
			viewproj.Transpose(vpt);
			simul::math::Matrix4x4 invp;
			vpt.Inverse(invp);
			mat4 &invViewProj=*((mat4*)ivp);
			invViewProj=invp;
			invViewProj.transpose();
		}
		void MakeViewProjMatrix(float *vp,const float *v,const float *p)
		{
			D3DXMATRIX viewProj, tmp,view(v),proj(p);
			D3DXMatrixMultiply(&tmp,&view,&proj);
			D3DXMatrixTranspose((D3DXMATRIX*)vp,&tmp);
		}
		void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,const float *w,const float *v,const float *p)
		{
			D3DXMATRIX tmp1,tmp2,view(v),proj(p),world(w);
			D3DXMatrixMultiply(&tmp1,&world,&view);
			D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
			D3DXMatrixTranspose(wvp,&tmp2);
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

ID3D1xShaderResourceView* simul::dx11::LoadTexture(ID3D11Device* pd3dDevice,const char *filename)
{
	ID3D11ShaderResourceView* tex=NULL;
	D3DX11_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory( &loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO) );
	loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	loadInfo.MipLevels=0;
	if(!texturePathsUtf8.size())
		texturePathsUtf8.push_back("media/textures");
	std::string str=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename,texturePathsUtf8);
	
	std::wstring wstr	=simul::base::Utf8ToWString(str);
	HRESULT hr=D3DX11CreateShaderResourceViewFromFileW(
										pd3dDevice,
										wstr.c_str(),
										&loadInfo,
										NULL,
										&tex,
										&hr);
	return tex;
}


ID3D11Texture2D* simul::dx11::LoadStagingTexture(ID3D11Device* pd3dDevice,const char *filename)
{
	D3DX11_IMAGE_LOAD_INFO loadInfo;

    ZeroMemory(&loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO));
	//loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	loadInfo.MipLevels=0;

    loadInfo.Width          = D3DX11_FROM_FILE;
    loadInfo.Height         = D3DX11_FROM_FILE;
     loadInfo.Depth          = D3DX11_FROM_FILE;
	loadInfo.Usage          = D3D11_USAGE_STAGING;
    loadInfo.Format         = (DXGI_FORMAT) D3DX11_FROM_FILE;
    loadInfo.CpuAccessFlags = D3D11_CPU_ACCESS_READ;
	loadInfo.FirstMipLevel  = D3DX11_FROM_FILE;
    loadInfo.MipLevels      = D3DX11_FROM_FILE;
    loadInfo.MiscFlags      = 0;
    loadInfo.MipFilter      = D3DX11_FROM_FILE;
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
#endif
	}
	return tex;
}

ID3D1xTexture1D* simul::dx11::make1DTexture(
							ID3D1xDevice			*m_pd3dDevice
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
							ID3D1xDevice			*m_pd3dDevice
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
							ID3D1xDevice			*m_pd3dDevice
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
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xTexture3D* &tex
							,ID3D11ShaderResourceView* &srv
							,int w,int l,int d
							,DXGI_FORMAT format)
{
	if(tex)
	{
		D3D11_TEXTURE3D_DESC desc;
		tex->GetDesc(&desc);
		if(desc.Width!=w||desc.Height!=l||desc.Depth!=d||desc.Format!=format)
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

void simul::dx11::setDepthState(ID3DX11Effect *effect,const char *name	,ID3D11DepthStencilState * value)
{
	ID3DX11EffectDepthStencilVariable*	var	=effect->GetVariableByName(name)->AsDepthStencil();
	var->SetDepthStencilState(0,value);
}

void simul::dx11::setSamplerState(ID3DX11Effect *effect,const char *name	,ID3D11SamplerState * value)
{
	ID3DX11EffectSamplerVariable*	var	=effect->GetVariableByName(name)->AsSampler();
	var->SetSampler(0,value);
} 

void simul::dx11::setTexture(ID3DX11Effect *effect,const char *name	,ID3D11ShaderResourceView * value)
{
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetResource(value);
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

void simul::dx11::setUnorderedAccessView(ID3DX11Effect *effect,const char *name	,ID3D11UnorderedAccessView * value)
{
	ID3DX11EffectUnorderedAccessViewVariable*	var	=effect->GetVariableByName(name)->AsUnorderedAccessView();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetUnorderedAccessView(value);
}

void simul::dx11::setStructuredBuffer(ID3DX11Effect *effect,const char *name,ID3D11ShaderResourceView * value)
{
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetResource(value);
}

void simul::dx11::setTextureArray(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView *value)
{
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetResource(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float value)
{
	ID3DX11EffectScalarVariable*	var	=effect->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloat(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float x,float y)
{
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	float V[]={x,y,0.f,0.f};
	var->SetFloatVector(V);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float x,float y,float z,float w)
{
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	float V[]={x,y,z,w};
	var->SetFloatVector(V);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,int value)
{
	ID3DX11EffectScalarVariable*	var	=effect->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetInt(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float *value)
{
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloatVector(value);
}

void simul::dx11::setMatrix(ID3DX11Effect *effect,const char *name	,const float *value)
{
	ID3DX11EffectMatrixVariable*	var	=effect->GetVariableByName(name)->AsMatrix();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetMatrix(value);
}

void simul::dx11::setConstantBuffer(ID3DX11Effect *effect	,const char *name	,ID3D11Buffer *b)
{
	ID3DX11EffectConstantBuffer*	pD3DX11EffectConstantBuffer=effect->GetConstantBufferByName(name);
	SIMUL_ASSERT(pD3DX11EffectConstantBuffer->IsValid()!=0);
	pD3DX11EffectConstantBuffer->SetConstantBuffer(b);
}

void simul::dx11::unbindTextures(ID3DX11Effect *effect)
{
	D3DX11_EFFECT_DESC desc;
	effect->GetDesc(&desc);
	for(unsigned i=0;i<desc.GlobalVariables;i++)
	{
		ID3DX11EffectShaderResourceVariable*	srv	=effect->GetVariableByIndex(i)->AsShaderResource();
		if(srv->IsValid())
			srv->SetResource(NULL);
		ID3DX11EffectUnorderedAccessViewVariable*	uav	=effect->GetVariableByIndex(i)->AsUnorderedAccessView();
		if(uav->IsValid())
			uav->SetUnorderedAccessView(NULL);
	}
}

struct d3dMacro
{
	std::string name;
	std::string define;
};

HRESULT WINAPI D3DX11CreateEffectFromBinaryFileUtf8(const char *text_filename_utf8, UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	HRESULT hr=(HRESULT)(-1);
	std::string binary_filename_utf8=std::string(text_filename_utf8)+"o";
	void *pData=NULL;
	unsigned sz=0;
	simul::base::FileLoader::GetFileLoader()->AcquireFileContents(pData,sz,binary_filename_utf8.c_str(),false);
	if(sz>0)
	{
		hr=D3DX11CreateEffectFromMemory(pData,sz,FXFlags,pDevice,ppEffect);
		if(hr!=S_OK)
			std::cerr<<"D3DX11CreateEffectFromBinaryFile error "<<(int)hr<<std::endl;
	}
	else
		std::cerr<<"D3DX11CreateEffectFromBinaryFile cannot find file "<<binary_filename_utf8.c_str()<<std::endl;
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(pData);
	return hr;
}

HRESULT WINAPI D3DX11CreateEffectFromFileUtf8(std::string text_filename_utf8,D3D10_SHADER_MACRO *macros,UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	HRESULT hr=S_OK;
	int pos=(int)text_filename_utf8.find_last_of("/");
	int bpos=(int)text_filename_utf8.find_last_of("\\");
	if(pos<0||bpos>pos)
		pos=bpos;
	std::string path_utf8=text_filename_utf8.substr(0,pos);
	std::string binary_filename_utf8=text_filename_utf8+"o";
	void *textData=NULL;
	unsigned textSize=0;
	simul::base::FileLoader::GetFileLoader()->AcquireFileContents(textData,textSize,text_filename_utf8.c_str(),true);
	// See if there's a binary that's newer than the file date.
	if(shaderBuildMode==BUILD_IF_CHANGED)
	{
		double text_date_jdn	=simul::base::FileLoader::GetFileLoader()->GetFileDate(text_filename_utf8.c_str());
		double binary_date_jdn	=simul::base::FileLoader::GetFileLoader()->GetFileDate(binary_filename_utf8.c_str());
		bool changes_detected=false;
		if(text_date_jdn>binary_date_jdn||!binary_date_jdn)
			changes_detected=true;
		else if(text_date_jdn>0)	// maybe some of the includes have changed?
		{
			ID3DBlob *binaryBlob=NULL;
			ID3DBlob *errorMsgs=NULL;
			DetectChangesIncludeHandler detectChangesIncludeHandler(path_utf8.c_str(),binary_date_jdn);
			hr=D3DPreprocess(	textData	
								,textSize
								,text_filename_utf8.c_str()		//in   LPCSTR pSourceName,
								,macros							//in   const D3D_SHADER_MACRO *pDefines,
								,&detectChangesIncludeHandler	//in   ID3DInclude pInclude,
								,&binaryBlob				//ID3DBlob **ppCodeText,
								,&errorMsgs					//ID3DBlob **ppErrorMsgs
								);
			if(hr!=S_OK||detectChangesIncludeHandler.HasDetectedChanges())
				changes_detected=true;
			if(binaryBlob)
				binaryBlob->Release();
			if(errorMsgs)
				errorMsgs->Release();
		}
		if(!changes_detected&&binary_date_jdn>0)
		{
			hr=D3DX11CreateEffectFromBinaryFileUtf8(text_filename_utf8.c_str(),FXFlags,pDevice,ppEffect);
			if(hr==S_OK)
				return S_OK;
		}
	}
	ID3DBlob *binaryBlob=NULL;
	ID3DBlob *errorMsgs=NULL;
	ShaderIncludeHandler shaderIncludeHandler(path_utf8.c_str(),"");
	hr=D3DCompile(		textData,
		textSize,
		text_filename_utf8.c_str(),		//LPCSTR pSourceName,
		macros,		//const D3D_SHADER_MACRO *pDefines,
		&shaderIncludeHandler,		//ID3DInclude *pInclude,
		NULL,		//LPCSTR pEntrypoint,
		"fx_5_0",	//LPCSTR pTarget,
		0,	//UINT Flags1,
		FXFlags,	//UINT Flags2,
		&binaryBlob,//ID3DBlob **ppCode,
		&errorMsgs	//ID3DBlob **ppErrorMsgs
		);
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

HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3DX11Effect **effect,const char *filename)
{
	std::map<std::string,std::string> defines;
	return CreateEffect(d3dDevice,effect,filename,defines);
}

ID3D11ComputeShader *LoadComputeShader(ID3D1xDevice *pd3dDevice,const char *filename_utf8)
{
	if(!shaderPathsUtf8.size())
		shaderPathsUtf8.push_back(std::string("media/hlsl/dx11"));
	std::string fn;
	for(int i=(int)shaderPathsUtf8.size()-1;i>=0;i--)
	{
		fn=(shaderPathsUtf8[i]+"/")+filename_utf8;
		if(FileExists(fn))
			break;
	}
	if(!FileExists(fn))
		return NULL;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	LPCSTR pProfile = (pd3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ) ? "cs_5_0" : "cs_4_0";

	ID3DBlob* pErrorBlob = NULL;
	ID3DBlob* pBlob = NULL;
	HRESULT hr = D3DX11CompileFromFileW(simul::base::Utf8ToWString(fn.c_str()).c_str(), NULL, NULL, "main", pProfile, dwShaderFlags, NULL, NULL, &pBlob, &pErrorBlob, NULL );
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

HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3DX11Effect **effect,const char *filenameUtf8,const std::map<std::string,std::string>&defines)
{
	SIMUL_ASSERT(d3dDevice!=NULL);
	HRESULT hr=S_OK;
	std::string text_filename=(filenameUtf8);
	std::string filename_utf8;
	for(int i=(int)shaderPathsUtf8.size()-1;i>=0;i--)
	{
		filename_utf8=(shaderPathsUtf8[i]+"/")+filenameUtf8;
		if(FileExists(filename_utf8))
			break;
	}
	if(!FileExists(filename_utf8))
		return S_FALSE;
	
	D3D10_SHADER_MACRO *macros=NULL;
	std::vector<std::string> d3dmacros;
	{
		size_t num_defines=defines.size();
		if(num_defines)
		{
			macros=new D3D10_SHADER_MACRO[num_defines+1];
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
	DWORD flags=default_effect_flags;
	SAFE_RELEASE(*effect);
	hr=1;
	while(hr!=S_OK)
	{
		hr=D3DX11CreateEffectFromFileUtf8(	filename_utf8,
											macros,
											flags,
											d3dDevice,
											effect);
		if(hr==S_OK)
			break;
		std::string err="";
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateEffect", hr );
#endif
		DebugBreak();
	}
	assert((*effect)->IsValid());

	// Name stuff:
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
	delete [] macros;
	return hr;
}

ID3DX11Effect *LoadEffect(ID3D1xDevice *d3dDevice,const char *filename_utf8)
{
	std::map<std::string,std::string> defines;
	return LoadEffect(d3dDevice,filename_utf8,defines);
}

ID3DX11Effect *LoadEffect(ID3D1xDevice *d3dDevice,const char *filename_utf8,const std::map<std::string,std::string>&defines)
{
	ID3DX11Effect *effect=NULL;
	CreateEffect(d3dDevice,&effect,filename_utf8,defines);
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

void MakeCubeMatrices(D3DXMATRIX mat[],const float *cam_pos,bool ReverseDepth)
{
    D3DXVECTOR3 vEyePt = D3DXVECTOR3( cam_pos );
    D3DXVECTOR3 vLookAt;
    D3DXVECTOR3 vUpDir;
    ZeroMemory(mat,6*sizeof(D3DXMATRIX) );
    /*D3DCUBEMAP_FACE_POSITIVE_X     = 0,
    D3DCUBEMAP_FACE_NEGATIVE_X     = 1,
    D3DCUBEMAP_FACE_POSITIVE_Y     = 2,
    D3DCUBEMAP_FACE_NEGATIVE_Y     = 3,
    D3DCUBEMAP_FACE_POSITIVE_Z     = 4,
    D3DCUBEMAP_FACE_NEGATIVE_Z     = 5,*/
	static const D3DVECTOR lookf[6]=
	{
		 {1.f,0.f,0.f}		,{-1.f,0.f,0.f}
		,{0.f,-1.f,0.f}		,{0.f,1.f,0.f}
		,{0.f,0.f,-1.f}		,{0.f,0.f,1.f}
	};
	static const D3DVECTOR upf[6]=
	{
		 {0.f,1.f,0.f}		,{0.f,1.f,0.f}
		,{0.f,0.f,-1.f}		,{0.f,0.f,1.f}
		,{0.f,1.f,0.f}		,{0.f,1.f,0.f}
	};
	static const D3DVECTOR lookr[6]=
	{
		 {-1.f,0.f,0.f}		,{1.f,0.f,0.f}
		,{0.f,-1.f,0.f}		,{0.f,1.f,0.f}
		,{0.f,0.f,-1.f}		,{0.f,0.f,1.f}
	};
	static const D3DVECTOR upr[6]=
	{
		 {0.f,-1.f,0.f}		,{0.f,-1.f,0.f}
		,{0.f,0.f,1.f}		,{0.f,0.f,-1.f}
		,{0.f,-1.f,0.f}		,{0.f,-1.f,0.f}
	};
	for(int i=0;i<6;i++)
	{
		vLookAt		=vEyePt+lookf[i];
		vUpDir		=upf[i];
		D3DXMatrixLookAtLH(&mat[i], &vEyePt,&vLookAt, &vUpDir );
		if(true)//everseDepth)
		{
			vLookAt		=vEyePt+lookr[i];
			vUpDir		=upr[i];
			D3DXMatrixLookAtRH(&mat[i], &vEyePt, &vLookAt, &vUpDir );
		}
	}
}

void BreakIfDebugging()
{
	DebugBreak();
}

// Stored states
static ID3D11DepthStencilState* m_pDepthStencilStateStored11=NULL;
static UINT m_StencilRefStored11;
static ID3D11RasterizerState* m_pRasterizerStateStored11=NULL;
static ID3D11BlendState* m_pBlendStateStored11=NULL;
static float m_BlendFactorStored11[4];
static UINT m_SampleMaskStored11;
static ID3D11SamplerState* m_pSamplerStateStored11=NULL;

void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMGetDepthStencilState( &m_pDepthStencilStateStored11, &m_StencilRefStored11 );
	SetDebugObjectName(m_pDepthStencilStateStored11,"m_pDepthStencilStateStored11");
    pd3dImmediateContext->RSGetState( &m_pRasterizerStateStored11 );
	SetDebugObjectName(m_pRasterizerStateStored11,"m_pRasterizerStateStored11");
    pd3dImmediateContext->OMGetBlendState( &m_pBlendStateStored11, m_BlendFactorStored11, &m_SampleMaskStored11 );
	SetDebugObjectName(m_pBlendStateStored11,"m_pBlendStateStored11");
    pd3dImmediateContext->PSGetSamplers( 0, 1, &m_pSamplerStateStored11 );
	SetDebugObjectName(m_pSamplerStateStored11,"m_pSamplerStateStored11");
}

void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilStateStored11, m_StencilRefStored11 );
    pd3dImmediateContext->RSSetState( m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMSetBlendState( m_pBlendStateStored11, m_BlendFactorStored11, m_SampleMaskStored11 );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSamplerStateStored11 );

    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
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