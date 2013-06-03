#include "Framebuffer3D.h"
using namespace simul;
using namespace dx11;
Framebuffer3D::Framebuffer3D()
	:BaseFramebuffer(0,0)
	,m_pd3dDevice(NULL)
	,texture(NULL)
	,texture_SRV(NULL)
	,renderTarget(NULL)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
	,target_format(DXGI_FORMAT_R32G32B32A32_FLOAT)
	,num_v(0)
{
}

Framebuffer3D::~Framebuffer3D()
{
	InvalidateDeviceObjects();
}

void Framebuffer3D::RestoreDeviceObjects(void *dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	if(!m_pd3dDevice)
		return;
	CreateBuffers();
}

void Framebuffer3D::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(renderTarget)
	SAFE_RELEASE(texture);
	SAFE_RELEASE(texture_SRV);
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
}
void Framebuffer3D::Activate(void *context)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
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
	m_pImmediateContext->OMSetRenderTargets(1,&renderTarget,NULL);
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
void Framebuffer3D::Deactivate(void *context)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	m_pImmediateContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	if(num_v>0)
		m_pImmediateContext->RSSetViewports(num_v,m_OldViewports);
	m_pImmediateContext=NULL;
}
void Framebuffer3D::SetFormat(int f)
{
	DXGI_FORMAT F=(DXGI_FORMAT)f;
	if(F==target_format)
		return;
	target_format=F;
	CreateBuffers();
}
void Framebuffer3D::Clear(void* context,float,float,float,float,float,int mask)
{
}
void* Framebuffer3D::GetColorTex()
{
	return texture_SRV;
}
void Framebuffer3D::SetWidthAndHeight(int w,int h)
{
	if(Width!=w||Height!=h)
	{
		Width=w;
		Height=h;
		if(Depth>0&&m_pd3dDevice)
			CreateBuffers();
	}
}
void Framebuffer3D::SetDepth(int d)
{
	if(Depth!=d)
	{
		Depth=d;
		if(m_pd3dDevice)
			CreateBuffers();
	}
}

void Framebuffer3D::CreateBuffers()
{
	if(!Width||!Height||!Depth)
		return ;
	HRESULT hr=S_OK;
	D3D11_TEXTURE3D_DESC desc=
	{
		Width,
		Height,
		Depth,
		1,
		target_format,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
		0,
		0
	};
	SAFE_RELEASE(texture);
	m_pd3dDevice->CreateTexture3D(&desc,NULL,&texture);
	SAFE_RELEASE(texture_SRV);
    m_pd3dDevice->CreateShaderResourceView(texture,NULL,&texture_SRV );

	SAFE_RELEASE(renderTarget);

	D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	renderTargetViewDesc.Format = target_format;
	renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
	renderTargetViewDesc.Texture2D.MipSlice = 0;
	// Create the render target in DX11:
	hr=m_pd3dDevice->CreateRenderTargetView((ID3D1xResource*)texture,&renderTargetViewDesc, &renderTarget);
	
}