// Copyright (c) 2007-2011 Simul Software Ltd
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
#include "Export.h"

namespace simul
{
	namespace dx1x_namespace
	{
		extern SIMUL_DIRECTX1x_EXPORT void SetShaderPath(const char *path);
		extern SIMUL_DIRECTX1x_EXPORT void SetTexturePath(const char *path);
		extern SIMUL_DIRECTX1x_EXPORT void SetDevice(ID3D1xDevice* dev);
		extern SIMUL_DIRECTX1x_EXPORT  void UnsetDevice();
	}
}
extern ID3D1xShaderResourceView* LoadTexture(const TCHAR *filename);

typedef long HRESULT;
extern SIMUL_DIRECTX1x_EXPORT HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename);
extern SIMUL_DIRECTX1x_EXPORT HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename,const std::map<std::string,std::string>&defines);


extern SIMUL_DIRECTX1x_EXPORT HRESULT Map2D(ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp);
extern SIMUL_DIRECTX1x_EXPORT void Unmap2D(ID3D1xTexture2D *tex);
extern SIMUL_DIRECTX1x_EXPORT HRESULT Map3D(ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp);
extern SIMUL_DIRECTX1x_EXPORT void Unmap3D(ID3D1xTexture3D *tex);
extern SIMUL_DIRECTX1x_EXPORT HRESULT Map1D(ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp);
extern SIMUL_DIRECTX1x_EXPORT void Unmap1D(ID3D1xTexture1D *tex);
#ifdef DX10
extern SIMUL_DIRECTX1x_EXPORT HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,void **vert);
#else
extern SIMUL_DIRECTX1x_EXPORT HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE *vert);
#endif
extern SIMUL_DIRECTX1x_EXPORT void UnmapBuffer(ID3D1xBuffer *vertexBuffer);
extern SIMUL_DIRECTX1x_EXPORT HRESULT ApplyPass(ID3D1xEffectPass *pass);
extern void SIMUL_DIRECTX1x_EXPORT FixProjectionMatrix(struct D3DXMATRIX &proj,float zFar,bool y_vertical);
extern void SIMUL_DIRECTX1x_EXPORT FixProjectionMatrix(struct D3DXMATRIX &proj,float zNear,float zFar,bool y_vertical);

extern void SIMUL_DIRECTX1x_EXPORT MakeCubeMatrices(D3DXMATRIX g_amCubeMapViewAdjust[]);
extern void GetCameraPosVector(D3DXMATRIX &view,bool y_vertical,float *dcam_pos,float *view_dir);
extern void RenderAngledQuad(ID3D1xDevice *m_pd3dDevice,const float *cam_pos,const float *dir,bool y_vertical,float half_angle_radians,ID3D1xEffect* effect,ID3D1xEffectTechnique* tech,D3DXMATRIX view,D3DXMATRIX proj);



void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );
void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );