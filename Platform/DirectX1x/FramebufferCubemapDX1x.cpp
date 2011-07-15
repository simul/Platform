#include "FramebufferCubemapDX1x.h"
#include <assert.h>
const int MIPLEVELS=1;

FramebufferCubemapDX1x::FramebufferCubemapDX1x()
	:m_pImmediateContext(NULL)
	,m_pCubeEnvDepthMap(NULL)
	,m_pCubeEnvMap(NULL)
	,m_pCubeEnvMapSRV(NULL)
	,Width(0)
	,Height(0)
	,current_face(0)
{
	for(int i=0;i<6;i++)
	{
		m_pCubeEnvMapRTV[i]=NULL;
		m_pCubeEnvDepthMapDSV[i]=NULL;
	}
}

FramebufferCubemapDX1x::~FramebufferCubemapDX1x()
{
	InvalidateDeviceObjects();
}

void FramebufferCubemapDX1x::SetWidthAndHeight(int w,int h)
{
	Width=w;
	Height=w;
	assert(h==w);
}

bool FramebufferCubemapDX1x::RestoreDeviceObjects(ID3D1xDevice* pd3dDevice)
{
	HRESULT hr=S_OK;
#ifdef DX10
	m_pImmediateContext=pd3dDevice;
#else
	SAFE_RELEASE(m_pImmediateContext);
	pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif
	// Create cubic depth stencil texture
	D3D1x_TEXTURE2D_DESC dstex;
	dstex.Width = Width;
	dstex.Height = Width;
	dstex.MipLevels = 1;
	dstex.ArraySize = 6;
	dstex.SampleDesc.Count = 1;
	dstex.SampleDesc.Quality = 0;
	dstex.Format = DXGI_FORMAT_R24G8_TYPELESS;//DXGI_FORMAT_D32_FLOAT;
	dstex.Usage = D3D1x_USAGE_DEFAULT;
	dstex.BindFlags = D3D1x_BIND_DEPTH_STENCIL | D3D1x_BIND_SHADER_RESOURCE;
	dstex.CPUAccessFlags = 0;
	dstex.MiscFlags = D3D1x_RESOURCE_MISC_TEXTURECUBE;
 
	B_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &m_pCubeEnvDepthMap ));

	// Create the depth stencil view for the entire cube
	D3D1x_DEPTH_STENCIL_VIEW_DESC DescDS;
    ZeroMemory( &DescDS, sizeof( DescDS ) );
	DescDS.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DescDS.ViewDimension = D3D1x_DSV_DIMENSION_TEXTURE2DARRAY;
	DescDS.Texture2DArray.FirstArraySlice = 0;
	DescDS.Texture2DArray.ArraySize = 6;
	DescDS.Texture2DArray.MipSlice = 0;
 
	//B_RETURN( pd3dDevice->CreateDepthStencilView(m_pCubeEnvDepthMap, &DescDS, &m_pCubeEnvDepthMapDSV ));
	for(int i=0;i<6;i++)
	{
		DescDS.Texture2DArray.FirstArraySlice = i;
		DescDS.Texture2DArray.ArraySize = 1;
		B_RETURN(pd3dDevice->CreateDepthStencilView(m_pCubeEnvDepthMap, &DescDS, &(m_pCubeEnvDepthMapDSV[i])));
	}
	 
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Create the cube map for env map render target
	dstex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dstex.BindFlags = D3D1x_BIND_RENDER_TARGET | D3D1x_BIND_SHADER_RESOURCE;
	dstex.MiscFlags = D3D1x_RESOURCE_MISC_GENERATE_MIPS | D3D1x_RESOURCE_MISC_TEXTURECUBE;
	dstex.MipLevels = MIPLEVELS;
 
	B_RETURN(pd3dDevice->CreateTexture2D(&dstex,NULL,&m_pCubeEnvMap));

	// Create the 6-face render target view
	D3D1x_RENDER_TARGET_VIEW_DESC DescRT;
	DescRT.Format = dstex.Format;
	DescRT.ViewDimension = D3D1x_RTV_DIMENSION_TEXTURE2DARRAY;
	DescRT.Texture2DArray.FirstArraySlice = 0;
	DescRT.Texture2DArray.ArraySize = 6;
	DescRT.Texture2DArray.MipSlice = 0;
	 
	for(int i=0;i<6;i++)
	{
		DescRT.Texture2DArray.FirstArraySlice = i;
		DescRT.Texture2DArray.ArraySize = 1;
		B_RETURN( pd3dDevice->CreateRenderTargetView(m_pCubeEnvMap, &DescRT, &(m_pCubeEnvMapRTV[i])));
	}

	// Create the shader resource view for the cubic env map
	D3D1x_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format = dstex.Format;
	SRVDesc.ViewDimension = D3D1x_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = MIPLEVELS;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	 
	B_RETURN( pd3dDevice->CreateShaderResourceView(m_pCubeEnvMap, &SRVDesc, &m_pCubeEnvMapSRV ));

	return true;
}

bool FramebufferCubemapDX1x::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pCubeEnvDepthMap);
	SAFE_RELEASE(m_pCubeEnvMap);
	for(int i=0;i<6;i++)
	{
		SAFE_RELEASE(m_pCubeEnvMapRTV[i]);
		SAFE_RELEASE(m_pCubeEnvDepthMapDSV[i]);
	}
	SAFE_RELEASE(m_pCubeEnvMapSRV);
	SAFE_RELEASE(m_pImmediateContext);
	return true;
}

void FramebufferCubemapDX1x::SetCurrentFace(int i)
{
	current_face=i;
}
void FramebufferCubemapDX1x::Activate()
{
#if 0
	for(int i=0;i<6;i++)
	{
		cubemap_framebuffers[i].Activate();
		if(simulSkyRenderer)
		{
			simulSkyRenderer->SetMatrices(view_matrices[i],proj);
			hr=simulSkyRenderer->Render(true);
		}
		cubemap_framebuffers[i].Deactivate();
	}
#else
	HRESULT hr=S_OK;
	unsigned int num_v=0;
	m_pImmediateContext->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		m_pImmediateContext->RSGetViewports(&num_v,m_OldViewports);

	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	m_pImmediateContext->OMGetRenderTargets(	1,
												&m_pOldRenderTarget,
												&m_pOldDepthSurface
												);
	m_pImmediateContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],m_pCubeEnvDepthMapDSV[current_face]);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width = (float)Width;
	viewport.Height = (float)Width;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	// Create the viewport.
	m_pImmediateContext->RSSetViewports(1, &viewport);
#endif
}

void FramebufferCubemapDX1x::Deactivate()
{
	//ID3D11RenderTargetView* rTargets[2] = { m_pOldRenderTarget, NULL };
	m_pImmediateContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	// Create the viewport.
	m_pImmediateContext->RSSetViewports(1,m_OldViewports);
}
