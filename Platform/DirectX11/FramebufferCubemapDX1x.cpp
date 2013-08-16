#include "FramebufferCubemapDX1x.h"
#include <assert.h>
const int MIPLEVELS=1;

FramebufferCubemapDX1x::FramebufferCubemapDX1x()
	:m_pCubeEnvDepthMap(NULL)
	,m_pCubeEnvMap(NULL)
	,m_pCubeEnvMapSRV(NULL)
	,Width(0)
	,Height(0)
	,current_face(0)
	,format(DXGI_FORMAT_R8G8B8A8_UNORM)
	,stagingTexture(NULL)
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

void FramebufferCubemapDX1x::SetFormat(int f)
{
	DXGI_FORMAT F=(DXGI_FORMAT)f;
	if(F==format)
		return;
	format=F;
	//CreateBuffers();
}

void FramebufferCubemapDX1x::RestoreDeviceObjects(void* dev)
{
	HRESULT hr=S_OK;
	pd3dDevice=(ID3D1xDevice*)dev;
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
 
	V_CHECK( pd3dDevice->CreateTexture2D( &dstex, NULL, &m_pCubeEnvDepthMap ));

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
		V_CHECK(pd3dDevice->CreateDepthStencilView(m_pCubeEnvDepthMap, &DescDS, &(m_pCubeEnvDepthMapDSV[i])));
	}
	 
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Create the cube map for env map render target
	dstex.Format = format;
	dstex.BindFlags = D3D1x_BIND_RENDER_TARGET | D3D1x_BIND_SHADER_RESOURCE;
	dstex.MiscFlags = D3D1x_RESOURCE_MISC_GENERATE_MIPS | D3D1x_RESOURCE_MISC_TEXTURECUBE;
	dstex.MipLevels = MIPLEVELS;
 
	V_CHECK(pd3dDevice->CreateTexture2D(&dstex,NULL,&m_pCubeEnvMap));

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
		V_CHECK( pd3dDevice->CreateRenderTargetView(m_pCubeEnvMap, &DescRT, &(m_pCubeEnvMapRTV[i])));
	}

	// Create the shader resource view for the cubic env map
	D3D1x_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format = dstex.Format;
	SRVDesc.ViewDimension = D3D1x_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = MIPLEVELS;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	 
	V_CHECK( pd3dDevice->CreateShaderResourceView(m_pCubeEnvMap, &SRVDesc, &m_pCubeEnvMapSRV ));
}
ID3D11Texture2D* makeStagingTexture(ID3D1xDevice *pd3dDevice,int w,DXGI_FORMAT target_format)
{
	D3D11_TEXTURE2D_DESC dstex;
	dstex.Width					= w;
	dstex.Height				= w;
	dstex.MipLevels				= 1;
	dstex.ArraySize				= 6;
	dstex.SampleDesc.Count		= 1;
	dstex.SampleDesc.Quality	= 0;
	dstex.Format				= target_format;
	dstex.Usage					= D3D11_USAGE_STAGING;
	dstex.BindFlags				= 0;
	dstex.CPUAccessFlags		=D3D11_CPU_ACCESS_READ| D3D11_CPU_ACCESS_WRITE;
	dstex.MiscFlags				=D3D11_RESOURCE_MISC_TEXTURECUBE;
	ID3D11Texture2D* tex		=NULL;
	pd3dDevice->CreateTexture2D(&dstex,NULL,&tex);
	return tex;
}

void FramebufferCubemapDX1x::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pCubeEnvDepthMap);
	SAFE_RELEASE(m_pCubeEnvMap);
	for(int i=0;i<6;i++)
	{
		SAFE_RELEASE(m_pCubeEnvMapRTV[i]);
		SAFE_RELEASE(m_pCubeEnvDepthMapDSV[i]);
	}
	SAFE_RELEASE(m_pCubeEnvMapSRV);
}

void FramebufferCubemapDX1x::SetCurrentFace(int i)
{
	current_face=i;
}

ID3D11Texture2D *FramebufferCubemapDX1x::GetCopy(void *context)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext*)context;
	if(!stagingTexture)
		stagingTexture		=makeStagingTexture(pd3dDevice,Width,format);
	D3D11_BOX sourceRegion;
	sourceRegion.left = 0;
	sourceRegion.right = Width;
	sourceRegion.top = 0;
	sourceRegion.bottom = Height;
	sourceRegion.front = 0;
	sourceRegion.back = 1;
	for(int i=0;i<6;i++)
		m_pImmediateContext->CopySubresourceRegion(stagingTexture,i, 0, 0, 0, m_pCubeEnvMap,i, &sourceRegion);
	return stagingTexture;
}

void FramebufferCubemapDX1x::Activate(void *context, float viewportX, float viewportY, float viewportW, float viewportH)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
#if 0
	for(int i=0;i<6;i++)
	{
		cubemap_framebuffers[i].Activate(context);
		if(simulSkyRenderer)
		{
			simulSkyRenderer->SetMatrices(view_matrices[i],proj);
			hr=simulSkyRenderer->Render(true);
		}
		cubemap_framebuffers[i].Deactivate(context);
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
	viewport.Width = floorf((float)Width*viewportW + 0.5f);
	viewport.Height = floorf((float)Height*viewportH + 0.5f);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = floorf((float)Width*viewportX + 0.5f);
	viewport.TopLeftY = floorf((float)Height*viewportY + 0.5f);

	// Create the viewport.
	m_pImmediateContext->RSSetViewports(1, &viewport);
#endif
}

void FramebufferCubemapDX1x::Deactivate(void *context)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	m_pImmediateContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	// Create the viewport.
	m_pImmediateContext->RSSetViewports(1,m_OldViewports);
}

void FramebufferCubemapDX1x::Clear(void *context,float r,float g,float b,float a,float depth,int mask)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	if(!mask)
		mask=D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL;
	// Clear the screen to black:
    float clearColor[4]={r,g,b,a};
    for(int i=0;i<6;i++)
    {
		m_pImmediateContext->ClearRenderTargetView(m_pCubeEnvMapRTV[i],clearColor);
		if(m_pCubeEnvDepthMapDSV[i])
			m_pImmediateContext->ClearDepthStencilView(m_pCubeEnvDepthMapDSV[i],mask,depth, 0);
		}
}

void FramebufferCubemapDX1x::ClearColour(void *context,float r,float g,float b,float a)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	float clearColor[4]={r,g,b,a};
	for(int i=0;i<6;i++)
	{
		m_pImmediateContext->ClearRenderTargetView(m_pCubeEnvMapRTV[i],clearColor);
	}
}

void FramebufferCubemapDX1x::GetTextureDimensions(const void* tex, unsigned int& widthOut, unsigned int& heightOut) const
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
