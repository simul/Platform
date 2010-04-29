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
	static tstring shader_path=TEXT("game:\\");
	static tstring texture_path=TEXT("game:\\");
	static DWORD default_effect_flags=0;
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring shader_path=TEXT("");
	static tstring texture_path=TEXT("");
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#endif
	#include <vector>
#include <iostream>
#include "Macros.h"
#include "Resources.h"
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)			{ hr = x; if( FAILED(hr) ) { return hr; } }
#endif

bool BUNDLE_SHADERS=false;

static tstring module;

void SetBundleShaders(bool b)
{
	BUNDLE_SHADERS=b;
}

struct d3dMacro
{
	std::string name;
	std::string define;
};
static ShaderModel shaderModel=NO_SHADERMODEL;
static ShaderModel maxShaderModel=USE_SHADER_3;
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
void SetResourceModule(const char *txt)
{
#ifdef UNICODE
	// tstring and TEXT cater for the confusion between wide and regular strings.
	module.resize(strlen(txt),L' '); // Make room for characters
	// Copy string to wstring.
	std::copy(txt,txt+strlen(txt),module.begin());
#else
	module=txt;
#endif
}



ShaderModel GetShaderModel()
{
	return shaderModel;
}
LPDIRECT3DDEVICE9 last_d3dDevice=0;

static const char *GetPixelShaderString(const D3DCAPS9 &caps)
{
    switch(caps.PixelShaderVersion)
    {
		case D3DPS_VERSION(1, 1):
			return "ps_1_1";

		case D3DPS_VERSION(1, 2):
			return "ps_1_2";

		case D3DPS_VERSION(1, 3):
			return "ps_1_3";

		case D3DPS_VERSION(1, 4):
			return "ps_1_4";

		case D3DPS_VERSION(2, 0):
			if ((caps.PS20Caps.NumTemps>=22)                          &&
				(caps.PS20Caps.Caps&D3DPS20CAPS_ARBITRARYSWIZZLE)     &&
				(caps.PS20Caps.Caps&D3DPS20CAPS_GRADIENTINSTRUCTIONS) &&
				(caps.PS20Caps.Caps&D3DPS20CAPS_PREDICATION)          &&
				(caps.PS20Caps.Caps&D3DPS20CAPS_NODEPENDENTREADLIMIT) &&
				(caps.PS20Caps.Caps&D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT))
			{
				return "ps_2_a";
			}
			if ((caps.PS20Caps.NumTemps>=32)                          &&
				(caps.PS20Caps.Caps&D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT))
			{
				return "ps_2_b";
			}
			return "ps_2_0";

		case D3DPS_VERSION(3, 0):
        return "ps_3_0";
    }
    return NULL;
}
static void CalcShaderModel(LPDIRECT3DDEVICE9 m_pd3dDevice)
{
	if(last_d3dDevice==m_pd3dDevice)
		return;
	if(shaderModel!=NO_SHADERMODEL&&shaderModel<=maxShaderModel)
		return;
	last_d3dDevice=m_pd3dDevice;
	
	D3DCAPS9 pCaps;//
	//, D3DFORMAT AdapterFormat, 
              //                    D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )

	m_pd3dDevice->GetDeviceCaps(&pCaps);
	const char *shader_str=GetPixelShaderString(pCaps);
	std::cout<<"Pixel shader version ";
	std::cout<<shader_str;
	std::cout<<std::endl;
    // check vertex shading support
    if (pCaps.VertexShaderVersion < D3DVS_VERSION(2,0))
	{
		std::cerr<<"Device does not support 2.0 vertex shaders!";
		DebugBreak();
		shaderModel=NO_SHADERMODEL;
		return;
	}
    // check pixel shader support 
    if (pCaps.PixelShaderVersion < D3DPS_VERSION(2,0))
	{
		std::cerr<<"Device does not support 2.0 pixel shaders!";
		DebugBreak();
		shaderModel=NO_SHADERMODEL;
		return;
	}
	shaderModel=USE_SHADER_3;
    if(pCaps.PixelShaderVersion < D3DPS_VERSION(3,0))
	{
		if(pCaps.PixelShaderVersion == D3DPS_VERSION(2,0))
		{
			std::cout<<"Device does not support 2.a pixel shaders, defaulting to 2.0"<<std::endl;
			shaderModel=USE_SHADER_2;
		}
		else
		{
			std::cout<<"Device does not support 3.0 pixel shaders, defaulting to 2.a"<<std::endl;
			shaderModel=USE_SHADER_2A;
		}
	}
	if(shaderModel>maxShaderModel)
		shaderModel=maxShaderModel;
}

void SetMaxShaderModel(ShaderModel m)
{
	maxShaderModel=m;
	if((int)shaderModel>m)
		shaderModel=m;
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
static D3DXMACRO *MakeMacroList(const std::map<std::string,std::string>&defines)
{
	D3DXMACRO *macros=NULL;
	if(defines.size())
	{
		macros=new D3DXMACRO[defines.size()+1];
		macros[defines.size()].Definition=0;
		macros[defines.size()].Name=0;
	}
	size_t def=0;
	for(std::map<std::string,std::string>::const_iterator i=defines.begin();i!=defines.end();i++)
	{
		macros[def].Name=i->first.c_str();
		macros[def].Definition=i->second.c_str();
		def++;
	}
	return macros;
}

HRESULT CreateDX9Texture(LPDIRECT3DDEVICE9 m_pd3dDevice,LPDIRECT3DTEXTURE9 &texture,DWORD resource)
{
	HMODULE hModule=GetModuleHandle(module.c_str());
	LPTSTR rn=MAKEINTRESOURCE(resource);
	return D3DXCreateTextureFromResource(m_pd3dDevice,hModule,rn,&texture);
}

HRESULT CreateDX9Texture(LPDIRECT3DDEVICE9 m_pd3dDevice,LPDIRECT3DTEXTURE9 &texture,const char *filename)
{
	if(BUNDLE_SHADERS)
		return CreateDX9Texture(m_pd3dDevice,texture,GetResourceId(filename));
	if(!texture_path_set)
	{
		std::cerr<<"CreateDX9Texture: Texture path not set, use SetTexturePath() with the relative path to the .fx files."<<std::endl;
	}
#ifdef UNICODE
	// tstring and TEXT cater for the confusion between wide and regular strings.
	std::wstring wfilename(strlen(filename),L' '); // Make room for characters
	// Copy string to wstring.
	std::copy(filename,filename+strlen(filename),wfilename.begin());
	tstring fn=texture_path+wfilename;
#else
	tstring fn=filename;
	fn=texture_path+fn;
#endif
	return D3DXCreateTextureFromFile(m_pd3dDevice,fn.c_str(),&texture);
}

HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const char *filename)
{
	std::map<std::string,std::string> defines;
	return CreateDX9Effect(m_pd3dDevice,effect,filename,defines);
}

HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const char *filename,const std::map<std::string,std::string>&defines)
{
	CalcShaderModel(m_pd3dDevice);
	if(BUNDLE_SHADERS)
		return CreateDX9Effect(m_pd3dDevice,effect,GetResourceId(filename));

	std::cout<<"CreateDX9Effect "<<filename<<std::endl;
	HRESULT hr;
    LPD3DXBUFFER errors=0;

	if(!shader_path_set)
	{
		std::cerr<<"CreateDX9Effect.cpp: Shader path not set, use SetShaderPath() with the relative path to the .fx files."<<std::endl;
	}
#ifdef UNICODE
	// tstring and TEXT cater for the confusion between wide and regular strings.
	std::wstring wfilename(strlen(filename),L' '); // Make room for characters
	// Copy string to wstring.
	std::copy(filename,filename+strlen(filename),wfilename.begin());
	tstring fn=shader_path+wfilename;
#else
	tstring fn=filename;
	fn=shader_path+fn;
#endif
	D3DXMACRO *macros=MakeMacroList(defines);

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
        hr=DXTRACE_ERR( "CreateDX9Effect", hr );
#endif
		DebugBreak();
	}
	delete [] macros;
	V_RETURN(hr);
	return hr;
}


HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,DWORD resource)
{
	std::map<std::string,std::string> defines;
	return CreateDX9Effect(m_pd3dDevice,effect,resource,defines);
}
HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,DWORD resource,const std::map<std::string,std::string> &defines)
{
	std::cout<<"CreateDX9Effect "<<resource<<std::endl;
	HRESULT hr;
    LPD3DXBUFFER errors=0;
	D3DXMACRO *macros=MakeMacroList(defines);
	HMODULE hModule=GetModuleHandle(module.c_str());
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
		std::cout<<"Error building shader"<<std::endl;
	if(errors)
	{
		if(!FAILED(hr))
			std::cout<<"Warnings building shader"<<std::endl;
		err=static_cast<const char*>(errors->GetBufferPointer());
		std::cout<<err<<std::endl;
	}
	if(FAILED(hr))
	{
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( "CreateDX9Effect", hr );
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



HRESULT RenderTexture(IDirect3DDevice9 *m_pd3dDevice,int x1,int y1,int dx,int dy,LPDIRECT3DBASETEXTURE9 texture)
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
	float x=(float)x1,y=(float)y1;
	struct Vertext
	{
		float x,y,z,h;
		float r,g,b,a;
		float tx,ty;
	};
	float width=(float)dx,height=(float)dy;
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
    m_pd3dDevice->SetSamplerState(0,D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pd3dDevice->SetSamplerState(0,D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
	SAFE_RELEASE(m_pBufferVertexDecl);
	return S_OK;
}
