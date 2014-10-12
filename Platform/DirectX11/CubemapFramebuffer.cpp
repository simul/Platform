#define NOMINMAX
#include "CubemapFramebuffer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "D3dx11effect.h"
#include <assert.h>
const int MIPLEVELS=1;

using namespace simul;
using namespace dx11;

CubemapFramebuffer::CubemapFramebuffer()
{
	target_format=crossplatform::RGBA_8_UNORM;
}

bool CubemapFramebuffer::CreateBuffers()
{
	Framebuffer::CreateBuffers();

	if(depth_format!=crossplatform::UNKNOWN)
		buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,depth_format,false,false,true);

	// Create the cube map for env map render target
	D3D11_TEXTURE2D_DESC tex2dDesc;
	tex2dDesc.Width					=Width;
	tex2dDesc.Height				=Width;
	tex2dDesc.ArraySize				=6;
	tex2dDesc.SampleDesc.Count		=1;
	tex2dDesc.SampleDesc.Quality	=0;
	tex2dDesc.Usage					=D3D11_USAGE_DEFAULT;
	tex2dDesc.CPUAccessFlags		=0;
	tex2dDesc.Format				=RenderPlatform::ToDxgiFormat(target_format);
	tex2dDesc.BindFlags				=D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	tex2dDesc.MiscFlags				=D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;
	tex2dDesc.MipLevels				=MIPLEVELS;
	dx11::Texture *t				=(dx11::Texture *)buffer_texture;
	ID3D11Texture2D *tex2d			=(ID3D11Texture2D*)t->texture;
	V_CHECK(renderPlatform->AsD3D11Device()->CreateTexture2D(&tex2dDesc,NULL,&tex2d));
	t->texture=tex2d;

	// Create the 6-face render target view
	D3D11_RENDER_TARGET_VIEW_DESC DescRT;
	DescRT.Format							=tex2dDesc.Format;
	DescRT.ViewDimension					=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	DescRT.Texture2DArray.FirstArraySlice	=0;
	DescRT.Texture2DArray.ArraySize			=6;
	DescRT.Texture2DArray.MipSlice			=0;
	 
	for(int i=0;i<6;i++)
	{
		DescRT.Texture2DArray.FirstArraySlice = i;
		DescRT.Texture2DArray.ArraySize = 1;
		V_CHECK(renderPlatform->AsD3D11Device()->CreateRenderTargetView(tex2d, &DescRT, &(m_pCubeEnvMapRTV[i])));
	}
	// Create the shader resource view for the cubic env map
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format						=tex2dDesc.Format;
	SRVDesc.ViewDimension				=D3D11_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels		=MIPLEVELS;
	SRVDesc.TextureCube.MostDetailedMip =0;
	 
	V_CHECK( renderPlatform->AsD3D11Device()->CreateShaderResourceView(tex2d, &SRVDesc, &t->shaderResourceView ));
	
	return true;
}

ID3D11Texture2D* makeStagingTexture(ID3D11Device *pd3dDevice,int w,DXGI_FORMAT target_format)
{
	D3D11_TEXTURE2D_DESC tex2dDesc;
	tex2dDesc.Width					= w;
	tex2dDesc.Height				= w;
	tex2dDesc.MipLevels				= 1;
	tex2dDesc.ArraySize				= 6;
	tex2dDesc.SampleDesc.Count		= 1;
	tex2dDesc.SampleDesc.Quality	= 0;
	tex2dDesc.Format				= target_format;
	tex2dDesc.Usage					= D3D11_USAGE_STAGING;
	tex2dDesc.BindFlags				= 0;
	tex2dDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_READ| D3D11_CPU_ACCESS_WRITE;
	tex2dDesc.MiscFlags				=D3D11_RESOURCE_MISC_TEXTURECUBE;
	ID3D11Texture2D* tex			=NULL;
	pd3dDevice->CreateTexture2D(&tex2dDesc,NULL,&tex);
	return tex;
}

ID3D11Texture2D* CubemapFramebuffer::GetCopy(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	D3D11_BOX sourceRegion;
	sourceRegion.left = 0;
	sourceRegion.right = Width;
	sourceRegion.top = 0;
	sourceRegion.bottom = Height;
	sourceRegion.front = 0;
	sourceRegion.back = 1;
	 dx11::Texture *t=(dx11::Texture *)buffer_texture;
	 ID3D11Texture2D *tex2d=(ID3D11Texture2D*)t->texture;
	 ID3D11Texture2D* stagingTexture=makeStagingTexture(renderPlatform->AsD3D11Device(),Width,RenderPlatform::ToDxgiFormat(target_format));
	for(int i=0;i<6;i++)
		pContext->CopySubresourceRegion(stagingTexture,i, 0, 0, 0, tex2d,i, &sourceRegion);
	return stagingTexture;
}

void CubemapFramebuffer::Activate(crossplatform::DeviceContext &deviceContext)
{
	SaveOldRTs(deviceContext);
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],buffer_depth_texture?buffer_depth_texture->AsD3D11DepthStencilView():NULL);
	SetViewport(deviceContext,0.f,0.f,1.f,1.f);
}

void CubemapFramebuffer::ActivateColour(crossplatform::DeviceContext &deviceContext,const float viewportXYWH[4])
{
	unsigned int num_v=0;
	deviceContext.asD3D11DeviceContext()->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		deviceContext.asD3D11DeviceContext()->RSGetViewports(&num_v,m_OldViewports);

	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	deviceContext.asD3D11DeviceContext()->OMGetRenderTargets(	1,
									&m_pOldRenderTarget,
									&m_pOldDepthSurface
									);
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],NULL);

	SetViewport(deviceContext,viewportXYWH[0],viewportXYWH[1],viewportXYWH[2],viewportXYWH[3],0.0,1.0f);
}

void CubemapFramebuffer::Deactivate(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	// Create the viewport.
	pContext->RSSetViewports(1,m_OldViewports);
}

void CubemapFramebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],NULL);
}

void CubemapFramebuffer::Clear(crossplatform::DeviceContext &deviceContext, float r, float g, float b, float a, float depth, int mask)
{
	ID3D11DeviceContext *pContext = deviceContext.asD3D11DeviceContext();
	if(!mask)
		mask=D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL;
	// Clear the screen to the colour specified:
    float clearColor[4]={r,g,b,a};
	if(current_face>=0)
	{
		pContext->ClearRenderTargetView(m_pCubeEnvMapRTV[current_face],clearColor);
	}
	else
	{
		for(int i=0;i<6;i++)
		{
			pContext->ClearRenderTargetView(m_pCubeEnvMapRTV[i],clearColor);
		//	if(m_pCubeEnvDepthMap[i]->AsD3D11DepthStencilView())
		//		pContext->ClearDepthStencilView(m_pCubeEnvDepthMap[i]->AsD3D11DepthStencilView(),mask,depth, 0);
		}
	}
		if(buffer_depth_texture&&buffer_depth_texture->AsD3D11DepthStencilView())
			pContext->ClearDepthStencilView(buffer_depth_texture->AsD3D11DepthStencilView(),mask,depth,0);
}

void CubemapFramebuffer::ClearColour(crossplatform::DeviceContext &deviceContext, float r, float g, float b, float a)
{
	float clearColor[4]={r,g,b,a};
	for(int i=0;i<6;i++)
	{
		deviceContext.asD3D11DeviceContext()->ClearRenderTargetView(m_pCubeEnvMapRTV[i], clearColor);
	}
}