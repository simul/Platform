// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateDX9Effect.cpp Create a DirectX .fx effect and report errors.

#include "CreateDX9Effect.h"

#include <tchar.h>
#include <dxerr.h>
#include <string>
typedef std::basic_string<TCHAR> tstring;
static tstring shader_path=TEXT("");
static tstring texture_path=TEXT("");
static DWORD default_effect_flags=D3DXSHADER_SKIPVALIDATION;//D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#include <vector>
#include <iostream>
#include <Windows.h>
#include "Macros.h"
#include "Resources.h"
#include "Simul/Geometry/Orientation.h"
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)			{ hr = x; if( FAILED(hr) ) { return hr; } }
#endif

unsigned (*GetResourceId)(const char *filename)=NULL;
LPDIRECT3DDEVICE9				last_d3dDevice			=NULL;
LPDIRECT3DVERTEXDECLARATION9	m_pHudVertexDecl		=NULL;
LPD3DXEFFECT					m_pDebugEffect			=NULL;
D3DXHANDLE						m_hTechniquePlainColour	=NULL;
bool BUNDLE_SHADERS=false;
static tstring module;
int RT::instance_count=0;
int RT::screen_width=0;
int RT::screen_height=0;

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
ID3DXFont *m_pFont=NULL;
namespace simul
{
	namespace dx9
	{
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
	}
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
	
	D3DCAPS9 pCaps;

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
		return CreateDX9Texture(m_pd3dDevice,texture,(*GetResourceId)(filename));
	if(!texture_path_set)
	{
		std::cerr<<"CreateDX9Texture: Texture path not set, use SetTexturePath() with the relative path to the image files."<<std::endl;
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
	HRESULT hr=D3DXCreateTextureFromFileEx(	m_pd3dDevice,
											fn.c_str(),
											D3DX_DEFAULT_NONPOW2,
											D3DX_DEFAULT_NONPOW2,
											D3DX_DEFAULT,
											0,
											D3DFMT_UNKNOWN,
											D3DPOOL_DEFAULT,
											D3DX_DEFAULT,
											D3DX_DEFAULT,
											0,
											NULL,
											NULL,
											&texture);
	// if failed, try to get the resource:
	if(hr!=S_OK)
	{
		hr=CreateDX9Texture(m_pd3dDevice,texture,GetResourceId(filename));
	}
	return hr;
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
		std::cerr<<"CreateDX9Effect.cpp: Shader path not set, use SetShaderPath() with the relative path to the .fx or .hlsl files."<<std::endl;
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

	SAFE_RELEASE(effect);
	
	hr=D3DXCreateEffectFromFile(
			m_pd3dDevice,
			fn.c_str(),
			macros,
			NULL,
			default_effect_flags,
			NULL,
			&effect,
			&errors);
	std::string err="";
	if(errors)
	{
		if(FAILED(hr))
			std::cerr<<"Errors building "<<filename<<std::endl;
		else
			std::cerr<<"Warnings building "<<filename<<std::endl;
		err=static_cast<const char*>(errors->GetBufferPointer());
		std::cerr<<err<<std::endl;
	}
	if(FAILED(hr))
	{
		const TCHAR *err=DXGetErrorString(hr);
		std::cerr<<err<<std::endl;
		if(GetResourceId)
			hr=CreateDX9Effect(m_pd3dDevice,effect,GetResourceId(filename),defines);
		if(FAILED(hr))
		{
#ifdef DXTRACE_ERR
			hr=DXTRACE_ERR(_T("CreateDX9Effect"), hr );
#endif
			DebugBreak();
		}
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
	if(hr==D3DXERR_INVALIDDATA)
		std::cout<<"Invalid data building shader"<<std::endl;
	else if(hr==D3DERR_INVALIDCALL)
		std::cout<<"Invalid call building shader"<<std::endl;
	else if(FAILED(hr))
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
		hr=DXTRACE_ERR(_T("CreateDX9Effect"), hr );
#endif
		DebugBreak();
	}
	delete [] macros;
	V_RETURN(hr);
	return hr;
}
/*

{
LPVOID lpMsgBuf;
FormatMessage( 
	FORMAT_MESSAGE_ALLOCATE_BUFFER | 
	FORMAT_MESSAGE_FROM_SYSTEM | 
	FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL,
	hr,
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
	(LPTSTR) &lpMsgBuf,
	0,
	NULL 
	);
// Process any inserts in lpMsgBuf.
// ...
// Display the string.
MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
// Free the buffer.
atr=jj;
LocalFree( lpMsgBuf );
}
*/
HRESULT CanUseTexFormat(IDirect3DDevice9 *device,D3DFORMAT f)
{
	IDirect3D9 *pd3d=0;
	HRESULT hr=device->GetDirect3D(&pd3d);
	D3DDISPLAYMODE DisplayMode;
	ZeroMemory(&DisplayMode,sizeof(D3DDISPLAYMODE));
	device->GetDisplayMode(0,&DisplayMode);
	hr=pd3d->CheckDeviceFormat(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,DisplayMode.Format,0,D3DRTYPE_TEXTURE,f);
	if(hr==D3DERR_INVALIDCALL)
	{
		std::cout<<"D3DERR_INVALIDCALL "<<std::endl;
	}
	else if(hr==D3DERR_NOTAVAILABLE)
	{
		std::cout<<"D3DERR_NOTAVAILABLE "<<std::endl;
	}
	if(SUCCEEDED(hr))
		std::cout<<"OK to use texture format "<<f<<std::endl;
	if(FAILED(hr))
		std::cout<<"Cannot use texture format "<<f<<std::endl;
	hr=S_OK;
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
LPDIRECT3DVERTEXDECLARATION9 RT::m_pBufferVertexDecl=NULL;
RT::RT()
{
	instance_count++;
}

void RT::RestoreDeviceObjects(IDirect3DDevice9 *m_pd3dDevice)
{
	last_d3dDevice=m_pd3dDevice;

	if(m_pBufferVertexDecl==NULL)
	{
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
		V_CHECK(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl));
	}
	D3DVERTEXELEMENT9 decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 12, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0 },
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pHudVertexDecl);
	V_CHECK(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pHudVertexDecl));

	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pDebugEffect,"simul_debug.fx"));
	m_hTechniquePlainColour=m_pDebugEffect->GetTechniqueByName("simul_plain_colour");
}

void RT::SetScreenSize(int w,int h)
{
	screen_width=w;
	screen_height=h;
}

void RT::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pDebugEffect);
	SAFE_RELEASE(m_pBufferVertexDecl);
	SAFE_RELEASE(m_pHudVertexDecl);
	SAFE_RELEASE(m_pFont);
	last_d3dDevice=NULL;
}

RT::~RT()
{
	if(!instance_count)
		RT::InvalidateDeviceObjects();
}

HRESULT RenderTexture(IDirect3DDevice9 *m_pd3dDevice,int x1,int y1,int dx,int dy,
					  LPDIRECT3DBASETEXTURE9 texture,LPD3DXEFFECT eff,D3DXHANDLE tech)
{
	HRESULT hr=S_OK;
	D3DXMATRIX v,w,p;
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&v);
	m_pd3dDevice->GetTransform(D3DTS_WORLD,&w);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&p);
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
		{x,			y,			0.f,	1.f, 1.f,1.f,1.f,1.f	,0.0f	,0.0f},
		{x+width,	y,			0.f,	1.f, 1.f,1.f,1.f,1.f	,1.0f	,0.0f},
		{x+width,	y+height,	0.f,	1.f, 1.f,1.f,1.f,1.f	,1.0f	,1.0f},
		{x,			y+height,	0.f,	1.f, 1.f,1.f,1.f,1.f	,0.0f	,1.0f},
	};
#endif
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	m_pd3dDevice->SetVertexDeclaration(RT::m_pBufferVertexDecl);
#ifndef XBOX
	m_pd3dDevice->SetTransform(D3DTS_VIEW,&ident);
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&ident);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&ident);
#endif
	unsigned passes=0;
	if(eff)
	{
		if(tech)
			eff->SetTechnique(tech);
		eff->Begin(&passes,0);
		eff->BeginPass(0);
	}
	else
	{
		m_pd3dDevice->SetTexture(0,texture);
		m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
		m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		m_pd3dDevice->SetTextureStageState(0,D3DTSS_COLOROP, D3DTOP_MODULATE);
		m_pd3dDevice->SetTextureStageState(0,D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		m_pd3dDevice->SetSamplerState(0,D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		m_pd3dDevice->SetSamplerState(0,D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	}
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
	if(eff)
	{
		eff->EndPass();
		eff->End();
	}
	m_pd3dDevice->SetTransform(D3DTS_VIEW,&v);
	m_pd3dDevice->SetTransform(D3DTS_WORLD,&w);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&p);
	return S_OK;
}


void FixProjectionMatrix(D3DXMATRIX &proj,float zFar,bool y_vertical)
{
	float zNear;
	if(y_vertical)
	{
		zNear=-proj._43/proj._33;
		proj._33=zFar/(zFar-zNear);
	}
	else
	{
		zNear=proj._43/proj._33;
		proj._33=-zFar/(zFar-zNear);
	}
	proj._43=-zNear*zFar/(zFar-zNear);
}

void FixProjectionMatrix(D3DXMATRIX &proj,float zNear,float zFar,bool y_vertical)
{
	if(y_vertical)
	{
		proj._33=zFar/(zFar-zNear);
	}
	else
	{
		proj._33=-zFar/(zFar-zNear);
	}
	proj._43=-zNear*zFar/(zFar-zNear);
}

HRESULT RenderLines(LPDIRECT3DDEVICE9 m_pd3dDevice,int num,const float *pos)
{
	HRESULT hr=S_OK;
	m_pd3dDevice;num;pos;
	D3DVERTEXELEMENT9 decl[] = 
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 12, D3DDECLTYPE_D3DCOLOR	,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
		D3DDECL_END()
	};
	if(!m_pHudVertexDecl)
	{
		V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pHudVertexDecl));
	}
/*
	struct Vertext
	{
		float x,y,z;
		unsigned char r,g,b,a;
		float tx,ty;
	};
    m_pd3dDevice->SetVertexShader(NULL);
    m_pd3dDevice->SetPixelShader(NULL);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

	m_pd3dDevice->SetVertexDeclaration(m_pHudVertexDecl);

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );

	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_CONSTANT);

// Set the constant to 0.25 alpha (0x40 = 64 = 64/256 = 0.25)
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_CONSTANT, 0x80000080);

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
	m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

	m_pd3dDevice->SetTexture(0,NULL);
	
	Vertext *lines=new Vertext[num*2];

	for(int i=0;i<num;i++)
	{
		lines[i*2].x=pos[i*3];
		lines[i*2].y=pos[i*3+1];
		lines[i*2].z=pos[i*3+2];
		lines[i*2].r=255;
		lines[i*2].g=255;
		lines[i*2].b=0;
		lines[i*2].a=255;
		lines[i*2+1].x=pos[i*3+3]; 
		lines[i*2+1].y=pos[i*3+4];  
		lines[i*2+1].z=pos[i*3+5];
		lines[i*2+1].r=255;
		lines[i*2+1].g=255;
		lines[i*2+1].b=0;
		lines[i*2+1].a=255;
	}
	hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST,num,lines,(unsigned)sizeof(Vertext));
	delete [] lines;*/
	return hr;
}

void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj)
{
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(wvp,&tmp2);
}

HRESULT RenderAngledQuad(LPDIRECT3DDEVICE9 m_pd3dDevice,D3DXVECTOR3 cam_pos,D3DXVECTOR3 dir,bool y_vertical,float half_angle_radians,LPD3DXEFFECT effect)
{
	// If y is vertical, we have LEFT-HANDED rotations, otherwise right.
	// But D3DXMatrixRotationYawPitchRoll uses only left-handed, hence the change of sign below.
	float Yaw=atan2(dir.x,y_vertical?dir.z:dir.y);
	float Pitch=-asin(y_vertical?dir.y:dir.z);
	HRESULT hr=S_OK;
	D3DXMATRIX world, view,proj,tmp1, tmp2;
	D3DXMatrixIdentity(&world);
	static D3DXMATRIX flip(1.f,0,0,0,0,0,1.f,0,0,1.f,0,0,0,0,0,1.f);
	if(y_vertical)
	{
		D3DXMatrixRotationYawPitchRoll(
			  &world,
			  Yaw,
			  Pitch,
			  0
			);
	}
	else
	{
		simul::geometry::SimulOrientation or;
		or.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
		or.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
		world=*((const D3DXMATRIX*)(or.T4.RowPointer(0)));
	}
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif

	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	if(effect)
	{
		D3DXHANDLE worldViewProj=effect->GetParameterByName(NULL,"worldViewProj");
		effect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));
	}
	struct Vertext
	{
		float x,y,z;
		float tx,ty;
	};
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	static float w=1.f;
	float d=w/tan(half_angle_radians);
	Vertext vertices[4] =
	{
		{ w,-w,	d, 1.f	,0.f},
		{ w, w,	d, 1.f	,1.f},
		{-w, w,	d, 0.f	,1.f},
		{-w,-w,	d, 0.f	,0.f},
	};
	m_pd3dDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX0);
	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    hr=m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
     
	UINT passes=1;
	hr=effect->Begin(&passes,0);
	for(unsigned i=0;i<passes;++i)
	{
		hr=effect->BeginPass(i);
		m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
		hr=effect->EndPass();
	}
	hr=effect->End();
	return hr;
}


HRESULT DrawFullScreenQuad(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT effect)
{
	HRESULT hr=S_OK;
	D3DXMATRIX vpt;
	D3DXMATRIX viewproj,view,proj;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	view._41=view._42=view._43=0;
	D3DXMatrixMultiply(&viewproj,&view,&proj);
	D3DXMatrixTranspose(&vpt,&viewproj);
	D3DXMATRIX ivp;
	D3DXMatrixInverse(&ivp,NULL,&vpt);
	//hr=effect->SetMatrix(invViewProj,&ivp);
	#ifdef XBOX
	float x=-1.f,y=1.f;
	float w=2.f;
	float h=-2.f;
	struct Vertext
	{
		float x,y;
		float tx,ty,tz;
	};
	Vertext vertices[4] =
	{
		{x,			y,			0	,0},
		{x+w,		y,			0	,0},
		{x+w,		y+h,		1.f,0},
		{x,			y+h,		1.f,0},
	};
#else
//	D3DSURFACE_DESC desc;
	//if(input_texture)
	//	input_texture->GetLevelDesc(0,&desc);

	struct Vertext
	{
		float x,y;
		float tx,ty,tz;
	};
	Vertext vertices[4] =
	{
		{-1.f,	-1.f	,0.5f	,0		,1.f},
		{ 1.f,	-1.f	,0.5f	,1.f	,1.f},
		{ 1.f,	 1.f	,0.5f	,1.f	,0	},
		{-1.f,	 1.f	,0.5f	,0		,0	},
	};
#endif
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	m_pd3dDevice->SetFVF(D3DFVF_XYZ  |  D3DFVF_TEX1);
	//m_pd3dDevice->SetVertexDeclaration(vertexDecl);
	UINT passes=1;
	if(effect)
	{
		hr=effect->Begin(&passes,0);
		hr=effect->BeginPass(0);
	}
	m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
	if(effect)
	{
		hr=effect->EndPass();
		hr=effect->End();
	}
	return hr;
}

bool IsDepthFormatOk(LPDIRECT3DDEVICE9 pd3dDevice,D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat)
{
	LPDIRECT3D9 d3d;
	pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);

    return (hr==S_OK);
}


LPDIRECT3DSURFACE9 MakeRenderTarget(const LPDIRECT3DTEXTURE9 pTexture)
{
	LPDIRECT3DSURFACE9 pRenderTarget;
#ifdef XBOX
	XGTEXTURE_DESC desc;
	XGGetTextureDesc( pTexture, 0, &desc );
	D3DSURFACE_PARAMETERS SurfaceParams = {0};
	//HANDLE handle=&SurfaceParams;
	HRESULT hr=m_pd3dDevice->CreateRenderTarget(
		desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &pRenderTarget,&SurfaceParams);
	if(hr!=S_OK)
		std::cerr<<"SimulWeatherRenderer::MakeRenderTarget - Failed to create render target!\n";
#else
	pTexture->GetSurfaceLevel(0,&pRenderTarget);
#endif
	return pRenderTarget;
}

const TCHAR *GetErrorText(HRESULT hr)
{
	const TCHAR *err=DXGetErrorString(hr);
	if(!err)
	{
		static TCHAR txt[10];
		_stprintf(txt,_T("%3.3g"),hr);
		return txt;
	}
	return err;
}

void GetCameraPosVector(D3DXMATRIX &view,bool y_vertical,float *dcam_pos,float *view_dir)
{
	D3DXMATRIX tmp1;
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


std::map<std::string,std::string> MakeDefinesList(bool wrap,bool y_vertical)
{
	std::map<std::string,std::string> defines;
	defines["DETAIL_NOISE"]="1";
	if(wrap)
		defines["WRAP_CLOUDS"]="1";
	if(!y_vertical)
		defines["Z_VERTICAL"]='1';
	else
		defines["Y_VERTICAL"]='1';
	return defines;
}


void RT::PrintAt3dPos(const float *p,const char *text,const float *colr,int offsetx,int offsety)
{
	if(!m_pFont)
	{
		V_CHECK(D3DXCreateFont(last_d3dDevice,32,0,FW_NORMAL,1,FALSE,DEFAULT_CHARSET,
								OUT_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,
								_T("Arial"),&m_pFont));
	}
	D3DXHANDLE worldViewProj=m_pDebugEffect->GetParameterByName(NULL,"worldViewProj");
	HRESULT hr=S_OK;
	D3DXMATRIX tmp1,tmp2,wvp,world,view,proj;
#ifndef xbox
	last_d3dDevice->GetTransform(D3DTS_WORLD,&world);
	last_d3dDevice->GetTransform(D3DTS_VIEW,&view);
	last_d3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&wvp,&tmp1,&proj);
	m_pDebugEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&wvp));

	D3DXVECTOR4 pos(p[0],p[1],p[2],1.f);
	D3DXVECTOR4 screen_pos;
	D3DXVec4Transform(&screen_pos,&pos,&wvp);
	float x=0.5f*(screen_pos.x/screen_pos.w+1.f)*RT::screen_width+offsetx;
	float y=0.5f*(1.f-screen_pos.y/screen_pos.w)*RT::screen_height+offsety;
	RECT rcDest;
	rcDest.left=(long)x-32;
	rcDest.bottom=(long)y+32;
	rcDest.right=(long)x+32;
	rcDest.top=(long)y-32;

	DWORD dwTextFormat = DT_CENTER |  DT_NOCLIP ;//DT_CALCRECT;
	if(screen_pos.w<0)
		return;
	
//	wchar_t pwText[50];
	//MultiByteToWideChar (CP_ACP, 0,text, -1, pwText, 48 );
	hr = m_pFont->DrawTextA(NULL,text,-1,&rcDest,dwTextFormat,(D3DXCOLOR)colr);
}

void RT::DrawLines(VertexXyzRgba *lines,int vertex_count,bool strip)
{
	D3DXHANDLE worldViewProj=m_pDebugEffect->GetParameterByName(NULL,"worldViewProj");
	D3DXMATRIX tmp1,tmp2,wvp,view,proj;
#ifndef xbox
	last_d3dDevice->GetTransform(D3DTS_VIEW,&view);
	last_d3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMatrixMultiply(&tmp2,&view,&proj);
	D3DXMatrixTranspose(&wvp,&tmp2);
	m_pDebugEffect->SetTechnique(m_hTechniquePlainColour);
	last_d3dDevice->SetVertexDeclaration(m_pHudVertexDecl);
	last_d3dDevice->SetTexture(0,NULL);
	m_pDebugEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&wvp));
	unsigned passes=1;
	V_CHECK(m_pDebugEffect->Begin(&passes,0));
	V_CHECK(m_pDebugEffect->BeginPass(0));
	V_CHECK(last_d3dDevice->DrawPrimitiveUP(strip?D3DPT_LINESTRIP:D3DPT_LINELIST,strip?(vertex_count-1):(vertex_count/2),lines,(unsigned)sizeof(VertexXyzRgba)));
	V_CHECK(m_pDebugEffect->EndPass());
	V_CHECK(m_pDebugEffect->End());
}