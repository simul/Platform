#define NOMINMAX
#include "Simul/Platform/DirectX9/Framebuffer.h"
#include "Simul/Platform/DirectX9/Macros.h"
#include "Simul/Platform/DirectX9/CreateDX9Effect.h"

using namespace simul;
using namespace dx9;

Framebuffer::Framebuffer()
	:m_pd3dDevice(NULL)
	,buffer_depth_texture(NULL)
	,buffer_texture(NULL)
	,m_pHDRRenderTarget(NULL)
	,m_pBufferDepthSurface(NULL)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
	,depth_format((D3DFORMAT)0)
{
	texture_format=D3DFMT_A16B16G16R16F;
	texture_format=D3DFMT_A32B32G32R32F;
}

Framebuffer::~Framebuffer()
{
	InvalidateDeviceObjects();
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
	V_CHECK(m_pd3dDevice->CreateTexture(	Width,
									Height,
									1,
									D3DUSAGE_RENDERTARGET,
									texture_format,
									D3DPOOL_DEFAULT,
									&buffer_texture,
									NULL));
	m_pHDRRenderTarget=MakeRenderTarget(buffer_texture);
	
	SAFE_RELEASE(buffer_depth_texture);
	SAFE_RELEASE(m_pBufferDepthSurface);
	if(depth_format!=0)
	{
		V_CHECK(m_pd3dDevice->CreateTexture(	Width,
										Height,
										1,
										D3DUSAGE_DEPTHSTENCIL,
										depth_format,
										D3DPOOL_DEFAULT,
										&buffer_depth_texture,
										NULL));
		buffer_depth_texture->GetSurfaceLevel(0,&m_pBufferDepthSurface);
	}
}

void Framebuffer::SetFormat(int f)
{
	D3DFORMAT F=(D3DFORMAT)f;
	bool ok=CanUseTexFormat(m_pd3dDevice,F)==S_OK;
	if(ok)
	{
		if(texture_format==F)
			return;
		texture_format=F;
		MakeTexture();
	}
}
	
void Framebuffer::SetDepthFormat(int f)
{
	D3DFORMAT F=(D3DFORMAT)f;
	if(depth_format==F)
		return;
	bool ok=CanUseTexFormat(m_pd3dDevice,F)==S_OK;
	if(ok)
	{
		depth_format=F;
	}
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

void Framebuffer::SaveOldRTs(void *)
{
	m_pd3dDevice->GetViewport(&old_viewport);
	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget);
	m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface);
}

void Framebuffer::Activate(void *context)
{
	m_pOldRenderTarget=NULL;
	m_pOldDepthSurface=NULL;
	if(!m_pHDRRenderTarget)
		this->RestoreDeviceObjects(m_pd3dDevice);
	if(!m_pHDRRenderTarget)
		return;
	//D3DSURFACE_DESC desc;
	//V_CHECK(buffer_texture->GetLevelDesc(0,&desc));
	SaveOldRTs(context);
	//V_CHECK(m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget));
	//m_pOldRenderTarget->GetDesc(&desc);
	//V_CHECK(m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface));
	V_CHECK(m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget));
	if(m_pBufferDepthSurface)
		V_CHECK(m_pd3dDevice->SetDepthStencilSurface(m_pBufferDepthSurface));
	SetViewport(context,0,0,1.f,1.f);
}

void Framebuffer::ActivateColour(void *context,const float viewportXYWH[4])
{
	m_pOldRenderTarget=NULL;
	m_pOldDepthSurface=NULL;
	//D3DSURFACE_DESC desc;
	//V_CHECK(buffer_texture->GetLevelDesc(0,&desc));
	//V_CHECK(m_pd3dDevice->GetRenderTarget(0,&m_pOldRenderTarget));
	//m_pOldRenderTarget->GetDesc(&desc);
	//V_CHECK(m_pd3dDevice->GetDepthStencilSurface(&m_pOldDepthSurface));
	SaveOldRTs(context);
	V_CHECK(m_pd3dDevice->SetRenderTarget(0,m_pHDRRenderTarget));
	V_CHECK(m_pd3dDevice->SetDepthStencilSurface(NULL));
	SetViewport(context,viewportXYWH[0],viewportXYWH[1],viewportXYWH[2],viewportXYWH[3]);
}

void Framebuffer::ActivateViewport(void* context,float viewportX, float viewportY, float viewportW, float viewportH)
{
	Activate(context);
	SetViewport(context,viewportX,viewportY,viewportW,viewportH);
}

void Framebuffer::SetViewport(void*,float viewportX, float viewportY, float viewportW, float viewportH,float Z,float D)
{
	D3DVIEWPORT9 viewport;
	viewport.Width	=(int)(viewportW*Width);
	viewport.Height	=(int)(viewportH*Height);
	viewport.X		=(int)(viewportX*Width);
	viewport.Y		=(int)(viewportY*Height);
	viewport.MinZ	=Z;
	viewport.MaxZ	=D;
	m_pd3dDevice->SetViewport(&viewport);
}

void Framebuffer::Deactivate(void *)
{
	if(!m_pHDRRenderTarget)
		return;
	m_pd3dDevice->SetRenderTarget(0,m_pOldRenderTarget);
	if(m_pOldDepthSurface)
		m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
	m_pd3dDevice->SetViewport(&old_viewport);
	old_viewport.Width=old_viewport.Height=old_viewport.X=old_viewport.Y=0;
	old_viewport.MaxZ=old_viewport.MinZ=0.f;
}

void Framebuffer::DeactivateDepth(void*)
{
	if(m_pOldDepthSurface)
		m_pd3dDevice->SetDepthStencilSurface(m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldDepthSurface);
}

void Framebuffer::Clear(void *,float r,float g,float b,float a,float depth,int mask)
{
	// Don't yet support reverse depth on dx9.
	if(!mask)
		mask=D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER;
	m_pd3dDevice->Clear(0L,NULL,mask,D3DCOLOR_COLORVALUE(r,g,b,a),depth,0L);
}

void Framebuffer::ClearColour(void *context,float r,float g,float b,float a)
{
	Clear(context,r,g,b,a,0.f,D3DCLEAR_TARGET);
}