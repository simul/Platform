// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.h Create a DirectX .fx effect and report errors.
#ifdef DX10
#include <d3dx10.h>
#include <d3d10effect.h>
#else
#include <d3dx11.h>
#include <d3dx11effect.h>
#endif
#include <map>
#include "MacrosDX1x.h"

extern void SetShaderPath(const char *path);
extern void SetTexturePath(const char *path);
typedef long HRESULT;
extern HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename);
extern HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename,const std::map<std::string,std::string>&defines);

extern void SetDevice(ID3D1xDevice* dev);

extern HRESULT Map2D(ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp);
extern void Unmap2D(ID3D1xTexture2D *tex);
extern HRESULT Map3D(ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp);
extern void Unmap3D(ID3D1xTexture3D *tex);
extern HRESULT Map1D(ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp);
extern void Unmap1D(ID3D1xTexture1D *tex);
#ifdef DX10
extern HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,void **vert);
#else
extern HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE *vert);
#endif
extern void UnmapBuffer(ID3D1xBuffer *vertexBuffer);
extern HRESULT ApplyPass(ID3D1xEffectPass *pass);