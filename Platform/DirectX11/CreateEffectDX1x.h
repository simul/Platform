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
#include <D3Dcompiler.h>
#endif
#include <d3dx9.h>
#include <map>
#include "MacrosDX1x.h"
#include "Export.h"
struct VertexXyzRgba;
namespace simul
{
	namespace dx11
	{
		extern SIMUL_DIRECTX11_EXPORT void GetCameraPosVector(D3DXMATRIX &view,bool y_vertical,float *dcam_pos,float *view_dir);
		extern SIMUL_DIRECTX11_EXPORT const float *GetCameraPosVector(D3DXMATRIX &view,bool y_vertical);
		//! Call this to make the FX compiler put its warnings and errors to the standard output when used.
		extern SIMUL_DIRECTX11_EXPORT void PipeCompilerOutput(bool p);
		extern SIMUL_DIRECTX11_EXPORT void SetShaderPath(const char *path);
		extern SIMUL_DIRECTX11_EXPORT void SetTexturePath(const char *path);
		extern SIMUL_DIRECTX11_EXPORT void SetDevice(ID3D1xDevice* dev);
		extern SIMUL_DIRECTX11_EXPORT void UnsetDevice();
		extern SIMUL_DIRECTX11_EXPORT void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj);
		extern ID3D1xShaderResourceView* LoadTexture(const char *filename);
		extern ID3D1xShaderResourceView* LoadTexture(const TCHAR *filename);
		ID3D1xTexture1D* make1DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xDeviceContext	*m_pImmediateContext
							,int w
							,DXGI_FORMAT format
							,const float *src);
		ID3D11Texture2D* make2DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xDeviceContext	*m_pImmediateContext
							,int w,int h
							,DXGI_FORMAT format
							,const float *src);
		ID3D1xTexture3D* make3DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xDeviceContext	*m_pImmediateContext
							,int w,int l,int d
							,DXGI_FORMAT format
							,const float *src);
							
		void Ensure3DTextureSizeAndFormat(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xDeviceContext	*m_pImmediateContext
							,ID3D1xTexture3D* &tex
							,ID3D11ShaderResourceView* &srv
							,int w,int l,int d
							,DXGI_FORMAT format);
							
		void SIMUL_DIRECTX11_EXPORT FixProjectionMatrix(struct D3DXMATRIX &proj,float zFar,bool y_vertical);
		void SIMUL_DIRECTX11_EXPORT FixProjectionMatrix(struct D3DXMATRIX &proj,float zNear,float zFar,bool y_vertical);
	
		void setParameter(ID3D1xEffect *effect,const char *name	,ID3D11ShaderResourceView * value);
		void setParameter(ID3D1xEffect *effect,const char *name	,float value);
		void setParameter(ID3D1xEffect *effect,const char *name	,int value);
		void setParameter(ID3D1xEffect *effect,const char *name	,float *vec);
		void setMatrix(ID3D1xEffect *effect	,const char *name	,const float *value);
							
		int ByteSizeOfFormatElement( DXGI_FORMAT format );
							
		class UtilityRenderer
		{
			static int instance_count;
			static int screen_width;
			static int screen_height;
			static D3DXMATRIX view;
			static D3DXMATRIX proj;
			static ID3D1xEffect *m_pDebugEffect;
		public:
			UtilityRenderer();
			~UtilityRenderer();
			static void SetMatrices(D3DXMATRIX v,D3DXMATRIX p);
			static void RestoreDeviceObjects(void *m_pd3dDevice);
			static void RecompileShaders();
			static void SetScreenSize(int w,int h);
			static void InvalidateDeviceObjects();
			static void PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
			static void DrawLines(VertexXyzRgba *lines,int vertex_count,bool strip);
		};
		
	}
}

typedef long HRESULT;
extern SIMUL_DIRECTX11_EXPORT HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename);
extern SIMUL_DIRECTX11_EXPORT HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename,const std::map<std::string,std::string>&defines);


extern SIMUL_DIRECTX11_EXPORT HRESULT Map2D(ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap2D(ID3D1xTexture2D *tex);
extern SIMUL_DIRECTX11_EXPORT HRESULT Map3D(ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap3D(ID3D1xTexture3D *tex);
extern SIMUL_DIRECTX11_EXPORT HRESULT Map1D(ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap1D(ID3D1xTexture1D *tex);
#ifdef DX10
extern SIMUL_DIRECTX11_EXPORT HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,void **vert);
#else
extern SIMUL_DIRECTX11_EXPORT HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE *vert);
#endif
extern SIMUL_DIRECTX11_EXPORT void UnmapBuffer(ID3D1xBuffer *vertexBuffer);
extern SIMUL_DIRECTX11_EXPORT HRESULT ApplyPass(ID3D1xEffectPass *pass);

extern void SIMUL_DIRECTX11_EXPORT MakeCubeMatrices(D3DXMATRIX g_amCubeMapViewAdjust[],const float *cam_pos);
extern void RenderAngledQuad(ID3D1xDevice *m_pd3dDevice,const float *dir,bool y_vertical,float half_angle_radians,ID3D1xEffect* effect,ID3D1xEffectTechnique* tech,D3DXMATRIX view,D3DXMATRIX proj,D3DXVECTOR3 sun_dir);
extern void RenderTexture(ID3D1xDevice *m_pd3dDevice,int x1,int y1,int dx,int dy,ID3D1xEffectTechnique* tech);
extern void RenderTexture(ID3D1xDevice *m_pd3dDevice,float x1,float y1,float dx,float dy,ID3D1xEffectTechnique* tech);

void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );
void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );

#define PAD16(n) (((n)+15)/16*16)