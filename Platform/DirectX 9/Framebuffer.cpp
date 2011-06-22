#include "Simul/Platform/DirectX 9/Framebuffer.h"
#include "Simul/Platform/DirectX 9/Macros.h"
#include "Simul/Platform/DirectX 9/CreateDX9Effect.h"

Framebuffer::Framebuffer()
	:m_pd3dDevice(NULL)
	,Width(0)
	,Height(0)
	,buffer_depth_texture(NULL)
	,hdr_buffer_texture(NULL)
	,m_pHDRRenderTarget(NULL)
	,m_pBufferDepthSurface(NULL)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
{
}

Framebuffer::~Framebuffer()
{
}

bool Framebuffer::RestoreDeviceObjects(void *dev,int w,int h)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	if(!w||!h)
		return false;
	Width=w;
	Height=h;
#ifndef XBOX
	D3DFORMAT hdr_format=D3DFMT_A16B16G16R16F;
#else
	D3DFORMAT hdr_format=D3DFMT_LIN_A16B16G16R16F;
#endif
	HRESULT hr=-1;//CanUse16BitFloats(pd3dDevice);

	if(hr!=S_OK)
#ifndef XBOX
		hdr_format=D3DFMT_A32B32G32R32F;
#else
		hdr_format=D3DFMT_LIN_A32B32G32R32F;
#endif
	SAFE_RELEASE(hdr_buffer_texture);
	hr=m_pd3dDevice->CreateTexture(	Width,
									Height,
									1,
									D3DUSAGE_RENDERTARGET,
									hdr_format,
									D3DPOOL_DEFAULT,
									&hdr_buffer_texture,
									NULL
								);
/*	D3DFORMAT fmtDepthTex = D3DFMT_UNKNOWN;
	D3DFORMAT possibles[]={D3DFMT_D24S8,D3DFMT_D24FS8,D3DFMT_D32,D3DFMT_D24X8,D3DFMT_D16,D3DFMT_UNKNOWN};

	LPDIRECT3DSURFACE9 g_BackBuffer;
	pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_BackBuffer);
	D3DSURFACE_DESC desc;
	g_BackBuffer->GetDesc(&desc);
	SAFE_RELEASE(g_BackBuffer);
	D3DDISPLAYMODE d3ddm;
#ifdef XBOX
	if(FAILED(hr=pd3dDevice->GetDisplayMode( D3DADAPTER_DEFAULT, &d3ddm )))
	{
		return (hr==S_OK);
	}
#else
	LPDIRECT3D9 d3d;
	pd3dDevice->GetDirect3D(&d3d);
	if(FAILED(hr=d3d->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm )))
	{
		return (hr==S_OK);
	}
#endif
	for(int i=0;i<100;i++)
	{
		D3DFORMAT possible=possibles[i];
		if(possible==D3DFMT_UNKNOWN)
			break;
		hr=IsDepthFormatOk(pd3dDevice,possible,d3ddm.Format,desc.Format);
		if(SUCCEEDED(hr))
		{
			fmtDepthTex = possible;
		}
	}
	
	SAFE_RELEASE(buffer_depth_texture);
	LPDIRECT3DSURFACE9	m_pOldDepthSurface;
	hr=pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
	if(m_pOldDepthSurface)
		hr=m_pOldDepthSurface->GetDesc(&desc);
	SAFE_RELEASE(m_pOldDepthSurface);
	// Try creating a depth texture
	if(fmtDepthTex!=D3DFMT_UNKNOWN)
		hr=pd3dDevice->CreateTexture(	Width,
										Height,
										1,
										D3DUSAGE_DEPTHSTENCIL,
										desc.Format,
										D3DPOOL_DEFAULT,
										&buffer_depth_texture,
										NULL
									);
	SAFE_RELEASE(m_pBufferDepthSurface);
	if(buffer_depth_texture)
		buffer_depth_texture->GetSurfaceLevel(0,&m_pBufferDepthSurface);
	
	SAFE_RELEASE(m_pBufferDepthSurface);*/
	SAFE_RELEASE(m_pHDRRenderTarget);
	m_pHDRRenderTarget=MakeRenderTarget(hdr_buffer_texture);
	return true;
}

bool Framebuffer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(hdr_buffer_texture);
	SAFE_RELEASE(buffer_depth_texture);
	SAFE_RELEASE(m_pHDRRenderTarget);
	SAFE_RELEASE(m_pBufferDepthSurface);
	return true;
}

void Framebuffer::Activate()
{
	m_pOldRenderTarget=NULL;
	m_pOldDepthSurface=NULL;
	D3DSURFACE_DESC desc;
	hdr_buffer_texture->GetLevelDesc(0,&desc);
	HRESULT hr=m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget);
	m_pOldRenderTarget->GetDesc(&desc);
	hr=m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
	hr=m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget);
	if(m_pBufferDepthSurface)
		m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface);
}

void Framebuffer::Deactivate()
{
	//m_pOldRenderTarget->GetDesc(&desc);
	m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
	if(m_pOldDepthSurface)
		m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
}

void Framebuffer::DeactivateAndRender(bool blend)
{
	Deactivate();
	Render(blend);
}
void Framebuffer::Render(bool blend)
{
	blend;
}