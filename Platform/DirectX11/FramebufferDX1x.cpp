// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Framebuffer.cpp A renderer for skies, clouds and weather effects.

#include "FramebufferDX1x.h"


#include <tchar.h>
#include <dxerr.h>
#include <string>
#include <assert.h>
typedef std::basic_string<TCHAR> tstring;

#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"
#include "Utilities.h"
#include "Simul/Math/Pi.h"
using namespace simul;
using namespace dx11;


Framebuffer::Framebuffer(int w,int h) :
	BaseFramebuffer(w,h)
	,m_pd3dDevice(NULL),
	hdr_buffer_texture(NULL),
	buffer_depth_texture(NULL),
	buffer_texture_SRV(NULL),
	buffer_depth_texture_SRV(NULL),
	m_pHDRRenderTarget(NULL),
	m_pBufferDepthSurface(NULL),
	m_pOldRenderTarget(NULL),
	m_pOldDepthSurface(NULL)
	,stagingTexture(NULL)
	,timing(0.f)
	,target_format(DXGI_FORMAT_R32G32B32A32_FLOAT)
	,depth_format(DXGI_FORMAT_UNKNOWN) //The usual case is for the user to supply depth look-up textures, which is all we need for the majority of cases... So let's avoid needless construction of depth buffers unless otherwise indicated with a SetDepthFormat(...)
	,num_v(0)
	,GenerateMips(false)
{
}

void Framebuffer::SetWidthAndHeight(int w,int h)
{
	if(Width!=w||Height!=h)
	{
		Width=w;
		Height=h;
		if(m_pd3dDevice)
			InvalidateDeviceObjects();
	}
}

void Framebuffer::SetFormat(int f)
{
	DXGI_FORMAT F=(DXGI_FORMAT)f;
	if(F==target_format)
		return;
	target_format=F;
	InvalidateDeviceObjects();
}

void Framebuffer::SetDepthFormat(int f)
{
	DXGI_FORMAT F=(DXGI_FORMAT)f;
	if(F==depth_format)
		return;
	depth_format=F;
	InvalidateDeviceObjects();
}

void Framebuffer::SetGenerateMips(bool m)
{
	GenerateMips=m;
}

void Framebuffer::RestoreDeviceObjects(void *dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	if(!m_pd3dDevice)
		return;
}

void Framebuffer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;

	SAFE_RELEASE(m_pHDRRenderTarget)
	SAFE_RELEASE(m_pBufferDepthSurface)

	SAFE_RELEASE(hdr_buffer_texture);
	SAFE_RELEASE(buffer_texture_SRV);

	SAFE_RELEASE(buffer_depth_texture);
	SAFE_RELEASE(buffer_depth_texture_SRV);

	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
	SAFE_RELEASE(stagingTexture);
}

bool Framebuffer::Destroy()
{
	InvalidateDeviceObjects();
	return true;
}

Framebuffer::~Framebuffer()
{
	InvalidateDeviceObjects();
}


bool Framebuffer::IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat)
{
	DepthFormat;
	AdapterFormat;
	BackBufferFormat;
	HRESULT hr=S_OK;
	/*LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);
*/
    return (hr==S_OK);
}
struct Vertext
{
	D3DXVECTOR4 pos;
	D3DXVECTOR2 tex;
};

bool Framebuffer::CreateBuffers()
{
	if(!Width||!Height)
		return false;
	if(!m_pd3dDevice)
		return false;
	HRESULT hr=S_OK;
	SAFE_RELEASE(hdr_buffer_texture);
	SAFE_RELEASE(m_pHDRRenderTarget)
	SAFE_RELEASE(buffer_texture_SRV);
	SAFE_RELEASE(buffer_depth_texture);
	SAFE_RELEASE(m_pBufferDepthSurface)
	SAFE_RELEASE(buffer_depth_texture_SRV);
	SAFE_RELEASE(stagingTexture);
	D3D11_TEXTURE2D_DESC desc=
	{
		Width,
		Height,
		1,
		GenerateMips?0:1,
		target_format,
		{1,0},
		D3D1x_USAGE_DEFAULT,
		D3D1x_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
		0,
		GenerateMips?D3D11_RESOURCE_MISC_GENERATE_MIPS:0
	};
	int quality=0;
	if(target_format!=0)
	{
		unsigned int numQualityLevels=0;
		HRESULT hr=m_pd3dDevice->CheckMultisampleQualityLevels(
				target_format,
				numAntialiasingSamples,
				&numQualityLevels	);
		quality=numQualityLevels-1;
		desc.SampleDesc.Count	=numAntialiasingSamples;
		desc.SampleDesc.Quality	=quality;//numQualityLevels-1;

		V_CHECK(m_pd3dDevice->CreateTexture2D(	&desc,
												NULL,
												&hdr_buffer_texture	))
												
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format				=target_format;
		renderTargetViewDesc.ViewDimension		=numAntialiasingSamples>1?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice	=0;
		// Create the render target in DX11:
		hr=m_pd3dDevice->CreateRenderTargetView(hdr_buffer_texture,&renderTargetViewDesc, &m_pHDRRenderTarget);

		V_CHECK(m_pd3dDevice->CreateShaderResourceView(hdr_buffer_texture, NULL, &buffer_texture_SRV ));
	}
	DXGI_FORMAT fmtDepthTex = depth_format;
	DXGI_FORMAT possibles[]={
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
		DXGI_FORMAT_D32_FLOAT,
		DXGI_FORMAT_D16_UNORM,
		DXGI_FORMAT_UNKNOWN};
	// Try creating a depth texture
	desc.Width = Width;
	desc.Height = Height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.SampleDesc.Count = numAntialiasingSamples;
	desc.SampleDesc.Quality = quality;
	desc.Usage = D3D1x_USAGE_DEFAULT;
	desc.BindFlags = D3D1x_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	if(fmtDepthTex!=DXGI_FORMAT_UNKNOWN)
	{
		V_CHECK(m_pd3dDevice->CreateTexture2D(	&desc,
												NULL,
												&buffer_depth_texture))
	}
	if(buffer_depth_texture)
	{
		unsigned int numQualityLevels=0;
		HRESULT hr=m_pd3dDevice->CheckMultisampleQualityLevels(
				DXGI_FORMAT_D32_FLOAT,
				numAntialiasingSamples,
				&numQualityLevels	);
		D3D11_TEX2D_DSV dsv;
		dsv.MipSlice=0;
		D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc;
		depthDesc.ViewDimension		=numAntialiasingSamples>0?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
		depthDesc.Format			=DXGI_FORMAT_D32_FLOAT;
		depthDesc.Flags				=0;
		depthDesc.Texture2D			=dsv;
		hr=m_pd3dDevice->CreateDepthStencilView((ID3D1xResource*)buffer_depth_texture,&depthDesc, &m_pBufferDepthSurface);

		D3D11_SHADER_RESOURCE_VIEW_DESC depthSrvDesc;
		depthSrvDesc.Format			=DXGI_FORMAT_R32_FLOAT;
		depthSrvDesc.ViewDimension	=numAntialiasingSamples>0?D3D_SRV_DIMENSION_TEXTURE2DMS:D3D_SRV_DIMENSION_TEXTURE2D;
		depthSrvDesc.Texture2D.MipLevels=1;
		depthSrvDesc.Texture2D.MostDetailedMip=0;

		V_CHECK(m_pd3dDevice->CreateShaderResourceView(buffer_depth_texture,&depthSrvDesc, &buffer_depth_texture_SRV ));
	}
	return (hr==S_OK);
}

ID3D1xRenderTargetView* Framebuffer::MakeRenderTarget(const ID3D1xTexture2D* pTexture)
{
	ID3D1xRenderTargetView* pRenderTargetView;
	HRESULT hr;
	// Setup the description of the render target view.
	D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	renderTargetViewDesc.Format				=target_format;
	renderTargetViewDesc.ViewDimension		=numAntialiasingSamples>1?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
	renderTargetViewDesc.Texture2D.MipSlice	=0;
	// Create the render target in DX11:
	hr=m_pd3dDevice->CreateRenderTargetView((ID3D1xResource*)pTexture,&renderTargetViewDesc, &pRenderTargetView);
	return pRenderTargetView;
}

ID3D11Texture2D* makeStagingTexture(ID3D1xDevice *m_pd3dDevice
							,int w,int h,DXGI_FORMAT target_format)
{
	ID3D11Texture2D*	tex;
	D3D11_TEXTURE2D_DESC textureDesc=
	{
		w,h,
		1,
		1,
		target_format,
		{1,0}
		,D3D11_USAGE_STAGING,
		0,
		D3D11_CPU_ACCESS_READ| D3D11_CPU_ACCESS_WRITE,
		0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	m_pd3dDevice->CreateTexture2D(&textureDesc,NULL,&tex);
	return tex;
}

void Framebuffer::CopyToMemory(void *context,void *target,int start_texel,int texels)
{
	ID3D11DeviceContext *pContext=NULL;
	if(texels==0)
		texels=Width*Height;
m_pd3dDevice->GetImmediateContext(&pContext);

	if(!stagingTexture)
		stagingTexture=makeStagingTexture(m_pd3dDevice,Width,Height,target_format);
	D3D11_BOX sourceRegion;
	sourceRegion.left = 0;
	sourceRegion.right = Width;
	sourceRegion.top = 0;
	sourceRegion.bottom = Height;
	sourceRegion.front = 0;
	sourceRegion.back = 1;
	pContext->CopySubresourceRegion(stagingTexture,0,0,0,0,GetColorTexture(),0,&sourceRegion);
	HRESULT hr=S_OK;
	D3D11_MAPPED_SUBRESOURCE msr;
	V_CHECK(pContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &msr));
	int byteSize=simul::dx11::ByteSizeOfFormatElement(target_format);
	int required_pitch=Width*byteSize;
	char *dst=(char*)target;
	if(msr.RowPitch==required_pitch)
	{
		const char *dat=(const char *)msr.pData;
		dat+=start_texel*byteSize;
		//dst+=start_texel*byteSize;
		memcpy(dst,dat,texels*byteSize);
	}
	else
	{
		char *src=(char*)msr.pData;
		int h0=start_texel/Width;
		int h1=h0+texels/Width;
		src+=msr.RowPitch*h0;
		//dst+=byteSize*Width*h0;
		for(int i=h0;i<h1;i++)
		{
			memcpy(dst,src,required_pitch);
			dst+=required_pitch;
			src+=msr.RowPitch;
		}
	}
	// copy data
	pContext->Unmap(stagingTexture, 0);
	SAFE_RELEASE(pContext)
}

void Framebuffer::ActivateColour(void *context,const float viewportXYWH[4])
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!pContext)
		return;
	SaveOldRTs(context);
	if(!hdr_buffer_texture&&!buffer_depth_texture)
		CreateBuffers();
	if(m_pHDRRenderTarget)
		pContext->OMSetRenderTargets(1,&m_pHDRRenderTarget,NULL);
	else 
		pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,NULL);
	SetViewport(context,viewportXYWH[0],viewportXYWH[1],viewportXYWH[2],viewportXYWH[3],0,1.f);

}

void Framebuffer::ActivateViewport(void *context, float viewportX, float viewportY, float viewportW, float viewportH)
{
	Activate(context);
	SetViewport(context,viewportX,viewportY,viewportW,viewportH,0,1.f);
}

void Framebuffer::SaveOldRTs(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v>0)
		pContext->RSGetViewports(&num_v,m_OldViewports);
	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	pContext->OMGetRenderTargets(	1,
									&m_pOldRenderTarget,
									&m_pOldDepthSurface
									);
}

void Framebuffer::Activate(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!pContext)
		return;
	SaveOldRTs(context);
	if(!hdr_buffer_texture&&!buffer_depth_texture)
		CreateBuffers();
	if(m_pHDRRenderTarget)
		pContext->OMSetRenderTargets(1,&m_pHDRRenderTarget,m_pBufferDepthSurface);
	else 
		pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pBufferDepthSurface);
	SetViewport(context,0,0,1.f,1.f,0,1.f);
}

void Framebuffer::SetViewport(void *context,float X,float Y,float W,float H,float Z,float D)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!pContext)
		return;
	D3D11_VIEWPORT viewport;
	viewport.Width = floorf((float)Width*W + 0.5f);
	viewport.Height = floorf((float)Height*H + 0.5f);
	viewport.TopLeftX = floorf((float)Width*X + 0.5f);
	viewport.TopLeftY = floorf((float)Height*Y+ 0.5f);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	pContext->RSSetViewports(1, &viewport);
}

void Framebuffer::ActivateDepth(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!pContext)
		return;
	if(!hdr_buffer_texture&&!buffer_depth_texture)
		CreateBuffers();
	HRESULT hr=S_OK;
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v>0)
		pContext->RSGetViewports(&num_v,m_OldViewports);

	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	pContext->OMGetRenderTargets(	1,
												&m_pOldRenderTarget,
												&m_pOldDepthSurface
												);
	pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pBufferDepthSurface);
	D3D11_VIEWPORT viewport;
	// Setup the viewport for rendering.
	viewport.Width = (float) Width;
	viewport.Height = (float)Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.f;
	viewport.TopLeftY = 0.f;

	// Create the viewport.
	pContext->RSSetViewports(1, &viewport);
}

void Framebuffer::ActivateColour(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!pContext)
		return;
	if(!hdr_buffer_texture&&!buffer_depth_texture)
		CreateBuffers();
	if(!m_pHDRRenderTarget)
		return;
	SaveOldRTs(context);
	pContext->OMSetRenderTargets(1,&m_pHDRRenderTarget,NULL);
	SetViewport(context,0,0,1.f,1.f,0,1.f);
}

void Framebuffer::Deactivate(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	if(num_v>0)
		pContext->RSSetViewports(num_v,m_OldViewports);
	if(GenerateMips)
		pContext->GenerateMips(buffer_texture_SRV);
}

void Framebuffer::DeactivateDepth(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!m_pHDRRenderTarget)
	{
		Deactivate(context);
		return;
	}
	pContext->OMSetRenderTargets(1,&m_pHDRRenderTarget,NULL);
}

void Framebuffer::Clear(void *context,float r,float g,float b,float a,float depth,int mask)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	// Clear the screen to black:
    float clearColor[4]={r,g,b,a};
    if(!mask)
		mask=D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL;
	if(m_pHDRRenderTarget)
		pContext->ClearRenderTargetView(m_pHDRRenderTarget,clearColor);
	if(m_pBufferDepthSurface)
		pContext->ClearDepthStencilView(m_pBufferDepthSurface,mask,depth,0);
}

void Framebuffer::ClearColour(void *context,float r,float g,float b,float a)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	float clearColor[4]={r,g,b,a};
	if(m_pHDRRenderTarget)
		pContext->ClearRenderTargetView(m_pHDRRenderTarget,clearColor);
}

bool Framebuffer::DrawQuad(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	return true;
}

void Framebuffer::GetTextureDimensions(const void* tex, unsigned int& widthOut, unsigned int& heightOut) const
{
	ID3D11Resource* pTexResource;
	const_cast<ID3D11ShaderResourceView*>( reinterpret_cast<const ID3D11ShaderResourceView*>(tex) )->GetResource(&pTexResource); //GetResource increments the resources ref.count so we need to Release when done.
	ID3D11Texture2D* pD3DDepthTex = static_cast<ID3D11Texture2D*>(pTexResource);
	D3D11_TEXTURE2D_DESC depthTexDesc;
	pD3DDepthTex->GetDesc(&depthTexDesc);
	widthOut = depthTexDesc.Width;
	heightOut = depthTexDesc.Height;
	pTexResource->Release();
}
