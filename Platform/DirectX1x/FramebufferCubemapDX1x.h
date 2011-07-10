#pragma once
#include <d3dx9.h>
#ifdef DX10
#include <d3d10.h>
#include <d3dx10.h>
#include <d3d10effect.h>
#else
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#endif
#include "Simul/Platform/DirectX1x/MacrosDx1x.h"
#include "Simul/Platform/DirectX1x/Export.h"
#ifndef FRAMEBUFFER_INTERFACE
#define FRAMEBUFFER_INTERFACE
class FramebufferInterface
{
public:
	virtual void Activate()=0;
	virtual void Deactivate()=0;
	
	virtual void SetWidthAndHeight(int w,int h)=0;
};
#endif

SIMUL_DIRECTX1x_EXPORT_CLASS FramebufferCubemapDX1x:public FramebufferInterface
{
public:
	FramebufferCubemapDX1x();
	virtual ~FramebufferCubemapDX1x();
	void SetWidthAndHeight(int w,int h);
	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	bool RestoreDeviceObjects(ID3D1xDevice* pd3dDevice);
	//! Call this when the device has been lost.
	bool InvalidateDeviceObjects();
	void SetCurrentFace(int i);
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	void Activate();
	void Deactivate();
	void *GetTextureResource()
	{
		return (void*)m_pCubeEnvMapSRV;
	}
protected:
	//! The size of the 2D buffer the sky is rendered to.
	int Width,Height;
protected:
	ID3D1xDeviceContext *			m_pImmediateContext;
	ID3D1xRenderTargetView*			m_pOldRenderTarget;
	ID3D1xDepthStencilView*			m_pOldDepthSurface;
	D3D1x_VIEWPORT					m_OldViewports[4];
	
	ID3D1xTexture2D*				m_pCubeEnvDepthMap;
	ID3D1xTexture2D*				m_pCubeEnvMap;
	ID3D1xRenderTargetView*			m_pCubeEnvMapRTV[6];
	ID3D1xDepthStencilView*			m_pCubeEnvDepthMapDSV[6];
	ID3D1xShaderResourceView*		m_pCubeEnvMapSRV;
	int								current_face;
};
