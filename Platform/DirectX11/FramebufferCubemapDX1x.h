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
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Clouds/BaseFramebuffer.h"


SIMUL_DIRECTX11_EXPORT_CLASS FramebufferCubemapDX1x:public BaseFramebuffer
{
public:
	FramebufferCubemapDX1x();
	virtual ~FramebufferCubemapDX1x();
	void SetWidthAndHeight(int w,int h);
	void SetFormat(int i);
	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	void RestoreDeviceObjects(void* );
	//! Call this when the device has been lost.
	void InvalidateDeviceObjects();
	void SetCurrentFace(int i);
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	void Activate(void *context);
	void Deactivate(void *context);
	void Render(bool){}
	void Clear(void *context,float,float,float,float,float,int mask=0);
	virtual void* GetColorTex()
	{
		return (void*)m_pCubeEnvMapSRV;
	}
	ID3D11Texture2D					*GetCopy(void *context);
protected:
	//! The size of the 2D buffer the sky is rendered to.
	int Width,Height;
	ID3D11Texture2D					*stagingTexture;	// Only initialized if CopyToMemory or GetCopy invoked.
	ID3D1xDevice*					pd3dDevice;
	ID3D1xRenderTargetView*			m_pOldRenderTarget;
	ID3D1xDepthStencilView*			m_pOldDepthSurface;
	D3D1x_VIEWPORT					m_OldViewports[4];
	
	ID3D11Texture2D*				m_pCubeEnvDepthMap;
	ID3D11Texture2D*				m_pCubeEnvMap;
	ID3D1xRenderTargetView*			m_pCubeEnvMapRTV[6];
	ID3D1xDepthStencilView*			m_pCubeEnvDepthMapDSV[6];
	ID3D1xShaderResourceView*		m_pCubeEnvMapSRV;
	int								current_face;
	DXGI_FORMAT						format;
};
