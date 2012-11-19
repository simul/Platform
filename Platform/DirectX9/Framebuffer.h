#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Clouds/BaseFramebuffer.h"

SIMUL_DIRECTX9_EXPORT_CLASS Framebuffer:public BaseFramebuffer
{
public:
	Framebuffer();
	~Framebuffer();
	void SetWidthAndHeight(int w,int h);
	void RestoreDeviceObjects(void *pd3dDevice);
	void InvalidateDeviceObjects();
	//! The texture the sky and clouds are rendered to.
	LPDIRECT3DTEXTURE9				hdr_buffer_texture;
	LPDIRECT3DTEXTURE9				buffer_depth_texture;
	LPDIRECT3DSURFACE9	m_pHDRRenderTarget;
	void Activate();
	void Deactivate();
	void Clear(float,float,float,float);
	void DeactivateAndRender(bool blend);
	void Render(bool blend);
	void SetFormat(D3DFORMAT f);
	int Width,Height;
protected:
	LPDIRECT3DDEVICE9	m_pd3dDevice;
	LPDIRECT3DSURFACE9	m_pBufferDepthSurface;
	LPDIRECT3DSURFACE9	m_pOldRenderTarget;
	LPDIRECT3DSURFACE9	m_pOldDepthSurface;
	D3DFORMAT hdr_format;
};
