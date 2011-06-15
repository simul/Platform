#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Platform/DirectX 9/Export.h"

SIMUL_DIRECTX9_EXPORT_CLASS Framebuffer
{
public:
	Framebuffer();
	~Framebuffer();
	bool RestoreDeviceObjects(void *pd3dDevice,int w,int h);
	bool InvalidateDeviceObjects();
	//! The texture the sky and clouds are rendered to.
	LPDIRECT3DTEXTURE9				hdr_buffer_texture;
	LPDIRECT3DTEXTURE9				buffer_depth_texture;
	void Activate();
	void Deactivate();
	void DeactivateAndRender();
	int Width,Height;
protected:
	LPDIRECT3DDEVICE9	m_pd3dDevice;
	LPDIRECT3DSURFACE9	m_pHDRRenderTarget;
	LPDIRECT3DSURFACE9	m_pBufferDepthSurface;
	LPDIRECT3DSURFACE9	m_pOldRenderTarget;
	LPDIRECT3DSURFACE9	m_pOldDepthSurface;
};
