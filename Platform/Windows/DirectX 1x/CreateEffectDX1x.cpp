// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.cpp Create a DirectX .fx effect and report errors.

#include "CreateEffectDX1x.h"
#include "Simul/Base/StringToWString.h"
#pragma comment (lib, "d3dcompiler.lib")

#include <tchar.h>
#include <string>
typedef std::basic_string<TCHAR> tstring;
static tstring shader_path=TEXT("");
static tstring texture_path=TEXT("");
static DWORD default_effect_flags=0;

#include <vector>
#include <iostream>
#include <assert.h>
#include <fstream>
#include "MacrosDX1x.h"
#include <dxerr.h>
static bool shader_path_set=false;
static bool texture_path_set=false;

void SetShaderPath(const char *path)
{
#ifdef UNICODE
	// tstring and TEXT cater for the confusion between wide and regular strings.
	shader_path.resize(strlen(path),L' '); // Make room for characters
	// Copy string to wstring.
	std::copy(path,path+strlen(path),shader_path.begin());

	shader_path+=L"/";
#else
	shader_path=path;
	shader_path+="/";
#endif
	shader_path_set=true;
}
void SetTexturePath(const char *path)
{
#ifdef UNICODE
	// tstring and TEXT cater for the confusion between wide and regular strings.
	texture_path.resize(strlen(path),L' '); // Make room for characters
	// Copy string to wstring.
	std::copy(path,path+strlen(path),texture_path.begin());
	texture_path+=L"/";
#else
	texture_path=path;
	texture_path+="/";
#endif
	texture_path_set=true;
}
struct d3dMacro
{
	std::string name;
	std::string define;
};
#ifdef DX11
HRESULT WINAPI D3DX11CreateEffectFromFile(const TCHAR *filename, UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	char *pData;
	std::string fn=simul::base::WStringToString(filename)+"o";
	std::ifstream ifs(fn.c_str(),std::ios_base::binary);
	if(!ifs.good())
		return S_FALSE;
	ifs.seekg(0,std::ios_base::end);
	size_t sz=ifs.tellg();
	ifs.seekg(0,std::ios_base::beg);
	if(sz<=0)
		return S_FALSE;
	pData=new char[sz];
	ifs.read(pData,sz);
	HRESULT hr=D3DX11CreateEffectFromMemory(pData,sz,FXFlags,pDevice,ppEffect);
	delete [] pData;
	return hr;
}
#define ID3D1xDevice ID3D11Device 
#define ID3D1xEffect ID3DX11Effect
#else
#define ID3D1xDevice ID3D10Device
#define ID3D1xEffect ID3D10Effect
#endif
/*HRESULT WINAPI D3DX10CreateEffectFromFile(const TCHAR *filename, UINT FXFlags, ID3D10Device *pDevice, ID3D10Effect **ppEffect)
{
	char *pData;
	tstring fn=shader_path+_T("/");
	fn+=(filename);
	fn+=_T("o");
	std::ifstream ifs(fn.c_str(),std::ios_base::binary);
	if(!ifs.good())
		return S_FALSE;
	ifs.seekg(0,std::ios_base::end);
	size_t sz=ifs.tellg();
	ifs.seekg(0,std::ios_base::beg);
	if(sz<=0)
		return S_FALSE;
	pData=new char[sz];
	ifs.read(pData,sz);
	HRESULT hr=D3D10CreateEffectFromMemory(pData,sz,FXFlags,pDevice,NULL,ppEffect);
	delete [] pData;
	return hr;
}*/

HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename)
{
	std::map<std::string,std::string> defines;
	return CreateEffect(d3dDevice,effect,filename,defines);
}

HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename,const std::map<std::string,std::string>&defines)
{
	HRESULT hr=S_OK;
	std::cout<<_T("Create effect: ")<<filename<<std::endl;
	//tstring fn=filename;
	tstring fn=shader_path+filename;
	
	D3D10_SHADER_MACRO *macros=NULL;
	std::vector<std::string> d3dmacros;
	{
		bool name=true;
		const char *txt=NULL;
		size_t num_defines=defines.size();
		if(num_defines)
		{
			macros=new D3D10_SHADER_MACRO[num_defines+1];
			macros[num_defines].Definition=0;
			macros[num_defines].Name=0;
		}
		va_list marker;
		va_start(marker,num_defines);
		size_t def=0;
		for(int i=0;i<num_defines*2;i++)
		{
			txt=va_arg(marker,const char *);
			if(name)
			{
				macros[def].Name=txt;
			}
			else
			{
				macros[def].Definition=txt;
				def++;
			}
			name=!name;
		}
		va_end(marker); 
	}
	DWORD flags=default_effect_flags;
	SAFE_RELEASE(*effect);

#ifdef DX10
	ID3D10Blob *errors;
	if(FAILED(
		hr=D3DX10CreateEffectFromFile(
				  fn.c_str(),
				  macros,
				  NULL,
				  "fx_4_0",
				  flags,
				  flags,
				  d3dDevice,
				  NULL,
				  NULL,
				  effect,
				  &errors,
				  0
				)))
#else
	if(FAILED(
		hr=D3DX11CreateEffectFromFile(
				fn.c_str(),
				flags,
				d3dDevice,
				effect)))
#endif
	{
		std::string err="";
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateEffect", hr );
#endif
#ifdef DX10
		if(errors)
			std::cerr<<(char*)errors->GetBufferPointer()<<std::endl;
#endif
		//errors->GetBufferSize();

		DebugBreak();
	}
	else
	{
		std::string err="";
	}
	assert((*effect)->IsValid());
	delete [] macros;
	return hr;
}


ID3D1xDevice *m_pd3dDevice=NULL;
ID3D1xDeviceContext*			m_pImmediateContext=NULL;

void SetDevice(ID3D1xDevice* dev)
{
	m_pd3dDevice=dev;
#ifdef DX10
	m_pImmediateContext=dev;
#else
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
}


HRESULT Map2D(ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp)
{
#ifdef DX10
	return tex->Map(0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#else
	return m_pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#endif
}

HRESULT Map3D(ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp)
{
#ifdef DX10
	return tex->Map(0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#else
	return m_pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#endif
}

HRESULT Map1D(ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp)
{
#ifdef DX10
	return tex->Map(0,D3D1x_MAP_WRITE_DISCARD,0,&(mp->pData));
#else
	return m_pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#endif
}

void Unmap2D(ID3D1xTexture2D *tex)
{
#ifdef DX10
	tex->Unmap(0);
#else
	m_pImmediateContext->Unmap(tex,0);
#endif
}

void Unmap3D(ID3D1xTexture3D *tex)
{
#ifdef DX10
	tex->Unmap(0);
#else
	m_pImmediateContext->Unmap(tex,0);
#endif
}

void Unmap1D(ID3D1xTexture1D *tex)
{
#ifdef DX10
	tex->Unmap(0);
#else
	m_pImmediateContext->Unmap(tex,0);
#endif
}

#ifdef DX10
HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,void **vert)
#else
HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE*vert)
#endif
{
#ifdef DX10
	return vertexBuffer->Map(D3D1x_MAP_WRITE_DISCARD,0,vert);
#else
	return m_pImmediateContext->Map(vertexBuffer,0,D3D1x_MAP_WRITE_DISCARD,0,vert);
#endif
}

void UnmapBuffer(ID3D1xBuffer *vertexBuffer)
{
#ifdef DX10
	vertexBuffer->Unmap();
#else
	m_pImmediateContext->Unmap(vertexBuffer,0);
#endif
}


HRESULT ApplyPass(ID3D1xEffectPass *pass)
{
#ifdef DX10
	return pass->Apply(0);
#else
	return pass->Apply(0,m_pImmediateContext);
#endif
}