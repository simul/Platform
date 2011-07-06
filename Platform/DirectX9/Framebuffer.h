#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Platform/DirectX9/Export.h"
#ifndef FRAMEBUFFER_INTERFACE
#define FRAMEBUFFER_INTERFACE
class FramebufferInterface
{
public:
	virtual void Activate()=0;
	virtual void Deactivate()=0;
	virtual void DeactivateAndRender(bool blend)=0;
	virtual void SetWidthAndHeight(int w,int h)=0;
};
#endif

SIMUL_DIRECTX9_EXPORT_CLASS Framebuffer:public FramebufferInterface
{
public:
	Framebuffer();
	~Framebuffer();
	void SetWidthAndHeight(int w,int h);
	bool RestoreDeviceObjects(void *pd3dDevice);
	bool InvalidateDeviceObjects();
	//! The texture the sky and clouds are rendered to.
	LPDIRECT3DTEXTURE9				hdr_buffer_texture;
	LPDIRECT3DTEXTURE9				buffer_depth_texture;
	void Activate();
	void Deactivate();
	void DeactivateAndRender(bool blend);
	void Render(bool blend);
	int Width,Height;
protected:
	LPDIRECT3DDEVICE9	m_pd3dDevice;
	LPDIRECT3DSURFACE9	m_pHDRRenderTarget;
	LPDIRECT3DSURFACE9	m_pBufferDepthSurface;
	LPDIRECT3DSURFACE9	m_pOldRenderTarget;
	LPDIRECT3DSURFACE9	m_pOldDepthSurface;
};
