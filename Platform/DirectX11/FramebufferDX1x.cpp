// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Framebuffer.cpp A renderer for skies, clouds and weather effects.
#define NOMINMAX
#include "FramebufferDX1x.h"

#include <tchar.h>
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#endif
#include <string>
#include <assert.h>

#include "Simul/Sky/Float4.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"
#include "Utilities.h"
#include "Simul/Math/Pi.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"

using namespace simul;
using namespace dx11;

// First figure out sizes, for placement create
UINT64 iCurrentESRAMOffset = 0; // We allow this to grow beyond ESRAM_SIZE
const UINT64 ESRAM_SIZE = 32 * 1024 * 1024;

Framebuffer::Framebuffer(int w,int h) :
	BaseFramebuffer(w,h)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
	,num_OldViewports(0)
	,buffer_texture(NULL)
	,buffer_depth_texture(NULL)
	,stagingTexture(NULL)
	,timing(0.f)
	,target_format(DXGI_FORMAT_R32G32B32A32_FLOAT)
	,depth_format(DXGI_FORMAT_UNKNOWN) //The usual case is for the user to supply depth look-up textures, which is all we need for the majority of cases... So let's avoid needless construction of depth buffers unless otherwise indicated with a SetDepthFormat(...)
	,GenerateMips(false)
	,useESRAM(false)
	,useESRAMforDepth(false)
{
}

void Framebuffer::SetWidthAndHeight(int w,int h)
{
	if(Width!=w||Height!=h)
	{
		Width=w;
		Height=h;
		if(renderPlatform)
			InvalidateDeviceObjects();
	}
}

void Framebuffer::SetFormat(crossplatform::PixelFormat f)
{
	DXGI_FORMAT F=dx11::RenderPlatform::ToDxgiFormat(f);
	if(F==target_format)
		return;
	target_format=F;
	InvalidateDeviceObjects();
}

void Framebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
	DXGI_FORMAT F=dx11::RenderPlatform::ToDxgiFormat(f);
	if(F==depth_format)
		return;
	depth_format=F;
	InvalidateDeviceObjects();
}

void Framebuffer::SetGenerateMips(bool m)
{
	GenerateMips=m;
}

void Framebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	SAFE_DELETE(buffer_depth_texture);
	SAFE_DELETE(buffer_texture);
	if(renderPlatform)
	{
		buffer_texture=renderPlatform->CreateTexture(useESRAM?"ESRAM":NULL);
		buffer_depth_texture=renderPlatform->CreateTexture(useESRAMforDepth?"ESRAM":NULL);
	}
}

void Framebuffer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();

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
	SAFE_DELETE(buffer_depth_texture);
	SAFE_DELETE(buffer_texture);
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
	if(!renderPlatform)
	{
		SIMUL_BREAK("renderPlatform should not be NULL here");
	}
	if(!renderPlatform)
		return false;
	if((buffer_texture&&buffer_texture->AsD3D11Texture2D()))
		return true;
	if(buffer_depth_texture&&buffer_depth_texture->AsD3D11Texture2D())
		return true;
		
	HRESULT hr=S_OK;
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
	if(!buffer_texture)
		buffer_texture=renderPlatform->CreateTexture();
	if(!buffer_depth_texture)
		buffer_depth_texture=renderPlatform->CreateTexture();
	SAFE_RELEASE(stagingTexture);
	static int quality=0;
	if(target_format!=0)
	{
		buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,dx11::RenderPlatform::FromDxgiFormat(target_format),false,true,false,numAntialiasingSamples,quality);
	
												
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format				=target_format;
		renderTargetViewDesc.ViewDimension		=numAntialiasingSamples>1?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice	=0;
	}
	//DXGI_FORMAT fmtDepthTex = depth_format;
	/*DXGI_FORMAT possibles[]={
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
		DXGI_FORMAT_D32_FLOAT,
		DXGI_FORMAT_D16_UNORM,
		DXGI_FORMAT_UNKNOWN};*/
	// Try creating a depth texture
	if(depth_format!=DXGI_FORMAT_UNKNOWN)
	{
		buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,dx11::RenderPlatform::FromDxgiFormat(depth_format),false,false,true,numAntialiasingSamples,quality);
	}
	return true;
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
	hr=renderPlatform->AsD3D11Device()->CreateRenderTargetView((ID3D1xResource*)pTexture,&renderTargetViewDesc, &pRenderTargetView);
	return pRenderTargetView;
}

ID3D11Texture2D* makeStagingTexture(ID3D11Device *m_pd3dDevice
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
	renderPlatform->AsD3D11Device()->GetImmediateContext(&pContext);

	if(!stagingTexture)
		stagingTexture=makeStagingTexture(renderPlatform->AsD3D11Device(),Width,Height,target_format);
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

void Framebuffer::ActivateColour(crossplatform::DeviceContext &deviceContext,const float viewportXYWH[4])
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	SaveOldRTs(pContext);
	if((!buffer_texture||!buffer_texture->AsD3D11Texture2D())&&(!buffer_depth_texture||!buffer_depth_texture->AsD3D11Texture2D()))
		CreateBuffers();
	if(buffer_texture->AsD3D11RenderTargetView())
	{
		colour_active=true;
		ID3D11RenderTargetView *renderTargetView=buffer_texture->AsD3D11RenderTargetView();
		pContext->OMSetRenderTargets(1,&renderTargetView,NULL);
	}
	else 
	{
		pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,NULL);
	}
	SetViewport(pContext,viewportXYWH[0],viewportXYWH[1],viewportXYWH[2],viewportXYWH[3],0,1.f);
}

void Framebuffer::ActivateViewport(crossplatform::DeviceContext &deviceContext, float viewportX, float viewportY, float viewportW, float viewportH)
{
	Activate(deviceContext);
	SetViewport(deviceContext.asD3D11DeviceContext(),viewportX,viewportY,viewportW,viewportH,0,1.f);
}

void Framebuffer::SaveOldRTs(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->RSGetViewports(&num_OldViewports,NULL);
	if(num_OldViewports>0)
		pContext->RSGetViewports(&num_OldViewports,m_OldViewports);
	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	pContext->OMGetRenderTargets(	1,
									&m_pOldRenderTarget,
									&m_pOldDepthSurface
									);
}

bool Framebuffer::IsValid() const
{
	bool ok=(buffer_texture->AsD3D11RenderTargetView()!=NULL)||(buffer_depth_texture->AsD3D11DepthStencilView()!=NULL);
	return ok;
}

void Framebuffer::MoveToFastRAM()
{
	if(useESRAM)
		buffer_texture->MoveToFastRAM();
	if(useESRAMforDepth)
		buffer_depth_texture->MoveToFastRAM();
}

void Framebuffer::MoveToSlowRAM()
{
	if(useESRAM)
		buffer_texture->MoveToSlowRAM();
	if(useESRAMforDepth)
		buffer_depth_texture->MoveToSlowRAM();
}

void Framebuffer::MoveDepthToSlowRAM()
{
	if(useESRAMforDepth)
		buffer_depth_texture->MoveToSlowRAM();
}
void Framebuffer::Activate(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	SaveOldRTs(pContext);
	CreateBuffers();
	SIMUL_ASSERT(IsValid());
	ID3D11RenderTargetView *renderTargetView=buffer_texture->AsD3D11RenderTargetView();
	if(renderTargetView)
	{
		colour_active=true;
		depth_active=(buffer_depth_texture->AsD3D11DepthStencilView()!=NULL);
		pContext->OMSetRenderTargets(1,&renderTargetView,buffer_depth_texture->AsD3D11DepthStencilView());
	}
	else 
	{
		depth_active=(buffer_depth_texture->AsD3D11DepthStencilView()!=NULL);
		pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,buffer_depth_texture->AsD3D11DepthStencilView());
	}
	SetViewport(pContext,0,0,1.f,1.f,0,1.f);
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

void Framebuffer::ActivateDepth(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	CreateBuffers();
	HRESULT hr=S_OK;
	
	ID3D11RenderTargetView *renderTargetView=buffer_texture->AsD3D11RenderTargetView();
	if(m_pOldRenderTarget==NULL&&m_pOldDepthSurface==NULL)
	{
		pContext->RSGetViewports(&num_OldViewports,NULL);
		if(num_OldViewports>0)
			pContext->RSGetViewports(&num_OldViewports,m_OldViewports);
		pContext->OMGetRenderTargets(	1,
										&m_pOldRenderTarget,
										&m_pOldDepthSurface
										);
		pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,buffer_depth_texture->AsD3D11DepthStencilView());
	}
	else
	{
		pContext->OMSetRenderTargets(1,&renderTargetView,buffer_depth_texture->AsD3D11DepthStencilView());
	}
	depth_active=(buffer_depth_texture->AsD3D11DepthStencilView()!=NULL);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width = (float)Width;
	viewport.Height = (float)Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.f;
	viewport.TopLeftY = 0.f;

	// Create the viewport.
	pContext->RSSetViewports(1, &viewport);
}

void Framebuffer::ActivateColour(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	CreateBuffers();
	if(!buffer_texture->AsD3D11RenderTargetView())
		return;
	SaveOldRTs(pContext);
	ID3D11RenderTargetView *renderTargetView=buffer_texture->AsD3D11RenderTargetView();
	pContext->OMSetRenderTargets(1,&renderTargetView,NULL);
	colour_active=true;
	SetViewport(pContext,0,0,1.f,1.f,0,1.f);
}

void Framebuffer::Deactivate(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	if(num_OldViewports>0)
		pContext->RSSetViewports(num_OldViewports,m_OldViewports);
	if(GenerateMips)
		pContext->GenerateMips(buffer_texture->AsD3D11ShaderResourceView());
	colour_active=false;
	depth_active=false;
}

void Framebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!buffer_texture->AsD3D11RenderTargetView())
	{
		Deactivate(deviceContext);
		return;
	}
	ID3D11RenderTargetView *renderTargetView=buffer_texture->AsD3D11RenderTargetView();
	pContext->OMSetRenderTargets(1,&renderTargetView,NULL);
	depth_active=false;
}

void Framebuffer::Clear(void *context,float r,float g,float b,float a,float depth,int mask)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	// Clear the screen to black:
    float clearColor[4]={r,g,b,a};
    if(!mask)
		mask=D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL;
	if(buffer_texture->AsD3D11RenderTargetView())
		pContext->ClearRenderTargetView(buffer_texture->AsD3D11RenderTargetView(),clearColor);
	if(buffer_depth_texture->AsD3D11DepthStencilView())
		pContext->ClearDepthStencilView(buffer_depth_texture->AsD3D11DepthStencilView(),mask,depth,0);
}

void Framebuffer::ClearDepth(void *context,float depth)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(buffer_depth_texture->AsD3D11DepthStencilView())
		pContext->ClearDepthStencilView(buffer_depth_texture->AsD3D11DepthStencilView(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,depth,0);
}

void Framebuffer::ClearColour(void *context,float r,float g,float b,float a)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	float clearColor[4]={r,g,b,a};
	if(buffer_texture->AsD3D11RenderTargetView())
		pContext->ClearRenderTargetView(buffer_texture->AsD3D11RenderTargetView(),clearColor);
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
