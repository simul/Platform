// Copyright (c) 2007-2009 Simul Software Ltd
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
extern int GetShaderModel();
extern void SetShaderModel(int m);
extern void SetShaderPath(const char *path);
// Get the technique, or look for a less hardware-demanding equivalent:
extern D3DXHANDLE GetDX9Technique(LPD3DXEFFECT effect,const char *tech_name);
extern HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,DWORD resource,int num_defines=0,...);
extern HRESULT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const char *filename,int num_defines=0,...);
extern HRESULT CanUseTexFormat(IDirect3DDevice9 *device,D3DFORMAT f);
extern HRESULT CanUseDepthFormat(IDirect3DDevice9 *device,D3DFORMAT f);
extern HRESULT CanUse16BitFloats(IDirect3DDevice9 *device);

extern HRESULT RenderTexture(IDirect3DDevice9 *m_pd3dDevice,int x1,int y1,int dx,int dy,LPDIRECT3DTEXTURE9 texture);