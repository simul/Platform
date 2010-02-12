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
static int ShaderModel=2;
void SetShaderPath(const char *path)
{
#ifdef UNICODE
	// tstring and TEXT cater for the confusion between wide and regular strings.
	filepath.resize(strlen(path),L' '); // Make room for characters
	// Copy string to wstring.
	std::copy(path,path+strlen(path),filepath.begin());

	filepath+=L"/";
#else
	filepath=path;
	filepath+="/";
#endif
}

int GetShaderModel()
{
	return ShaderModel;
}

void SetShaderModel(int m)
{
	ShaderModel=m;
}

// Get the named technique. If not supported, 
D3DXHANDLE GetDX9Technique(LPD3DXEFFECT effect,const char *tech_name)
{
	D3DXHANDLE tech=NULL;
	std::string str=tech_name;
	tech=effect->GetTechniqueByName(str.c_str());
	if(effect->ValidateTechnique(tech)!=S_OK)
	{
		tech=effect->GetTechniqueByName((str+"_sm2").c_str());
	}
	return tech;
}

HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const char *filename,int num_defines,...)
{
#ifdef BUNDLE_SHADERS
	return CreateDX9Effect(m_pd3dDevice,effect,GetResourceId(filename),num_defines,...)
#else
	std::cout<<"CreateDX9Effect "<<filename<<std::endl;
	HRESULT hr;
    LPD3DXBUFFER errors=0;
#ifdef UNICODE
	// tstring and TEXT cater for the confusion between wide and regular strings.
	std::wstring wfilename(strlen(filename),L' '); // Make room for characters
	// Copy string to wstring.
	std::copy(filename,filename+strlen(filename),wfilename.begin());
	tstring fn=filepath+wfilename;
#else
	tstring fn=filename;
	fn=filepath+fn;
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
#endif
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
				macros[def].Definition="";
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



HRESULT RenderTexture(IDirect3DDevice9 *m_pd3dDevice,int x1,int y1,int dx,int dy,LPDIRECT3DTEXTURE9 texture)
{
	static bool disable=false;

	if(disable)
		return S_OK;
	HRESULT hr=S_OK;
	LPDIRECT3DVERTEXDECLARATION9	m_pBufferVertexDecl=NULL;
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 8, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0 },
		{ 0, 32, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pBufferVertexDecl);
	V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl));
#ifdef XBOX
	float x=-1.f,y=1.f;
	float w=2.f;
	float h=-2.f;
	struct Vertext
	{
		float x,y;
		float r,g,b,a;
		float tx,ty;
	};
	Vertext vertices[4] =
	{
		{x,			y,			0	,0},
		{x+w,		y,			1.f	,0},
		{x+w,		y+h,		1.f	,1.f},
		{x,			y+h,		0	,1.f},
	};
#else
	float x=x1,y=y1;
	struct Vertext
	{
		float x,y,z,h;
		float r,g,b,a;
		float tx,ty;
	};
	float width=dx,height=dy;
	Vertext vertices[4] =
	{
		{x,			y,			1,	1, 1.f,1.f,1.f,1.f	,0.0f	,0.0f},
		{x+width,	y,			1,	1, 1.f,1.f,1.f,1.f	,1.0f	,0.0f},
		{x+width,	y+height,	1,	1, 1.f,1.f,1.f,1.f	,1.0f	,1.0f},
		{x,			y+height,	1,	1, 1.f,1.f,1.f,1.f	,0.0f	,1.0f},
	};
#endif
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

    m_pd3dDevice->SetVertexShader(NULL);
    m_pd3dDevice->SetPixelShader(NULL);
	m_pd3dDevice->SetVertexDeclaration(m_pBufferVertexDecl);

#ifndef XBOX
	m_pd3dDevice->SetTransform(D3DTS_VIEW,&ident);
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&ident);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&ident);
#endif
	m_pd3dDevice->SetTexture(0,texture);
    m_pd3dDevice->SetTextureStageState(0,D3DTSS_COLOROP, D3DTOP_MODULATE);
    m_pd3dDevice->SetTextureStageState(0,D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));


	SAFE_RELEASE(m_pBufferVertexDecl);
	return S_OK;
}
