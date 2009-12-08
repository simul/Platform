// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateDX9Effect.cpp Create a DirectX .fx effect and report errors.

#include "CreateDX9Effect.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
#else
	#include <tchar.h>
	#include <dxerr9.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#endif
typedef std::basic_string<TCHAR> tstring;
	#include <vector>
#include <iostream>
#include "Macros.h"
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)			{ hr = x; if( FAILED(hr) ) { return hr; } }
#endif
struct d3dMacro
{
	std::string name;
	std::string define;
};
void SetFilepath(const TCHAR *fp)
{
	filepath=fp;
}
HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const TCHAR *filename,int num_defines,...)
{
	std::cout<<"CreateDX9Effect "<<filename<<std::endl;
	HRESULT hr;
    LPD3DXBUFFER errors=0;
#ifdef UNICODE
	tstring fn=filename;
	fn=filepath+fn;
#else
	// tstring and TEXT cater for the confusion between wide and regular strings.
	std::wstring wfilename(_tclen(filename),L' '); // Make room for characters
	// Copy string to wstring.
	std::copy(filename,filename+_tclen(filename),wfilename.begin());
	tstring fn=filepath+wfilename;
#endif
	D3DXMACRO *macros=NULL;
	std::vector<std::string> d3dmacros;
	{
		bool name=true;
		const char *txt=NULL;
		if(num_defines)
		{
			macros=new D3DXMACRO[num_defines+1];
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
	SAFE_RELEASE(effect);
	
	hr=D3DXCreateEffectFromFile(
			m_pd3dDevice,
			fn.c_str(),
			macros,
			NULL,
			flags,
			NULL,
			&effect,
			&errors);
	std::string err="";
	if(errors)
	{
		if(FAILED(hr))
			std::cerr<<"Errors building "<<fn.c_str()<<std::endl;
		else
			std::cerr<<"Warnings building "<<fn.c_str()<<std::endl;
		err=static_cast<const char*>(errors->GetBufferPointer());
		std::cerr<<err<<std::endl;
	}
	if(FAILED(hr))
	{
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateDX9Effect", hr );
#endif
		DebugBreak();
	}
	delete [] macros;
	V_RETURN(hr);
	return hr;
}

HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,DWORD resource,int num_defines,...)
{
	std::cout<<"CreateDX9Effect "<<resource<<std::endl;
	HRESULT hr;
    LPD3DXBUFFER errors=0;
	D3DXMACRO *macros=NULL;
	std::vector<std::string> d3dmacros;
	{
		bool name=true;
		const char *txt=NULL;
		if(num_defines)
		{
			macros=new D3DXMACRO[num_defines+1];
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
	HMODULE hModule=GetModuleHandle(NULL);
	LPTSTR rn=MAKEINTRESOURCE(resource);
	DWORD flags=default_effect_flags;
	SAFE_RELEASE(effect);
	std::string err="";
	hr=D3DXCreateEffectFromResource(
				m_pd3dDevice,
				hModule,
				rn,
				macros,
				NULL,
				flags,
				NULL,
				&effect,
				&errors);
	if(FAILED(hr))
		std::cout<<"Error building "<<rn<<std::endl;
	if(errors)
	{
		if(!FAILED(hr))
			std::cout<<"Warnings building "<<rn<<std::endl;
		err=static_cast<const char*>(errors->GetBufferPointer());
		std::cout<<err<<std::endl;
	}
	if(FAILED(hr))
	{
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateDX9Effect", hr );
#endif
		DebugBreak();
	}
	delete [] macros;
	V_RETURN(hr);
	return hr;
}
HRESULT CanUseTexFormat(IDirect3DDevice9 *device,D3DFORMAT f)
{
	IDirect3D9 *pd3d=0;
	HRESULT hr=device->GetDirect3D(&pd3d);
	D3DDISPLAYMODE DisplayMode;
	ZeroMemory(&DisplayMode, sizeof(D3DDISPLAYMODE));
	device->GetDisplayMode(0, &DisplayMode);
	hr=pd3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,DisplayMode.Format, 
			D3DUSAGE_DYNAMIC,D3DRTYPE_TEXTURE,f);
	if(SUCCEEDED(hr))
		std::cout<<"OK to use texture format"<<f<<std::endl;
	if(FAILED(hr))
		std::cout<<"Cannot use texture format"<<f<<std::endl;
	return hr;
}
HRESULT CanUseDepthFormat(IDirect3DDevice9 *device,D3DFORMAT f)
{
	IDirect3D9 *pd3d=0;
	HRESULT hr=device->GetDirect3D(&pd3d);
	D3DDISPLAYMODE DisplayMode;
	ZeroMemory(&DisplayMode, sizeof(D3DDISPLAYMODE));
	device->GetDisplayMode(0, &DisplayMode);
	hr=pd3d->CheckDeviceFormat(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,DisplayMode.Format, 
			D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_SURFACE,f);
	return hr;
}
HRESULT CanUse16BitFloats(IDirect3DDevice9 *device)
{
	HRESULT hr=CanUseTexFormat(device,D3DFMT_A16B16G16R16F);
	if(SUCCEEDED(hr))
		std::cout<<"OK to create 16-bit float textures"<<std::endl;
	if(FAILED(hr))
		std::cout<<"Cannot create 16-bit float textures"<<std::endl;
	return hr;
}