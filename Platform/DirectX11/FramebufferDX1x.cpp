// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// FramebufferDX1x.cpp A renderer for skies, clouds and weather effects.

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


#define BLUR_SIZE 9

#include "Simul/Math/Pi.h"

FramebufferDX1x::FramebufferDX1x(int w,int h) :
	BaseFramebuffer(w,h)
	,m_pd3dDevice(NULL),
	m_pImmediateContext(NULL),
	m_pBufferVertexDecl(NULL),
	m_pVertexBuffer(NULL),
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
	,num_v(0)
	,target_format(DXGI_FORMAT_R32G32B32A32_FLOAT)
{
}

void FramebufferDX1x::SetWidthAndHeight(int w,int h)
{
	if(Width!=w||Height!=h)
	{
		Width=w;
		Height=h;
		InvalidateDeviceObjects();
	}
}

void FramebufferDX1x::SetFormat(int f)
{
	DXGI_FORMAT F=(DXGI_FORMAT)f;
	if(F==target_format)
		return;
	target_format=F;
	CreateBuffers();
}

void FramebufferDX1x::RestoreDeviceObjects(void *dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(m_pImmediateContext);
	if(!m_pd3dDevice)
		return;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	RecompileShaders();
	CreateBuffers();
}

void FramebufferDX1x::RecompileShaders()
{
	CreateBuffers();
}

void FramebufferDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pImmediateContext);

	SAFE_RELEASE(m_pBufferVertexDecl);
	SAFE_RELEASE(m_pVertexBuffer);
	
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

bool FramebufferDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return true;
}

FramebufferDX1x::~FramebufferDX1x()
{
	Destroy();
}


bool FramebufferDX1x::IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat)
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

bool FramebufferDX1x::CreateBuffers()
{
	if(!Width||!Height)
		return false;
	HRESULT hr=S_OK;
	D3D1x_TEXTURE2D_DESC desc=
	{
		Width,
		Height,
		1,
		1,
		target_format,
		{1,0},
		D3D1x_USAGE_DEFAULT,
		D3D1x_BIND_RENDER_TARGET|D3D1x_BIND_SHADER_RESOURCE,
		0,
		0
	};
	SAFE_RELEASE(hdr_buffer_texture);
	V_CHECK(m_pd3dDevice->CreateTexture2D(	&desc,
											NULL,
											&hdr_buffer_texture
										))
	SAFE_RELEASE(m_pHDRRenderTarget)
	m_pHDRRenderTarget=MakeRenderTarget(hdr_buffer_texture);
	//hdr_buffer_texture->GetDesc(&desc);
	SAFE_RELEASE(buffer_texture_SRV);
    V_CHECK(m_pd3dDevice->CreateShaderResourceView(hdr_buffer_texture, NULL, &buffer_texture_SRV ));

 	//desc.Width=Width/4;
	//desc.Height=Height/4;
	DXGI_FORMAT fmtDepthTex = DXGI_FORMAT_D32_FLOAT;//DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	
	DXGI_FORMAT possibles[]={
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
		DXGI_FORMAT_D32_FLOAT,
		DXGI_FORMAT_D16_UNORM,
		DXGI_FORMAT_UNKNOWN};

	SAFE_RELEASE(buffer_depth_texture);
	// Try creating a depth texture
	desc.Width = Width;
	desc.Height = Height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = fmtDepthTex;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D1x_USAGE_DEFAULT;
	desc.BindFlags = D3D1x_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	if(fmtDepthTex!=DXGI_FORMAT_UNKNOWN)
	{
		V_CHECK(m_pd3dDevice->CreateTexture2D(	&desc,
												NULL,
												&buffer_depth_texture))
	}
	SAFE_RELEASE(m_pBufferDepthSurface)
	if(buffer_depth_texture)
		hr=m_pd3dDevice->CreateDepthStencilView((ID3D1xResource*)buffer_depth_texture, NULL, &m_pBufferDepthSurface);

	const D3D1x_INPUT_ELEMENT_DESC decl[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	16,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
	};

	// Witness the following DX11 silliness.
	D3D1x_PASS_DESC PassDesc;

	ID3D1xEffect * effect=NULL;
	CreateEffect(m_pd3dDevice,&effect,_T("simul_hdr.fx"));
	ID3D1xEffectTechnique*	tech=effect->GetTechniqueByName("simul_direct");
	assert(tech->IsValid());
	ID3D1xEffectPass *pass=tech->GetPassByIndex(0);
	assert(pass->IsValid());
	hr=pass->GetDesc(&PassDesc);
	V_CHECK(hr);

	SAFE_RELEASE(m_pBufferVertexDecl);
	hr=m_pd3dDevice->CreateInputLayout(
		decl, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize
		, &m_pBufferVertexDecl);
	SAFE_RELEASE(effect);

	static float x=-1.f,y=-1.f;
	static float width=2.f,height=2.f;
	Vertext vertices[4] =
	{
		D3DXVECTOR4(x		,y			,0.f,	1.f), D3DXVECTOR2(0.f	,1.f),
		D3DXVECTOR4(x+width	,y			,0.f,	1.f), D3DXVECTOR2(1.f	,1.f),
		D3DXVECTOR4(x		,y+height	,0.f,	1.f), D3DXVECTOR2(0.f	,0.f),
		D3DXVECTOR4(x+width	,y+height	,0.f,	1.f), D3DXVECTOR2(1.f	,0.f),
	};
	D3D1x_BUFFER_DESC bdesc=
	{
        4*sizeof(Vertext),
        D3D1x_USAGE_DEFAULT,
        D3D1x_BIND_VERTEX_BUFFER,
        0,
        0
	};
    D3D1x_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(Vertext);
    InitData.SysMemSlicePitch = 0;
	SAFE_RELEASE(m_pVertexBuffer);
	hr=m_pd3dDevice->CreateBuffer(&bdesc,&InitData,&m_pVertexBuffer);
	return (hr==S_OK);
}

ID3D1xRenderTargetView* FramebufferDX1x::MakeRenderTarget(const ID3D1xTexture2D* pTexture)
{
	ID3D1xRenderTargetView* pRenderTargetView;
	HRESULT hr;
	// Setup the description of the render target view.
	D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	renderTargetViewDesc.Format = target_format;
	renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	renderTargetViewDesc.Texture2D.MipSlice = 0;
	// Create the render target in DX11:
	hr=m_pd3dDevice->CreateRenderTargetView((ID3D1xResource*)pTexture,&renderTargetViewDesc, &pRenderTargetView);
	return pRenderTargetView;
}

ID3D11Texture2D* makeStagingTexture(ID3D1xDevice			*m_pd3dDevice
							,ID3D1xDeviceContext	*m_pImmediateContext,int w,int h,DXGI_FORMAT target_format)
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

void FramebufferDX1x::CopyToMemory(void *target)
{
	CopyToMemory(target,0,Width*Height);
}

void FramebufferDX1x::CopyToMemory(void *target,int start_texel,int texels)
{
	if(!stagingTexture)
		stagingTexture		=makeStagingTexture(m_pd3dDevice,m_pImmediateContext,Width,Height,target_format);
	D3D11_BOX sourceRegion;
	sourceRegion.left = 0;
	sourceRegion.right = Width;
	sourceRegion.top = 0;
	sourceRegion.bottom = Height;
	sourceRegion.front = 0;
	sourceRegion.back = 1;
	m_pImmediateContext->CopySubresourceRegion(stagingTexture, 0, 0, 0, 0, GetColorTexResource(), 0, &sourceRegion);
HRESULT hr=S_OK;
	D3D11_MAPPED_SUBRESOURCE msr;
	V_CHECK(m_pImmediateContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &msr));
	int byteSize=simul::dx11::ByteSizeOfFormatElement(target_format);
	int required_pitch=Width*byteSize;
	char *dst=(char*)target;
	if(msr.RowPitch==required_pitch)
	{
		const char *dat=(const char *)msr.pData;
		dat+=start_texel*byteSize;
		dst+=start_texel*byteSize;
		memcpy(dst,dat,texels*byteSize);
	}
	else
	{
		char *src=(char*)msr.pData;
		int h0=start_texel/Width;
		int h1=h0+texels/Width;
		src+=msr.RowPitch*h0;
		dst+=byteSize*Width*h0;
		for(int i=h0;i<h1;i++)
		{
			memcpy(dst,src,required_pitch);
			dst+=required_pitch;
			src+=msr.RowPitch;
		}
	}
	// copy data
	m_pImmediateContext->Unmap(stagingTexture, 0);
}

void FramebufferDX1x::Activate()
{
	if(!m_pImmediateContext)
		RestoreDeviceObjects(m_pd3dDevice);
	if(!m_pImmediateContext)
		return;
	HRESULT hr=S_OK;
	m_pImmediateContext->RSGetViewports(&num_v,NULL);
	if(num_v>0)
		m_pImmediateContext->RSGetViewports(&num_v,m_OldViewports);

	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	m_pImmediateContext->OMGetRenderTargets(	1,
												&m_pOldRenderTarget,
												&m_pOldDepthSurface
												);
	m_pImmediateContext->OMSetRenderTargets(1,&m_pHDRRenderTarget,m_pBufferDepthSurface);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width = (float)Width;
	viewport.Height = (float)Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	// Create the viewport.
	m_pImmediateContext->RSSetViewports(1, &viewport);
}

void FramebufferDX1x::Deactivate()
{
	m_pImmediateContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	if(num_v>0)
		m_pImmediateContext->RSSetViewports(num_v,m_OldViewports);
}

void FramebufferDX1x::Clear(float r,float g,float b,float a,int mask)
{
	// Clear the screen to black:
    float clearColor[4]={r,g,b,a};
    if(!mask)
		mask=D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL;
	m_pImmediateContext->ClearRenderTargetView(m_pHDRRenderTarget,clearColor);
	if(m_pBufferDepthSurface)
		m_pImmediateContext->ClearDepthStencilView(m_pBufferDepthSurface,mask, 1.f, 0);
}

void FramebufferDX1x::DeactivateAndRender(void *context,bool blend)
{
	Deactivate();
	Render(context,blend);
}

void FramebufferDX1x::Render(void *,bool blend)
{
	DrawQuad();
}

bool FramebufferDX1x::DrawQuad()
{
	HRESULT hr=S_OK;
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
	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pImmediateContext->IASetInputLayout(m_pBufferVertexDecl);
	m_pImmediateContext->Draw(4,0);
	return (hr==S_OK);
}
