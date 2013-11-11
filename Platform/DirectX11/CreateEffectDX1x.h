// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.h Create a DirectX .fx effect and report errors.
#ifndef CREATEEFFECTDX1X_H
#define CREATEEFFECTDX1X_H
#include <d3dx11.h>
#include "Simul/External/DirectX/Effects11/Inc/D3dx11effect.h"
#include <D3Dcompiler.h>
#include <d3dx9.h>
#include <map>
#include "MacrosDX1x.h"
#include "Export.h"
struct VertexXyzRgba;
namespace simul
{
	namespace dx11
	{
		enum ShaderBuildMode
		{
			ALWAYS_BUILD=1,BUILD_IF_CHANGED,NEVER_BUILD
		};
		//! Find the camera position and view direction from the given view matrix.
		extern SIMUL_DIRECTX11_EXPORT void GetCameraPosVector(const float *v,float *dcam_pos,float *view_dir,bool y_vertical=false);
		//! Find the camera position from the given view matrix.
		extern SIMUL_DIRECTX11_EXPORT const float *GetCameraPosVector(const float *v,bool y_vertical=false);
		//! Call this to make the FX compiler put its warnings and errors to the standard output when used.
		extern SIMUL_DIRECTX11_EXPORT void PipeCompilerOutput(bool p);
		//! When shader should be built, or loaded if available.
		extern SIMUL_DIRECTX11_EXPORT void SetShaderBuildMode(ShaderBuildMode s);
		extern SIMUL_DIRECTX11_EXPORT void SetShaderPath(const char *path);
		extern SIMUL_DIRECTX11_EXPORT void SetTexturePath(const char *path);
		extern SIMUL_DIRECTX11_EXPORT void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,const float *world,const float *view,const float *proj);
		extern SIMUL_DIRECTX11_EXPORT ID3D11ShaderResourceView* LoadTexture(ID3D11Device* dev,const char *filename);
		extern SIMUL_DIRECTX11_EXPORT ID3D11Texture2D* LoadStagingTexture(ID3D11Device* dev,const char *filename);
		ID3D11Texture1D* make1DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w
							,DXGI_FORMAT format
							,const float *src);
		ID3D11Texture2D* make2DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w,int h
							,DXGI_FORMAT format
							,const float *src);
		ID3D11Texture3D* make3DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w,int l,int d
							,DXGI_FORMAT format
							,const void *src);
		void Ensure3DTextureSizeAndFormat(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xTexture3D		* &tex
							,ID3D11ShaderResourceView* &srv
							,int w,int l,int d
							,DXGI_FORMAT format);
							
		D3DXMATRIX SIMUL_DIRECTX11_EXPORT ConvertReversedToRegularProjectionMatrix(const D3DXMATRIX &proj);
	
		// These functions encapsulate getting an effect variable of the given name if it exists, and
		// if so, setting its value. Due to inefficiency it is best to replace usage of this over time
		// with Effect variable pointers, but this is a good way to write new render code quickly
		void setDepthState			(ID3DX11Effect *effect	,const char *name	,ID3D11DepthStencilState * value);
		void setSamplerState		(ID3DX11Effect *effect	,const char *name	,ID3D11SamplerState * value);
		void setTexture				(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView * value);
		void applyPass				(ID3D11DeviceContext *pContext,ID3DX11Effect *effect,const char *name,int pass=0);
		void setUnorderedAccessView	(ID3DX11Effect *effect	,const char *name	,ID3D11UnorderedAccessView * value);
		void setTextureArray		(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView *value);
		void setStructuredBuffer	(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView * value);
		void setParameter			(ID3DX11Effect *effect	,const char *name	,float value);
		void setParameter			(ID3DX11Effect *effect	,const char *name	,float x,float y);
		void setParameter			(ID3DX11Effect *effect	,const char *name	,float x,float y,float z,float w);
		void setParameter			(ID3DX11Effect *effect	,const char *name	,int value);
		void setParameter			(ID3DX11Effect *effect	,const char *name	,float *vec);
		void setMatrix				(ID3DX11Effect *effect	,const char *name	,const float *value);
		void setConstantBuffer		(ID3DX11Effect *effect	,const char *name	,ID3D11Buffer *b);
		void unbindTextures			(ID3DX11Effect *effect);
							
		int ByteSizeOfFormatElement( DXGI_FORMAT format );
	}
}

typedef long HRESULT;
extern SIMUL_DIRECTX11_EXPORT ID3D11ComputeShader *LoadComputeShader(ID3D1xDevice *d3dDevice,const char *filename);
//! Create an effect from the named .fx file. Depending on what was passed to SetShaderBuildMode(), this may instead simply load the binary .fxo file that corresponds to the given filename.
extern SIMUL_DIRECTX11_EXPORT HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3DX11Effect **effect,const char *filename);
//! Create an effect from the named .fx file. Depending on what was passed to SetShaderBuildMode(), this may instead simply load the binary .fxo file that corresponds to the given filename. In that case, the defines are ignored.
extern SIMUL_DIRECTX11_EXPORT HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3DX11Effect **effect,const char *filename,const std::map<std::string,std::string>&defines);
extern SIMUL_DIRECTX11_EXPORT ID3DX11Effect *LoadEffect(ID3D1xDevice *d3dDevice,const char *filename_utf8);
extern SIMUL_DIRECTX11_EXPORT ID3DX11Effect *LoadEffect(ID3D1xDevice *d3dDevice,const char *filename_utf8,const std::map<std::string,std::string>&defines);

extern SIMUL_DIRECTX11_EXPORT HRESULT Map2D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap2D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture2D *tex);
extern SIMUL_DIRECTX11_EXPORT HRESULT Map3D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap3D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture3D *tex);
extern SIMUL_DIRECTX11_EXPORT HRESULT Map1D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap1D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture1D *tex);

extern SIMUL_DIRECTX11_EXPORT HRESULT MapBuffer(ID3D11DeviceContext *pImmediateContext,ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE *vert);
extern SIMUL_DIRECTX11_EXPORT void UnmapBuffer(ID3D11DeviceContext *pImmediateContext,ID3D1xBuffer *vertexBuffer);
extern SIMUL_DIRECTX11_EXPORT HRESULT ApplyPass(ID3D11DeviceContext *pImmediateContext,ID3DX11EffectPass *pass);

extern void SIMUL_DIRECTX11_EXPORT MakeCubeMatrices(D3DXMATRIX g_amCubeMapViewAdjust[],const float *cam_pos,bool ReverseDepth);

void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );
void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );

#define PAD16(n) (((n)+15)/16*16)
#endif
