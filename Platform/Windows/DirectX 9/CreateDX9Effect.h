// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateDX9Effect.h Create a DirectX .fx effect and report errors.

#ifdef XBOX
	#include <xgraphics.h>
#else
	#include <d3dx9.h>
#endif
#include <map>
#include <string>
enum ShaderModel {NO_SHADERMODEL=0,USE_SHADER_2,USE_SHADER_2A,USE_SHADER_3};
extern ShaderModel GetShaderModel();
extern void SetMaxShaderModel(ShaderModel m);
extern void SetShaderPath(const char *path);
extern void SetTexturePath(const char *path);
// Get the technique, or look for a less hardware-demanding equivalent:
extern D3DXHANDLE GetDX9Technique(LPD3DXEFFECT effect,const char *tech_name);
extern HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,DWORD resource);
extern HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const char *filename);
extern HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,DWORD resource,const std::map<std::string,std::string>&defines);
extern HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const char *filename,const std::map<std::string,std::string>&defines);
extern HRESULT CreateDX9Texture(LPDIRECT3DDEVICE9 m_pd3dDevice,LPDIRECT3DTEXTURE9 &texture,DWORD resource);
extern HRESULT CreateDX9Texture(LPDIRECT3DDEVICE9 m_pd3dDevice,LPDIRECT3DTEXTURE9 &texture,const char *filename);
extern HRESULT CanUseTexFormat(IDirect3DDevice9 *device,D3DFORMAT f);
extern HRESULT CanUseDepthFormat(IDirect3DDevice9 *device,D3DFORMAT f);
extern HRESULT CanUse16BitFloats(IDirect3DDevice9 *device);

extern HRESULT RenderTexture(IDirect3DDevice9 *m_pd3dDevice,int x1,int y1,int dx,int dy,LPDIRECT3DBASETEXTURE9 texture,
							 LPD3DXEFFECT eff=NULL,D3DXHANDLE tech=NULL);

extern void SetBundleShaders(bool b);
extern void SetResourceModule(const char *txt);