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
	,num_OldViewports(0) //The usual case is for the user to supply depth look-up textures, which is all we need for the majority of cases... So let's avoid needless construction of depth buffers unless otherwise indicated with a SetDepthFormat(...)
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
	is_cubemap=false;
}

void Framebuffer::SetAsCubemap(int w)
{
	SetWidthAndHeight(w,w);
	is_cubemap=true;
}

void Framebuffer::SetCubeFace(int f)
{
	current_face=f;
}

void Framebuffer::SetFormat(crossplatform::PixelFormat f)
{
	if(f==target_format)
		return;
	target_format=f;
	InvalidateDeviceObjects();
}

void Framebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
	if(f==depth_format)
		return;
	depth_format=f;
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
	CreateBuffers();
}

void Framebuffer::InvalidateDeviceObjects()
{
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();

	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
	sphericalHarmonicsConstants.InvalidateDeviceObjects();
	SAFE_DELETE(buffer_texture);
	SAFE_DELETE(buffer_depth_texture);
	sphericalHarmonics.InvalidateDeviceObjects();
	SAFE_DELETE(sphericalHarmonicsEffect);
	sphericalSamples.InvalidateDeviceObjects();
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
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
	if(!buffer_texture)
		buffer_texture=renderPlatform->CreateTexture();
	if(!buffer_depth_texture)
		buffer_depth_texture=renderPlatform->CreateTexture();
	static int quality=0;
	if(target_format!=crossplatform::UNKNOWN)
	{
		if(!is_cubemap)
			buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,target_format,false,true,false,numAntialiasingSamples,quality);
		else
			buffer_texture->ensureTextureArraySizeAndFormat(renderPlatform,Width,Height,6,target_format,false,true,true);
	}
	if(depth_format!=crossplatform::UNKNOWN)
	{
		buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,depth_format,false,false,true,numAntialiasingSamples,quality);
	}
	// The table of coefficients.
	int s=(bands+1);
	if(s<4)
		s=4;
	sphericalHarmonics.InvalidateDeviceObjects();
	sphericalSamples.InvalidateDeviceObjects();
	sphericalHarmonicsConstants.RestoreDeviceObjects(renderPlatform);
	return true;
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

void Framebuffer::ActivateColour(crossplatform::DeviceContext &deviceContext,const float viewportXYWH[4])
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	SaveOldRTs(deviceContext);
	if((!buffer_texture||!buffer_texture->AsD3D11Texture2D())&&(!buffer_depth_texture||!buffer_depth_texture->AsD3D11Texture2D()))
		CreateBuffers();
	if(buffer_texture->AsD3D11RenderTargetView())
	{
		colour_active=true;
		ID3D11RenderTargetView *renderTargetView=buffer_texture->AsD3D11RenderTargetView();
		dx11::Texture *t=(dx11::Texture *)buffer_texture;
		if(is_cubemap)
			renderTargetView=t->ArrayD3D11RenderTargetView(current_face);
		pContext->OMSetRenderTargets(1,&renderTargetView,NULL);
	}
	else 
	{
		pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,NULL);
	}
	SetViewport(deviceContext,viewportXYWH[0],viewportXYWH[1],viewportXYWH[2],viewportXYWH[3],0,1.f);
}

void Framebuffer::SaveOldRTs(crossplatform::DeviceContext &deviceContext)
{
	deviceContext.asD3D11DeviceContext()->RSGetViewports(&num_OldViewports,NULL);
	if(num_OldViewports>0)
		deviceContext.asD3D11DeviceContext()->RSGetViewports(&num_OldViewports,m_OldViewports);
	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	deviceContext.asD3D11DeviceContext()->OMGetRenderTargets(	1,
																&m_pOldRenderTarget,
																&m_pOldDepthSurface
																);
}

bool Framebuffer::IsValid() const
{
	bool ok=(buffer_texture!=NULL)||(buffer_depth_texture!=NULL);
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

void Framebuffer::ActivateViewport(crossplatform::DeviceContext &deviceContext, float viewportX, float viewportY, float viewportW, float viewportH)
{
	Activate(deviceContext);
	SetViewport(deviceContext,viewportX,viewportY,viewportW,viewportH,0,1.f);
}

void Framebuffer::Activate(crossplatform::DeviceContext &deviceContext)
{
	SaveOldRTs(deviceContext);
	if((!buffer_texture||!buffer_texture->AsD3D11Texture2D())&&(!buffer_depth_texture||!buffer_depth_texture->AsD3D11Texture2D()))
		CreateBuffers();
	SIMUL_ASSERT(IsValid());
	ID3D11RenderTargetView *renderTargetView=NULL;
	dx11::Texture *t=(dx11::Texture *)buffer_texture;
	if(is_cubemap)
		renderTargetView=t->ArrayD3D11RenderTargetView(current_face);
	else
		renderTargetView=buffer_texture->AsD3D11RenderTargetView();
	if(renderTargetView)
	{
		colour_active=true;
		depth_active=(buffer_depth_texture->AsD3D11DepthStencilView()!=NULL);
		deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1,&renderTargetView,buffer_depth_texture->AsD3D11DepthStencilView());
	}
	else 
	{
		depth_active=(buffer_depth_texture->AsD3D11DepthStencilView()!=NULL);
		deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1,&m_pOldRenderTarget,buffer_depth_texture->AsD3D11DepthStencilView());
	}
	SetViewport(deviceContext,0,0,1.f,1.f,0,1.f);
}

void Framebuffer::SetViewport(crossplatform::DeviceContext &deviceContext,float X,float Y,float W,float H,float Z,float D)
{
	D3D11_VIEWPORT viewport;
	viewport.Width = floorf((float)Width*W + 0.5f);
	viewport.Height = floorf((float)Height*H + 0.5f);
	viewport.TopLeftX = floorf((float)Width*X + 0.5f);
	viewport.TopLeftY = floorf((float)Height*Y+ 0.5f);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	deviceContext.asD3D11DeviceContext()->RSSetViewports(1, &viewport);
}

void Framebuffer::ActivateDepth(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	if((!buffer_texture||!buffer_texture->AsD3D11Texture2D())&&(!buffer_depth_texture||!buffer_depth_texture->AsD3D11Texture2D()))
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
	if((!buffer_texture||!buffer_texture->AsD3D11Texture2D())&&(!buffer_depth_texture||!buffer_depth_texture->AsD3D11Texture2D()))
		CreateBuffers();
	if(!buffer_texture->AsD3D11RenderTargetView())
		return;
	SaveOldRTs(deviceContext);
	ID3D11RenderTargetView *renderTargetView=buffer_texture->AsD3D11RenderTargetView();
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1,&renderTargetView,NULL);
	colour_active=true;
	SetViewport(deviceContext,0,0,1.f,1.f,0,1.f);
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
		dx11::Texture *t=(dx11::Texture *)buffer_texture;
	if(is_cubemap)
		renderTargetView=t->ArrayD3D11RenderTargetView(current_face);
	pContext->OMSetRenderTargets(1,&renderTargetView,NULL);
	depth_active=false;
}

void Framebuffer::Clear(crossplatform::DeviceContext &deviceContext,float r,float g,float b,float a,float depth,int mask)
{
	ID3D11DeviceContext *pContext = deviceContext.asD3D11DeviceContext();
	// Clear the screen to black:
    float clearColor[4]={r,g,b,a};
    if(!mask)
		mask=D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL;
	if(is_cubemap)
	{
		dx11::Texture *t=(dx11::Texture *)buffer_texture;
		if(current_face>=0)
		{
			pContext->ClearRenderTargetView(t->ArrayD3D11RenderTargetView(current_face),clearColor);
		}
		else
		{
			for(int i=0;i<6;i++)
			{
				if(t->ArrayD3D11RenderTargetView(i))
					pContext->ClearRenderTargetView(t->ArrayD3D11RenderTargetView(i),clearColor);
			}
		}
	}
	else
	{
		if(buffer_texture&&buffer_texture->AsD3D11RenderTargetView())
			pContext->ClearRenderTargetView(buffer_texture->AsD3D11RenderTargetView(),clearColor);
	}
	if(buffer_depth_texture&&buffer_depth_texture->AsD3D11DepthStencilView())
		pContext->ClearDepthStencilView(buffer_depth_texture->AsD3D11DepthStencilView(),mask,depth,0);
}

void Framebuffer::ClearDepth(crossplatform::DeviceContext &context,float depth)
{
	if(buffer_depth_texture->AsD3D11DepthStencilView())
		context.asD3D11DeviceContext()->ClearDepthStencilView(buffer_depth_texture->AsD3D11DepthStencilView(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,depth,0);
}

void Framebuffer::ClearColour(crossplatform::DeviceContext &deviceContext,float r,float g,float b,float a)
{
	float clearColor[4]={r,g,b,a};
	if(is_cubemap)
	{
		dx11::Texture *t=(dx11::Texture *)buffer_texture;
		for(int i=0;i<6;i++)
		{
			deviceContext.asD3D11DeviceContext()->ClearRenderTargetView(t->ArrayD3D11RenderTargetView(i), clearColor);
		}
	}
	else if(buffer_texture->AsD3D11RenderTargetView())
		deviceContext.asD3D11DeviceContext()->ClearRenderTargetView(buffer_texture->AsD3D11RenderTargetView(),clearColor);
}

void Framebuffer::RecompileShaders()
{
	BaseFramebuffer::RecompileShaders();
	SAFE_DELETE(sphericalHarmonicsEffect);
	if(!renderPlatform)
		return;
	sphericalHarmonicsEffect=renderPlatform->CreateEffect("spherical_harmonics");
	sphericalHarmonicsConstants.LinkToEffect(sphericalHarmonicsEffect,"SphericalHarmonicsConstants");
}

void Framebuffer::CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!sphericalHarmonicsEffect)
		RecompileShaders();
	int num_coefficients=bands*bands;
	static int BLOCK_SIZE=4;
	static int sqrt_jitter_samples					=4;
	if(!sphericalHarmonics.count)
	{
		sphericalHarmonics.RestoreDeviceObjects(renderPlatform,num_coefficients,true);
		sphericalSamples.RestoreDeviceObjects(renderPlatform,sqrt_jitter_samples*sqrt_jitter_samples,true);
	}
	sphericalHarmonicsConstants.num_bands			=bands;
	sphericalHarmonicsConstants.sqrtJitterSamples	=sqrt_jitter_samples;
	sphericalHarmonicsConstants.numJitterSamples	=sqrt_jitter_samples*sqrt_jitter_samples;
	sphericalHarmonicsConstants.invNumJitterSamples	=1.0f/(float)sphericalHarmonicsConstants.numJitterSamples;
	sphericalHarmonicsConstants.Apply(deviceContext);
	simul::dx11::setUnorderedAccessView	(sphericalHarmonicsEffect->asD3DX11Effect(),"targetBuffer"	,sphericalHarmonics.AsD3D11UnorderedAccessView());
	crossplatform::EffectTechnique *clear		=sphericalHarmonicsEffect->GetTechniqueByName("clear");
	sphericalHarmonicsEffect->Apply(deviceContext,clear,0);
	pContext->Dispatch((num_coefficients+BLOCK_SIZE-1)/BLOCK_SIZE,1,1);
	sphericalHarmonicsEffect->Unapply(deviceContext);
	{
		// The table of 3D directional sample positions. sqrt_jitter_samples x sqrt_jitter_samples
		// We just fill this buffer_texture with random 3d directions.
		crossplatform::EffectTechnique *jitter=sphericalHarmonicsEffect->GetTechniqueByName("jitter");
		simul::dx11::setUnorderedAccessView(sphericalHarmonicsEffect->asD3DX11Effect(),"samplesBufferRW",sphericalSamples.AsD3D11UnorderedAccessView());
		sphericalHarmonicsEffect->Apply(deviceContext,jitter,0);
		pContext->Dispatch((sqrt_jitter_samples+BLOCK_SIZE-1)/BLOCK_SIZE,(sqrt_jitter_samples+BLOCK_SIZE-1)/BLOCK_SIZE,1);
		simul::dx11::setUnorderedAccessView(sphericalHarmonicsEffect->asD3DX11Effect(),"samplesBufferRW",NULL);
		sphericalHarmonicsEffect->Unapply(deviceContext);
	}

	crossplatform::EffectTechnique *tech	=sphericalHarmonicsEffect->GetTechniqueByName("encode");
	simul::dx11::setTexture				(sphericalHarmonicsEffect->asD3DX11Effect(),"cubemapTexture"	,buffer_texture->AsD3D11ShaderResourceView());
	simul::dx11::setTexture				(sphericalHarmonicsEffect->asD3DX11Effect(),"samplesBuffer"		,sphericalSamples.AsD3D11ShaderResourceView());
	
	static bool sh_by_samples=false;
	sphericalHarmonicsEffect->Apply(deviceContext,tech,0);
	pContext->Dispatch(((sh_by_samples?sphericalHarmonicsConstants.numJitterSamples:num_coefficients)+BLOCK_SIZE-1)/BLOCK_SIZE,1,1);
	simul::dx11::setTexture				(sphericalHarmonicsEffect->asD3DX11Effect(),"cubemapTexture"	,NULL);
	simul::dx11::setUnorderedAccessView	(sphericalHarmonicsEffect->asD3DX11Effect(),"targetBuffer"	,NULL);
	simul::dx11::setTexture				(sphericalHarmonicsEffect->asD3DX11Effect(),"samplesBuffer"	,NULL);
	sphericalHarmonicsConstants.Unbind(deviceContext);
	sphericalHarmonicsEffect->Unapply(deviceContext);
}