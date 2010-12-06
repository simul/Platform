// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulHDRRendererDX11.cpp A renderer for skies, clouds and weather effects.

#include "SimulHDRRendererDX11.h"


#include <tchar.h>
#include <dxerr.h>
#include <string>
#include <assert.h>
typedef std::basic_string<TCHAR> tstring;

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRendererDX11.h"
//#include "SimulPrecipitationRendererDX11.h"
//#include "Simul2DCloudRendererDX11.h"
#include "SimulSkyRendererDX11.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "CreateEffectDX11.h"
#include "SimulAtmosphericsRendererDX11.h"
#include "Macros.h"


#define BLUR_SIZE 9

#include "Simul/Math/Pi.h"

SimulHDRRendererDX11::SimulHDRRendererDX11() :
	m_pBufferVertexDecl(NULL),
	m_pVertexBuffer(NULL),
	m_pd3dDevice(NULL),
	m_pTonemapEffect(NULL),
	hdr_buffer_texture(NULL),
	faded_texture(NULL),
	buffer_depth_texture(NULL),
	hdr_buffer_texture_SRV(NULL),
	faded_texture_SRV(NULL),
	buffer_depth_texture_SRV(NULL),
	exposure(1.f),
	gamma(1.f/2.2f),
	BufferWidth(1024),
	BufferHeight(512),
	timing(0.f),
	exposure_multiplier(1.f),
	atmospherics(NULL)
{
}

HRESULT SimulHDRRendererDX11::RestoreDeviceObjects(ID3D11Device* dev,IDXGISwapChain* pSwapChain)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=dev;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	m_pSwapChain=pSwapChain;
	if(!m_pTonemapEffect)
	{
		V_RETURN(CreateEffect(m_pd3dDevice,&m_pTonemapEffect,_T("media\\HLSL\\gamma.fx")));
	}
	TonemapTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_tonemap");
	Exposure				=m_pTonemapEffect->GetVariableByName("exposure")->AsScalar();
	Gamma					=m_pTonemapEffect->GetVariableByName("gamma")->AsScalar();
	hdrTexture				=m_pTonemapEffect->GetVariableByName("hdrTexture")->AsShaderResource();
	worldViewProj			=m_pTonemapEffect->GetVariableByName("worldViewProj")->AsMatrix();
	V_RETURN(CreateBuffers());
	return hr;
}

HRESULT SimulHDRRendererDX11::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(m_pTonemapEffect);
	SAFE_RELEASE(m_pBufferVertexDecl);
	SAFE_RELEASE(m_pVertexBuffer);

	SAFE_RELEASE(m_pTonemapEffect);

	SAFE_RELEASE(hdr_buffer_texture);
	SAFE_RELEASE(hdr_buffer_texture_SRV);

	SAFE_RELEASE(faded_texture);
	SAFE_RELEASE(faded_texture_SRV);

	SAFE_RELEASE(buffer_depth_texture);
	SAFE_RELEASE(buffer_depth_texture_SRV);

	return hr;
}

HRESULT SimulHDRRendererDX11::Destroy()
{
	return InvalidateDeviceObjects();
}

SimulHDRRendererDX11::~SimulHDRRendererDX11()
{
	Destroy();
}


HRESULT SimulHDRRendererDX11::IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat)
{
	HRESULT hr=S_OK;
	/*LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return hr;

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return hr;

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);
*/
    return hr;
}
struct Vertext
{
	D3DXVECTOR4 pos;
	D3DXVECTOR2 tex;
};

HRESULT SimulHDRRendererDX11::CreateBuffers()
{
	HRESULT hr=S_OK;
	D3D11_TEXTURE2D_DESC desc=
	{
		BufferWidth,
		BufferHeight,
		1,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		{1,0},
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
		0,
		0
	};
	SAFE_RELEASE(hdr_buffer_texture);
	hr=m_pd3dDevice->CreateTexture2D(	&desc,
										NULL,
										&hdr_buffer_texture
									);
	//hdr_buffer_texture->GetDesc(&desc);
	SAFE_RELEASE(hdr_buffer_texture_SRV);
    V_RETURN(m_pd3dDevice->CreateShaderResourceView(hdr_buffer_texture, NULL, &hdr_buffer_texture_SRV ));

	SAFE_RELEASE(faded_texture);
	hr=m_pd3dDevice->CreateTexture2D(	&desc,
										NULL,
										&faded_texture
									);
	SAFE_RELEASE(faded_texture_SRV);
    V_RETURN(m_pd3dDevice->CreateShaderResourceView(faded_texture, NULL, &faded_texture_SRV ));


	desc.Width=BufferWidth/4;
	desc.Height=BufferHeight/4;
	DXGI_FORMAT fmtDepthTex = DXGI_FORMAT_UNKNOWN;
	
	DXGI_FORMAT possibles[]={
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
		DXGI_FORMAT_D32_FLOAT,
		DXGI_FORMAT_D16_UNORM,
		DXGI_FORMAT_UNKNOWN};
	ID3D11Texture2D *pBackBuffer=NULL;
	m_pSwapChain->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&pBackBuffer);
	fmtDepthTex = DXGI_FORMAT_D32_FLOAT;
	pBackBuffer->GetDesc(&desc);
	screen_width=desc.Width;
	screen_height=desc.Height;
	SAFE_RELEASE(pBackBuffer);
/*	D3DDISPLAYMODE d3ddm;

	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
	if(FAILED(hr=d3d->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm )))
    {
        return hr;
    }
	ID3D11Surface* hdr_rt=MakeRenderTarget(hdr_buffer_texture);
	hdr_rt->GetDesc(&desc);
	SAFE_RELEASE(hdr_rt)
	for(int i=0;i<100;i++)
	{
		DXGI_FORMAT possible=possibles[i];
		if(possible==DXGI_FORMAT_UNKNOWN)
			break;
		hr=IsDepthFormatOk(possible,d3ddm.Format,desc.Format);
		if(SUCCEEDED(hr))
		{
			fmtDepthTex = possible;
			break;
		}
	}*/
	SAFE_RELEASE(buffer_depth_texture);
	// Try creating a depth texture
	desc.Width = BufferWidth;
	desc.Height = BufferHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = fmtDepthTex;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	if(fmtDepthTex!=DXGI_FORMAT_UNKNOWN)
		hr=m_pd3dDevice->CreateTexture2D(	&desc,
											NULL,
											&buffer_depth_texture);
	
	const D3D11_INPUT_ELEMENT_DESC decl[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	16,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	SAFE_RELEASE(m_pBufferVertexDecl);
	D3DX11_PASS_DESC PassDesc;
	assert(TonemapTechnique->IsValid());
	ID3DX11EffectPass *pass=TonemapTechnique->GetPassByIndex(0);
	assert(pass->IsValid());
	hr=pass->GetDesc(&PassDesc);
	V_RETURN(hr);
	hr=m_pd3dDevice->CreateInputLayout( decl, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pBufferVertexDecl);

	float x=-1.f,y=-1.f;
	float width=2.f,height=2.f;
	Vertext vertices[4] =
	{
		D3DXVECTOR4(x		,y			,1,	1), D3DXVECTOR2(0	,1.f),
		D3DXVECTOR4(x+width	,y			,1,	1), D3DXVECTOR2(1.f	,1.f),
		D3DXVECTOR4(x		,y+height	,1,	1), D3DXVECTOR2(0.f	,0	),
		D3DXVECTOR4(x+width	,y+height	,1,	1), D3DXVECTOR2(1.f	,0	),
	};
	D3D11_BUFFER_DESC bdesc=
	{
        4*sizeof(Vertext),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
	};
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(Vertext);
    InitData.SysMemSlicePitch = 0;
	D3D11_SUBRESOURCE_DATA initialData;
	hr=m_pd3dDevice->CreateBuffer(&bdesc,&InitData,&m_pVertexBuffer);
	return hr;
}

ID3D11RenderTargetView* SimulHDRRendererDX11::MakeRenderTarget(const ID3D11Texture2D* pTexture)
{
	ID3D11RenderTargetView* pRenderTargetView;
	HRESULT hr;
	hr=m_pd3dDevice->CreateRenderTargetView((ID3D11Resource*)pTexture,NULL, &pRenderTargetView);
	return pRenderTargetView;
}
	static float depth_start=1.f;

HRESULT SimulHDRRendererDX11::StartRender()
{
	HRESULT hr=S_OK;
	m_pHDRRenderTarget[0]	=NULL;
	m_pBufferDepthSurface	=NULL;
	m_pLDRRenderTarget[0]	=NULL;

	m_pOldRenderTarget[0]	=NULL;
	m_pOldDepthSurface		=NULL;
	PIXBeginNamedEvent(0,"Setup Sky Buffer");
    float clearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
	m_pImmediateContext->OMGetRenderTargets(	1,
										m_pOldRenderTarget,
										&m_pOldDepthSurface
										);
	m_pHDRRenderTarget[0]=MakeRenderTarget(hdr_buffer_texture);
	//hr=m_pd3dDevice->CreateDepthStencilView((ID3D11Resource*)buffer_depth_texture, NULL, &m_pBufferDepthSurface);
	m_pImmediateContext->OMSetRenderTargets(1,m_pHDRRenderTarget,m_pBufferDepthSurface);
	m_pImmediateContext->ClearRenderTargetView(m_pHDRRenderTarget[0],clearColor);
	if(m_pBufferDepthSurface)
		m_pImmediateContext->ClearDepthStencilView(m_pBufferDepthSurface,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, depth_start, 0);

	PIXEndNamedEvent();

	return S_OK;
}


float gaussian( float x, float mean, float std_deviation )
{
    return ( 1.0f / sqrt( 2.0f * D3DX_PI * std_deviation * std_deviation ) ) 
            * expf( (-((x-mean)*(x-mean)))/(2.0f * std_deviation * std_deviation) );
}
HRESULT SimulHDRRendererDX11::ApplyFade()
{
	HRESULT hr=S_OK;
	if(!atmospherics)
		return hr;
	atmospherics->SetInputTextures(hdr_buffer_texture,buffer_depth_texture);
	m_pImmediateContext->OMSetRenderTargets(1,m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pHDRRenderTarget[0])
	m_pHDRRenderTarget[0]=MakeRenderTarget(faded_texture);
	m_pImmediateContext->OMSetRenderTargets(1,m_pHDRRenderTarget,m_pBufferDepthSurface);
	hr=atmospherics->Render();
	return hr;
}

HRESULT SimulHDRRendererDX11::FinishRender()
{
	HRESULT hr=S_OK;
	D3D11_TEXTURE2D_DESC desc;
	PIXBeginNamedEvent(0,"HDR Buffer To Screen");

	// using gamma, render the hdr image to the LDR buffer:
	m_pImmediateContext->OMSetRenderTargets(1,m_pOldRenderTarget,m_pOldDepthSurface);
	//m_pOldRenderTarget->GetDesc(&desc);

	//m_pd3dDevice->ClearDepthStencilView(m_pOldDepthSurface,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1.f,0L);
	TonemapTechnique->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
	Exposure->SetFloat(exposure*exposure_multiplier);
	Gamma->SetFloat(gamma);
	hdrTexture->SetResource(hdr_buffer_texture_SRV);

	RenderBufferToCurrentTarget(true);
	//m_pd3dDevice->ClearDepthStencilView(m_pOldDepthSurface,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1.f,0L);

	SAFE_RELEASE(m_pOldRenderTarget[0])
	SAFE_RELEASE(m_pOldDepthSurface)
	SAFE_RELEASE(m_pLDRRenderTarget[0])
	SAFE_RELEASE(m_pHDRRenderTarget[0])
	SAFE_RELEASE(m_pBufferDepthSurface)
	PIXEndNamedEvent();
	return hr;
}

HRESULT SimulHDRRendererDX11::RenderBufferToCurrentTarget(bool do_tonemap)
{
	HRESULT hr=S_OK;
	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
    D3DXMatrixOrthoLH(&ident,screen_width/(float)BufferWidth,screen_height/(float)BufferHeight,-100.f,100.f);
	worldViewProj->SetMatrix(ident);
	UINT stride = sizeof(Vertext);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,					// the first input slot for binding
												1,					// the number of buffers in the array
												&m_pVertexBuffer,	// the array of vertex buffers
												&stride,			// array of stride values, one for each buffer
												&offset);			// array of offset values, one for each buffer
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pImmediateContext->IASetInputLayout(m_pBufferVertexDecl);
	m_pImmediateContext->Draw(4,0);
	return hr;
}

const char *SimulHDRRendererDX11::GetDebugText() const
{
	static char debug_text[256];
	return debug_text;
}

float SimulHDRRendererDX11::GetTiming() const
{
	return timing;
}