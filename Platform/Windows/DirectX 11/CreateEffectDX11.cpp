// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.cpp Create a DirectX .fx effect and report errors.

#include "CreateEffectDX11.h"
#include "Simul/Base/StringToWString.h"
#pragma comment (lib, "d3dcompiler.lib")
#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
#else
	#include <tchar.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static DWORD default_effect_flags=0;
#endif
	#include <vector>
#include <iostream>
	#include <assert.h>
#include <fstream>
#include "Macros.h"

struct d3dMacro
{
	std::string name;
	std::string define;
};
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

HRESULT CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const TCHAR *filename,int num_defines,...)
{
	HRESULT hr=S_OK;
	std::cout<<_T("Create effect: ")<<filename<<std::endl;
	tstring fn=filename;
	fn=filepath+fn;
	D3D10_SHADER_MACRO *macros=NULL;
	std::vector<std::string> d3dmacros;
	{
		bool name=true;
	const char *txt=NULL;
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


	if(FAILED(
		hr=D3DX11CreateEffectFromFile(
				fn.c_str(),
				flags,
				d3dDevice,
				effect)))
	{
		std::string err="";
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateEffect", hr );
#endif
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
