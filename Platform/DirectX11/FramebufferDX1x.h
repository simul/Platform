#pragma once

#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Clouds/BaseFramebuffer.h"

SIMUL_DIRECTX11_EXPORT_CLASS FramebufferDX1x:public BaseFramebuffer
{
public:
	FramebufferDX1x(int w=0,int h=0);
	virtual ~FramebufferDX1x();
	void SetWidthAndHeight(int w,int h);
	void SetFormat(int f);
	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	void RestoreDeviceObjects(void* pd3dDevice);
	//! Call to recompile the shaders - useful for debugging.
	void RecompileShaders();
	//! Call this when the device has been lost.
	void InvalidateDeviceObjects();
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	void Activate();
	void Deactivate();
	void Render(void *context,bool blend);
	void Clear(float,float,float,float,int mask=0);
	//! FinishRender: wraps up rendering to the HDR target, and then uses tone mapping to render this HDR image to the screen. Call at the end of the frame's rendering.
	void DeactivateAndRender(void *,bool blend);
	bool DrawQuad();
	ID3D1xShaderResourceView *GetBufferResource()
	{
		return buffer_texture_SRV;
	}
	void* GetColorTex()
	{
		return (void*)buffer_texture_SRV;
	}
	ID3D1xTexture2D* GetColorTexResource()
	{
		return hdr_buffer_texture;
	}
	void CopyToMemory(void *target);
	void CopyToMemory(void *target,int start_texel,int texels);
protected:
	DXGI_FORMAT target_format;
	bool Destroy();
	ID3D1xDevice*						m_pd3dDevice;
	ID3D1xDeviceContext *				m_pImmediateContext;
	ID3D1xInputLayout*					m_pBufferVertexDecl;
	ID3D1xBuffer*						m_pVertexBuffer;

public:
	ID3D1xRenderTargetView*				m_pHDRRenderTarget;
	ID3D1xDepthStencilView*				m_pBufferDepthSurface;
protected:
	ID3D11Texture2D *stagingTexture;	// Only initialized if CopyToMemory is invoked.
	
	ID3D1xRenderTargetView*				m_pOldRenderTarget;
	ID3D1xDepthStencilView*				m_pOldDepthSurface;
	D3D1x_VIEWPORT						m_OldViewports[4];

	//! The texture the scene is rendered to.
public:
	ID3D1xTexture2D*					hdr_buffer_texture;
	ID3D1xShaderResourceView*			buffer_texture_SRV;
protected:
	//! The depth buffer.
	ID3D1xTexture2D*					buffer_depth_texture;
	ID3D1xShaderResourceView*			buffer_depth_texture_SRV;

	bool IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat);
	bool CreateBuffers();
	ID3D1xRenderTargetView* MakeRenderTarget(const ID3D1xTexture2D* pTexture);
	float timing;
	unsigned int num_v;
};
