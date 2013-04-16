#include "Simul/Platform/DirectX9/Framebuffer.h"
#include "Simul/Platform/DirectX9/Macros.h"
#include "Simul/Platform/DirectX9/CreateDX9Effect.h"

Framebuffer::Framebuffer()
	:m_pd3dDevice(NULL)
	,buffer_depth_texture(NULL)
	,buffer_texture(NULL)
	,m_pHDRRenderTarget(NULL)
	,m_pBufferDepthSurface(NULL)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
{
#ifndef XBOX
	texture_format=D3DFMT_A16B16G16R16F;
#else
	texture_format=D3DFMT_LIN_A16B16G16R16F;
#endif
	//if(hr!=S_OK)
#ifndef XBOX
		texture_format=D3DFMT_A32B32G32R32F;
#else
		texture_format=D3DFMT_LIN_A32B32G32R32F;
#endif
}

Framebuffer::~Framebuffer()
{
}

void Framebuffer::SetWidthAndHeight(int w,int h)
{
	if(buffer_texture!=NULL&&w==Width&&h==Height)
		return;
	Width=w;
	Height=h;
	MakeTexture();
}
void Framebuffer::MakeTexture()
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(buffer_texture);
	SAFE_RELEASE(m_pHDRRenderTarget);
	if(!Width||!Height)
		return;
	m_pd3dDevice->CreateTexture(	Width,
									Height,
									1,
									D3DUSAGE_RENDERTARGET,
									texture_format,
									D3DPOOL_DEFAULT,
									&buffer_texture,
									NULL);
	m_pHDRRenderTarget=MakeRenderTarget(buffer_texture);
}

bool Framebuffer::SetFormat(D3DFORMAT f)
{
	bool ok=CanUseTexFormat(m_pd3dDevice,f)==S_OK;
	if(ok)
	{
		if(texture_format==f)
			return true;
		texture_format=f;
		MakeTexture();
	}
	return ok;
}
	
void Framebuffer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	MakeTexture();
}

void Framebuffer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(buffer_texture);
	SAFE_RELEASE(buffer_depth_texture);
	SAFE_RELEASE(m_pHDRRenderTarget);
	SAFE_RELEASE(m_pBufferDepthSurface);
}

void Framebuffer::Activate()
{
	m_pOldRenderTarget=NULL;
	m_pOldDepthSurface=NULL;
	D3DSURFACE_DESC desc;
	buffer_texture->GetLevelDesc(0,&desc);
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
void Framebuffer::Clear(float r,float g,float b,float a,int mask)
{
	if(!mask)
		mask=D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER;
	m_pd3dDevice->Clear(0L,NULL,mask,D3DCOLOR_COLORVALUE(r,g,b,a),1.f,0L);
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