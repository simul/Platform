// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.h Create a DirectX .fx effect and report errors.
#ifndef CREATEEFFECTDX1X_H
#define CREATEEFFECTDX1X_H
#include "SimulDirectXHeader.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
inline D3DXMATRIX D3DXMatrixMultiply(const D3DXMATRIX &a,const D3DXMATRIX &b)
{
	return a*b;
}
#else
#include <DirectXMath.h>
#define D3DXMATRIX DirectX::XMFLOAT4X4
#define D3DXVECTOR4 DirectX::XMFLOAT4
#define D3DXVECTOR3 DirectX::XMFLOAT3
#define D3DXVECTOR2 DirectX::XMFLOAT2
struct D3DVECTOR 
{
	float x,y,z;
	D3DVECTOR operator+(D3DVECTOR b)
	{
		D3DVECTOR v={x+b.x,y+b.y,z+b.z};
		return v;
	}
	void operator=(D3DVECTOR b)
	{
		x=b.x;
		y=b.y;
		z=b.z;
	}
};
inline float D3DXFresnelTerm(float c, float r)
{
	DirectX::XMVECTOR C=DirectX::XMLoadFloat(&c);
	DirectX::XMVECTOR R=DirectX::XMLoadFloat(&r);
 	DirectX::XMVECTOR F= DirectX::XMFresnelTerm(C,R);
	float o;
	DirectX::XMStoreFloat(&o,F);
	return o;
}

#define D3DX_PI (3.1415926536f)
inline void D3DXVec2Normalize(D3DXVECTOR2 *dest,const D3DXVECTOR2 *src)
{
	DirectX::XMVECTOR s=DirectX::XMLoadFloat2(src);
	DirectX::XMVECTOR d;
	d=DirectX::XMVector2Normalize(s);
	DirectX::XMStoreFloat2(dest,d);
}
inline void D3DXMatrixScaling(D3DXMATRIX *s,float x,float y,float z)
{
	DirectX::XMMATRIX S=DirectX::XMMatrixScaling(x,y,z);
	DirectX::XMStoreFloat4x4(s,S);
}

inline void D3DXMatrixTranslation(D3DXMATRIX *s,float x,float y,float z)
{
	DirectX::XMMATRIX S=DirectX::XMMatrixTranslation(x,y,z);
	DirectX::XMStoreFloat4x4(s,S);
}

inline void D3DXVec3TransformCoord(D3DXVECTOR3 *dest, const D3DXVECTOR3 *src, D3DXMATRIX *m)
{
	DirectX::XMMATRIX M=DirectX::XMLoadFloat4x4(m);
	DirectX::XMVECTOR s=DirectX::XMLoadFloat3(src);
	DirectX::XMVECTOR d=DirectX::XMVector3TransformCoord(s,M);
	DirectX::XMStoreFloat3(dest,d);
}
inline void D3DXMatrixTranspose(D3DXMATRIX *dest,const D3DXMATRIX *src)
{
	DirectX::XMMATRIX d;
	DirectX::XMMATRIX s=DirectX::XMLoadFloat4x4(src);
	d=DirectX::XMMatrixTranspose(s);
	DirectX::XMStoreFloat4x4(dest,d);
}

inline D3DXMATRIX *D3DXMatrixMultiply(D3DXMATRIX *c,const D3DXMATRIX *a,const D3DXMATRIX *b)
{
	DirectX::XMMATRIX A=DirectX::XMLoadFloat4x4(a);
	DirectX::XMMATRIX B=DirectX::XMLoadFloat4x4(b);
	DirectX::XMMATRIX C=DirectX::XMMatrixMultiply(A,B);
	
	DirectX::XMStoreFloat4x4(c,C);
	return c;
}

inline D3DXMATRIX D3DXMatrixMultiply(const D3DXMATRIX &a,const D3DXMATRIX &b)
{
	DirectX::XMMATRIX A=DirectX::XMLoadFloat4x4(&a);
	DirectX::XMMATRIX B=DirectX::XMLoadFloat4x4(&b);
	DirectX::XMMATRIX C=DirectX::XMMatrixMultiply(A,B);
	D3DXMATRIX c;
	DirectX::XMStoreFloat4x4(&c,C);
	return c;
}

inline void D3DXMatrixInverse(D3DXMATRIX *dest,D3DXVECTOR3 *det,const D3DXMATRIX *src)
{
	DirectX::XMMATRIX d;
	DirectX::XMMATRIX s=DirectX::XMLoadFloat4x4(src);
	if(det)
	{
		DirectX::XMVECTOR det2=DirectX::XMLoadFloat3(det);
		d=DirectX::XMMatrixInverse(&det2,s);
		DirectX::XMStoreFloat3(det,det2);
	}
	else
	{
		d=DirectX::XMMatrixInverse(NULL,s);
	}
	DirectX::XMStoreFloat4x4(dest,d);
}
inline void D3DXMatrixIdentity(D3DXMATRIX *i)
{
	DirectX::XMMATRIX I=DirectX::XMMatrixIdentity();
	DirectX::XMStoreFloat4x4(i,I);
}
inline void D3DXMatrixOrthoLH(D3DXMATRIX *ortho,float width,float height,float zmin,float zmax)
{
	DirectX::XMMATRIX M=DirectX::XMMatrixOrthographicLH(width,height,zmin,zmax);
	DirectX::XMStoreFloat4x4(ortho,M);
}
inline D3DXMATRIX* D3DXMatrixPerspectiveFovRH( D3DXMATRIX *pOut, FLOAT fovy, FLOAT Aspect, FLOAT zn, FLOAT zf )
{
	DirectX::XMMATRIX M=DirectX::XMMatrixPerspectiveFovRH(fovy,Aspect,zn,zf);
	DirectX::XMStoreFloat4x4(pOut,M);
	return pOut;
}
inline void D3DXMatrixLookAtRH(D3DXMATRIX *m, D3DVECTOR *vEyePt,D3DVECTOR *vLookAt, D3DVECTOR *vUpDir )
{
	DirectX::XMVECTOR e=DirectX::XMLoadFloat3(&DirectX::XMFLOAT3((const float*)vEyePt));
	DirectX::XMVECTOR l=DirectX::XMLoadFloat3(&DirectX::XMFLOAT3((const float*)vLookAt));
	DirectX::XMVECTOR u=DirectX::XMLoadFloat3(&DirectX::XMFLOAT3((const float*)vUpDir));
	DirectX::XMMATRIX M=DirectX::XMMatrixLookAtRH(e,l,u);
	DirectX::XMStoreFloat4x4(m,M);
}

#endif

#include <map>
#include <vector>
#include "MacrosDX1x.h"
#include "Export.h"

struct VertexXyzRgba;
struct ID3DX11Effect;
struct ID3DX11EffectTechnique;
struct ID3DX11EffectPass;

typedef long HRESULT;
namespace simul
{
	namespace crossplatform
	{
		class Effect;
	}
	namespace dx11
	{
		//! Find the camera position and view direction from the given view matrix.
		extern SIMUL_DIRECTX11_EXPORT void GetCameraPosVector(const float *v,float *dcam_pos,float *view_dir,bool y_vertical=false);
		//! Find the camera position from the given view matrix.
		extern SIMUL_DIRECTX11_EXPORT const float *GetCameraPosVector(const float *v,bool y_vertical=false);
		//! Call this to make the FX compiler put its warnings and errors to the standard output when used.
		extern SIMUL_DIRECTX11_EXPORT void PipeCompilerOutput(bool p);
		extern SIMUL_DIRECTX11_EXPORT void PushShaderPath(const char *path);
		extern SIMUL_DIRECTX11_EXPORT std::vector<std::string> GetShaderPathsUtf8();
		extern SIMUL_DIRECTX11_EXPORT void SetShaderPathsUtf8(const std::vector<std::string> &pathsUtf8);
		extern SIMUL_DIRECTX11_EXPORT void SetShaderBinaryPath(const char *path_utf8);
		extern SIMUL_DIRECTX11_EXPORT const char *GetShaderBinaryPathUtf8();
		extern SIMUL_DIRECTX11_EXPORT void PopShaderPath();
		extern SIMUL_DIRECTX11_EXPORT ID3D11ShaderResourceView* LoadTexture(ID3D11Device* dev,const char *filename,const std::vector<std::string> &texturePathsUtf8);
		extern SIMUL_DIRECTX11_EXPORT ID3D11Texture2D* LoadStagingTexture(ID3D11Device* dev,const char *filename,const std::vector<std::string> &texturePathsUtf8);
		ID3D11Texture1D* make1DTexture(
										ID3D11Device			*m_pd3dDevice
										,int w
										,DXGI_FORMAT format
										,const float *src);
		ID3D11Texture2D* make2DTexture(
										ID3D11Device			*m_pd3dDevice
										,int w,int h
										,DXGI_FORMAT format
										,const float *src);
		ID3D11Texture3D* make3DTexture(
										ID3D11Device			*m_pd3dDevice
										,int w,int l,int d
										,DXGI_FORMAT format
										,const void *src);
		void Ensure3DTextureSizeAndFormat(
											ID3D11Device			*m_pd3dDevice
											,ID3D1xTexture3D		* &tex
											,ID3D11ShaderResourceView* &srv
											,int w,int l,int d
											,DXGI_FORMAT format);
		// These functions encapsulate getting an effect variable of the given name if it exists, and
		// if so, setting its value. Due to inefficiency it is best to replace usage of this over time
		// with Effect variable pointers, but this is a good way to write new render code quickly
		void SIMUL_DIRECTX11_EXPORT setDepthState			(ID3DX11Effect *effect	,const char *name	,ID3D11DepthStencilState * value);
		void SIMUL_DIRECTX11_EXPORT setSamplerState			(ID3DX11Effect *effect	,const char *name	,ID3D11SamplerState * value);
		bool SIMUL_DIRECTX11_EXPORT setTexture				(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView * value);
		void SIMUL_DIRECTX11_EXPORT applyPass				(ID3D11DeviceContext *pContext,ID3DX11Effect *effect,const char *name,int pass=0);
		void SIMUL_DIRECTX11_EXPORT applyPass				(ID3D11DeviceContext *pContext,ID3DX11Effect *effect,const char *name,const char *pass);
		bool SIMUL_DIRECTX11_EXPORT setUnorderedAccessView	(ID3DX11Effect *effect	,const char *name	,ID3D11UnorderedAccessView * value);
		void SIMUL_DIRECTX11_EXPORT setTextureArray			(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView *value);
		void SIMUL_DIRECTX11_EXPORT setStructuredBuffer		(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView * value);
		void SIMUL_DIRECTX11_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,float value);
		void SIMUL_DIRECTX11_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,float x,float y);
		void SIMUL_DIRECTX11_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,float x,float y,float z,float w);
		void SIMUL_DIRECTX11_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,int value);
		void SIMUL_DIRECTX11_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,const float *vec);
		void SIMUL_DIRECTX11_EXPORT setMatrix				(ID3DX11Effect *effect	,const char *name	,const float *value);
		void SIMUL_DIRECTX11_EXPORT setConstantBuffer		(ID3DX11Effect *effect	,const char *name	,ID3D11Buffer *b);
		void SIMUL_DIRECTX11_EXPORT unbindTextures			(ID3DX11Effect *effect);
							
		int ByteSizeOfFormatElement( DXGI_FORMAT format );
		//! Create an effect from the named .fx file. Depending on what was passed to SetShaderBuildMode(), this may instead simply load the binary .fxo file that corresponds to the given filename.
		extern SIMUL_DIRECTX11_EXPORT HRESULT CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const char *filename,const std::vector<std::string> &shaderPathsUtf8,crossplatform::ShaderBuildMode shaderBuildMode);
		//! Create an effect from the named .fx file. Depending on what was passed to SetShaderBuildMode(), this may instead simply load the binary .fxo file that corresponds to the given filename and defines.
		extern SIMUL_DIRECTX11_EXPORT HRESULT CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const char *filename,const std::map<std::string,std::string>&defines,const std::vector<std::string> &shaderPathsUtf8,unsigned int shader_flags,crossplatform::ShaderBuildMode shaderBuildMode);
	}
}

#ifndef D3DCOMPILE_OPTIMIZATION_LEVEL3
#define D3DCOMPILE_OPTIMIZATION_LEVEL3            (1 << 15)
#endif

extern SIMUL_DIRECTX11_EXPORT HRESULT Map2D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap2D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture2D *tex);
extern SIMUL_DIRECTX11_EXPORT HRESULT Map3D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap3D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture3D *tex);
extern SIMUL_DIRECTX11_EXPORT HRESULT Map1D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap1D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture1D *tex);

extern SIMUL_DIRECTX11_EXPORT HRESULT MapBuffer(ID3D11DeviceContext *pImmediateContext,ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE *vert);
extern SIMUL_DIRECTX11_EXPORT void UnmapBuffer(ID3D11DeviceContext *pImmediateContext,ID3D1xBuffer *vertexBuffer);
extern SIMUL_DIRECTX11_EXPORT HRESULT ApplyPass(ID3D11DeviceContext *pImmediateContext,ID3DX11EffectPass *pass);

#define PAD16(n) (((n)+15)/16*16)
#endif
