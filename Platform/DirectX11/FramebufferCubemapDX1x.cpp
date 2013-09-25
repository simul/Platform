#include "FramebufferCubemapDX1x.h"
#include <assert.h>
const int MIPLEVELS=1;

using namespace simul::dx11;

FramebufferCubemapDX1x::FramebufferCubemapDX1x()
	:m_pCubeEnvDepthMap(NULL)
	,m_pCubeEnvMap(NULL)
	,m_pCubeEnvMapSRV(NULL)
	,m_pCubeEnvMapDepthSRV(NULL)
	,Width(0)
	,Height(0)
	,current_face(0)
	,format(DXGI_FORMAT_R8G8B8A8_UNORM)
	,stagingTexture(NULL)
	,sphericalHarmonicsEffect(NULL)
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
	D3D11_TEXTURE2D_DESC tex2dDesc;
	tex2dDesc.Width = Width;
	tex2dDesc.Height = Width;
	tex2dDesc.MipLevels = 1;
	tex2dDesc.ArraySize = 6;
	tex2dDesc.SampleDesc.Count = 1;
	tex2dDesc.SampleDesc.Quality = 0;
	tex2dDesc.Format = DXGI_FORMAT_R32_TYPELESS;//DXGI_FORMAT_D32_FLOAT;
	tex2dDesc.Usage = D3D1x_USAGE_DEFAULT;
	tex2dDesc.BindFlags = D3D1x_BIND_DEPTH_STENCIL | D3D1x_BIND_SHADER_RESOURCE;
	tex2dDesc.CPUAccessFlags = 0;
	tex2dDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
 
	V_CHECK( pd3dDevice->CreateTexture2D( &tex2dDesc, NULL, &m_pCubeEnvDepthMap ));

	// Create the depth stencil view for the entire cube
	D3D1x_DEPTH_STENCIL_VIEW_DESC DescDS;
    ZeroMemory( &DescDS, sizeof( DescDS ) );
	DescDS.Format = DXGI_FORMAT_D32_FLOAT;
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
	tex2dDesc.Format = format;
	tex2dDesc.BindFlags = D3D1x_BIND_RENDER_TARGET | D3D1x_BIND_SHADER_RESOURCE;
	tex2dDesc.MiscFlags = D3D1x_RESOURCE_MISC_GENERATE_MIPS | D3D1x_RESOURCE_MISC_TEXTURECUBE;
	tex2dDesc.MipLevels = MIPLEVELS;
 
	V_CHECK(pd3dDevice->CreateTexture2D(&tex2dDesc,NULL,&m_pCubeEnvMap));

	// Create the 6-face render target view
	D3D1x_RENDER_TARGET_VIEW_DESC DescRT;
	DescRT.Format = tex2dDesc.Format;
	DescRT.ViewDimension = D3D1x_RTV_DIMENSION_TEXTURE2DARRAY;
	DescRT.Texture2DArray.FirstArraySlice = 0;
	DescRT.Texture2DArray.ArraySize = 6;
	DescRT.Texture2DArray.MipSlice = 0;
	 
	for(int i=0;i<6;i++)
	{
		DescRT.Texture2DArray.FirstArraySlice = i;
		DescRT.Texture2DArray.ArraySize = 1;
		V_CHECK(pd3dDevice->CreateRenderTargetView(m_pCubeEnvMap, &DescRT, &(m_pCubeEnvMapRTV[i])));
	}

	// Create the shader resource view for the cubic env map
	D3D1x_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format = tex2dDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = MIPLEVELS;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	 
	V_CHECK( pd3dDevice->CreateShaderResourceView(m_pCubeEnvMap, &SRVDesc, &m_pCubeEnvMapSRV ));
	
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format = DescDS.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = MIPLEVELS;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	 
	//V_CHECK( pd3dDevice->CreateShaderResourceView(m_pCubeEnvDepthMap, &SRVDesc, &m_pCubeEnvMapDepthSRV ));
	
}

ID3D11Texture2D* makeStagingTexture(ID3D1xDevice *pd3dDevice,int w,DXGI_FORMAT target_format)
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
	ID3D11Texture2D* tex		=NULL;
	pd3dDevice->CreateTexture2D(&tex2dDesc,NULL,&tex);
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
	sphericalHarmonics.release();
	SAFE_RELEASE(sphericalHarmonicsEffect);
	sphericalSamples.release();
}

void FramebufferCubemapDX1x::SetCurrentFace(int i)
{
	current_face=i;
}

ID3D11Texture2D *FramebufferCubemapDX1x::GetCopy(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
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
		pContext->CopySubresourceRegion(stagingTexture,i, 0, 0, 0, m_pCubeEnvMap,i, &sourceRegion);
	return stagingTexture;
}

void FramebufferCubemapDX1x::CalcSphericalHarmonics(void *context,int bands)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!sphericalHarmonicsEffect)
		CreateEffect(pd3dDevice,&sphericalHarmonicsEffect,"spherical_harmonics.fx");
	{
		// The table of 3D directional sample positions. 16 x 16
		// We just fill this texture with random 3d directions.
		sphericalSamples.RestoreDeviceObjects(pd3dDevice,1024);
		ID3DX11EffectTechnique *jitter=sphericalHarmonicsEffect->GetTechniqueByName("jitter");
		simul::dx11::setUnorderedAccessView(sphericalHarmonicsEffect,"targetBuffer",(ID3D11UnorderedAccessView*)sphericalSamples.unorderedAccessView);
		ApplyPass(pContext,jitter->GetPassByIndex(0));
		pContext->Dispatch(16,16,1);
		simul::dx11::setUnorderedAccessView(sphericalHarmonicsEffect,"targetBuffer",(ID3D11UnorderedAccessView*)NULL);
		ApplyPass(pContext,jitter->GetPassByIndex(0));
	}

	int s=(bands+1);
	if(s<4)
		s=4;
	// The table of coefficients.
	sphericalHarmonics.RestoreDeviceObjects(pd3dDevice,s*s);
	
	ID3DX11EffectTechnique *tech	=sphericalHarmonicsEffect->GetTechniqueByName("encode");
	simul::dx11::setParameter			(sphericalHarmonicsEffect,"cubemapTexture",(ID3D11ShaderResourceView*)m_pCubeEnvMapSRV);
	simul::dx11::setParameter			(sphericalHarmonicsEffect,"targetBuffer",(ID3D11ShaderResourceView*)sphericalHarmonics.unorderedAccessView);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	pContext->Dispatch(16,16,1);
	simul::dx11::setParameter			(sphericalHarmonicsEffect,"cubemapTexture",(ID3D11ShaderResourceView*)NULL);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	sphericalSamples.release();
}

void FramebufferCubemapDX1x::Activate(void *context)
{
	ActivateViewport(context,0.f,0.f,1.f,1.f);
}

void FramebufferCubemapDX1x::ActivateViewport(void *context, float viewportX, float viewportY, float viewportW, float viewportH)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	HRESULT hr=S_OK;
	unsigned int num_v=0;
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		pContext->RSGetViewports(&num_v,m_OldViewports);

	m_pOldRenderTarget	=NULL;
	m_pOldDepthSurface	=NULL;
	pContext->OMGetRenderTargets(	1,
									&m_pOldRenderTarget,
									&m_pOldDepthSurface
									);
	pContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],m_pCubeEnvDepthMapDSV[current_face]);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width = floorf((float)Width*viewportW + 0.5f);
	viewport.Height = floorf((float)Height*viewportH + 0.5f);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = floorf((float)Width*viewportX + 0.5f);
	viewport.TopLeftY = floorf((float)Height*viewportY + 0.5f);

	// Create the viewport.
	pContext->RSSetViewports(1, &viewport);
}

void FramebufferCubemapDX1x::Deactivate(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	// Create the viewport.
	pContext->RSSetViewports(1,m_OldViewports);
}

void FramebufferCubemapDX1x::Clear(void *context,float r,float g,float b,float a,float depth,int mask)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!mask)
		mask=D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL;
	// Clear the screen to black:
    float clearColor[4]={r,g,b,a};
    for(int i=0;i<6;i++)
    {
		pContext->ClearRenderTargetView(m_pCubeEnvMapRTV[i],clearColor);
		if(m_pCubeEnvDepthMapDSV[i])
			pContext->ClearDepthStencilView(m_pCubeEnvDepthMapDSV[i],mask,depth, 0);
	}
}

void FramebufferCubemapDX1x::ClearColour(void *context,float r,float g,float b,float a)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	float clearColor[4]={r,g,b,a};
	for(int i=0;i<6;i++)
	{
		pContext->ClearRenderTargetView(m_pCubeEnvMapRTV[i],clearColor);
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
