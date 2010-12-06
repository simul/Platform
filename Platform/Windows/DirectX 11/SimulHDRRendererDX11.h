// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
	#include <d3dx9.h>
	#include <d3DX11.h>
	#include <D3dx11effect.h>
typedef long HRESULT;
class SimulHDRRendererDX11
{
public:
	SimulHDRRendererDX11();
	virtual ~SimulHDRRendererDX11();
	//standard d3d object interface functions

	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	HRESULT RestoreDeviceObjects(ID3D11Device* pd3dDevice,IDXGISwapChain* pSwapChain);
	//! Call this when the device has been lost.
	HRESULT InvalidateDeviceObjects();
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	HRESULT StartRender();
	//! ApplyFade: call this after rendering the solid stuff, before rendering transparent and background imagery.
	HRESULT ApplyFade();
	//! FinishRender: wraps up rendering to the HDR target, and then uses tone mapping to render this HDR image to the screen. Call at the end of the frame's rendering.
	HRESULT FinishRender();

	//! Set the exposure - a brightness factor.
	void SetExposure(float ex){exposure=ex;}
	//! Get the current debug text as a c-string pointer.
	const char *GetDebugText() const;
	//! Get a timing value for debugging.
	float GetTiming() const;
	//! Set the atmospherics renderer - null means no post-process fade.
	void SetAtmospherics(class SimulAtmosphericsInterface *a){atmospherics=a;}
protected:
	int screen_width;
	int screen_height;
	HRESULT Destroy();
	//! The size of the 2D buffer the sky is rendered to.
	int BufferWidth,BufferHeight;
	ID3D11Device*						m_pd3dDevice;
	ID3D11DeviceContext *				m_pImmediateContext;
	IDXGISwapChain*						m_pSwapChain;
	ID3D11InputLayout*					m_pBufferVertexDecl;
	ID3D11Buffer						*m_pVertexBuffer;

	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	ID3DX11Effect*							m_pTonemapEffect;
	ID3DX11EffectTechnique*					TonemapTechnique;
	ID3DX11EffectScalarVariable*			Exposure;
	ID3DX11EffectScalarVariable*			Gamma;
	ID3DX11EffectMatrixVariable*			worldViewProj;
	ID3DX11EffectShaderResourceVariable*	hdrTexture;

	ID3D11RenderTargetView*				m_pHDRRenderTarget[1];
	ID3D11DepthStencilView*				m_pBufferDepthSurface;
	ID3D11RenderTargetView*				m_pLDRRenderTarget[1];
	ID3D11RenderTargetView*				m_pOldRenderTarget[1];
	ID3D11DepthStencilView*				m_pOldDepthSurface;

	//! The texture the scene is rendered to.
	ID3D11Texture2D*					hdr_buffer_texture;
	ID3D11ShaderResourceView*			hdr_buffer_texture_SRV;
	//! The texture the fade is applied to.
	ID3D11Texture2D*					faded_texture;
	ID3D11ShaderResourceView*			faded_texture_SRV;
	//! The depth buffer.
	ID3D11Texture2D*					buffer_depth_texture;
	ID3D11ShaderResourceView*			buffer_depth_texture_SRV;

	HRESULT IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat);
	HRESULT CreateBuffers();
	HRESULT RenderBufferToCurrentTarget(bool do_tonemap);
	class SimulSkyRenderer *simulSkyRenderer;
	class SimulCloudRendererDX11 *simulCloudRenderer;
	class Simul2DCloudRenderer *simul2DCloudRenderer;
	class SimulPrecipitationRenderer *simulPrecipitationRenderer;
	float							exposure;
	float							gamma;
	ID3D11RenderTargetView* MakeRenderTarget(const ID3D11Texture2D* pTexture);
	float timing;
	float exposure_multiplier;
	class SimulAtmosphericsInterface *atmospherics;
};