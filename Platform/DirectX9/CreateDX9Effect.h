// Copyright (c) 2007-2011 Simul Software Ltd
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
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Clouds/BaseCloudRenderer.h"
enum ShaderModel {NO_SHADERMODEL=0,USE_SHADER_2,USE_SHADER_2A,USE_SHADER_3};
extern ShaderModel SIMUL_DIRECTX9_EXPORT GetShaderModel();
extern void SIMUL_DIRECTX9_EXPORT SetMaxShaderModel(ShaderModel m);
namespace simul
{
	namespace dx9
	{
		extern void SIMUL_DIRECTX9_EXPORT SetShaderPath(const char *path);
		extern void SIMUL_DIRECTX9_EXPORT SetTexturePath(const char *path);
	}
}
// Get the technique, or look for a less hardware-demanding equivalent:
extern D3DXHANDLE SIMUL_DIRECTX9_EXPORT GetDX9Technique(LPD3DXEFFECT effect,const char *tech_name);
extern HRESULT SIMUL_DIRECTX9_EXPORT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,DWORD resource);
extern HRESULT SIMUL_DIRECTX9_EXPORT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const char *filename);
extern HRESULT SIMUL_DIRECTX9_EXPORT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,DWORD resource,const std::map<std::string,std::string>&defines);
extern HRESULT SIMUL_DIRECTX9_EXPORT CreateDX9Effect(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT &effect,const char *filename,const std::map<std::string,std::string>&defines);
extern HRESULT SIMUL_DIRECTX9_EXPORT CreateDX9Texture(LPDIRECT3DDEVICE9 m_pd3dDevice,LPDIRECT3DTEXTURE9 &texture,DWORD resource);
extern HRESULT SIMUL_DIRECTX9_EXPORT CreateDX9Texture(LPDIRECT3DDEVICE9 m_pd3dDevice,LPDIRECT3DTEXTURE9 &texture,const char *filename);
extern HRESULT SIMUL_DIRECTX9_EXPORT CanUseTexFormat(IDirect3DDevice9 *device,D3DFORMAT f);
extern HRESULT SIMUL_DIRECTX9_EXPORT CanUseDepthFormat(IDirect3DDevice9 *device,D3DFORMAT f);
extern HRESULT SIMUL_DIRECTX9_EXPORT CanUse16BitFloats(IDirect3DDevice9 *device);

extern HRESULT SIMUL_DIRECTX9_EXPORT RenderLines(LPDIRECT3DDEVICE9 m_pd3dDevice,int num,const float *pos);

SIMUL_DIRECTX9_EXPORT_CLASS RT
{
	static int count;
public:
	RT();
	~RT();
	static void RestoreDeviceObjects(IDirect3DDevice9 *m_pd3dDevice);
	static void InvalidateDeviceObjects();
	static LPDIRECT3DVERTEXDECLARATION9 m_pBufferVertexDecl;
};
extern HRESULT RenderTexture(IDirect3DDevice9 *m_pd3dDevice,int x1,int y1,int dx,int dy,LPDIRECT3DBASETEXTURE9 texture,
							 LPD3DXEFFECT eff=NULL,D3DXHANDLE tech=NULL);

extern void SIMUL_DIRECTX9_EXPORT SetBundleShaders(bool b);
extern void SIMUL_DIRECTX9_EXPORT SetResourceModule(const char *txt);
extern void SIMUL_DIRECTX9_EXPORT FixProjectionMatrix(D3DXMATRIX &proj,float zFar,bool y_vertical);
extern void SIMUL_DIRECTX9_EXPORT FixProjectionMatrix(D3DXMATRIX &proj,float zNear,float zFar,bool y_vertical);
extern void SIMUL_DIRECTX9_EXPORT SetResourceModule(const char *txt);
extern void SIMUL_DIRECTX9_EXPORT MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj);
extern HRESULT RenderAngledQuad(LPDIRECT3DDEVICE9 m_pd3dDevice,D3DXVECTOR3 cam_pos,D3DXVECTOR3 dir,bool y_vertical,float half_angle_radians,LPD3DXEFFECT effect);
extern HRESULT SIMUL_DIRECTX9_EXPORT DrawFullScreenQuad(LPDIRECT3DDEVICE9 m_pd3dDevice,LPD3DXEFFECT effect);
extern bool SIMUL_DIRECTX9_EXPORT IsDepthFormatOk(LPDIRECT3DDEVICE9 pd3dDevice,D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat);
extern LPDIRECT3DSURFACE9 SIMUL_DIRECTX9_EXPORT MakeRenderTarget(const LPDIRECT3DTEXTURE9 pTexture);
extern void GetCameraPosVector(D3DXMATRIX &view,bool y_vertical,float *dcam_pos,float *view_dir=NULL);
extern std::map<std::string,std::string> MakeDefinesList(simul::clouds::BaseCloudRenderer::FadeMode fade_mode,bool wrap,bool y_vertical);

extern bool PrintAt(LPDIRECT3DDEVICE9 m_pd3dDevice,const float *p,const TCHAR *text,int screen_width,int screen_height,D3DXCOLOR colr,int offsetx=0,int offsety=0);

struct Vertext
{
	float x,y,z;
	float r,g,b,a;
};

extern void DrawLines(LPDIRECT3DDEVICE9 m_pd3dDevice,Vertext *lines,int count,bool strip=false);