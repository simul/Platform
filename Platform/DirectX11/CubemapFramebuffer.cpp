#include "CubemapFramebuffer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "D3dx11effect.h"
#include <assert.h>
const int MIPLEVELS=1;

using namespace simul;
using namespace dx11;

CubemapFramebuffer::CubemapFramebuffer()
	:bands(4)
	,m_pCubeEnvMap(NULL)
	,m_pCubeEnvMapSRV(NULL)
	,Width(0)
	,Height(0)
	,current_face(0)
	,format(DXGI_FORMAT_R8G8B8A8_UNORM)
	,stagingTexture(NULL)
	,sphericalHarmonicsEffect(NULL)
	,pd3dDevice(NULL)
{
	for(int i=0;i<6;i++)
	{
		m_pCubeEnvMapRTV[i]=NULL;
		m_pCubeEnvDepthMap[i]	=NULL;
	}
}

CubemapFramebuffer::~CubemapFramebuffer()
{
	InvalidateDeviceObjects();
}

void CubemapFramebuffer::SetWidthAndHeight(int w,int h)
{
	Width=w;
	Height=w;
	assert(h==w);
}

void CubemapFramebuffer::SetFormat(int f)
{
	DXGI_FORMAT F=(DXGI_FORMAT)f;
	if(F==format)
		return;
	format=F;
}

void CubemapFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform	*r)
{
	renderPlatform=r;
	for(int i=0;i<6;i++)
	{
		SAFE_RELEASE(m_pCubeEnvMapRTV[i]);
		SAFE_DELETE(m_pCubeEnvDepthMap[i]);
		m_pCubeEnvDepthMap[i]=renderPlatform->CreateTexture();
	}
	HRESULT hr=S_OK;
	pd3dDevice=renderPlatform->AsD3D11Device();
	// The table of coefficients.
	int s=(bands+1);
	if(s<4)
		s=4;
	sphericalHarmonics.release();
	sphericalSamples.release();
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
 
	sphericalHarmonicsConstants.RestoreDeviceObjects(pd3dDevice);
	// Create the depth stencil view for the entire cube
	D3D11_DEPTH_STENCIL_VIEW_DESC DescDS;
    ZeroMemory( &DescDS, sizeof( DescDS ) );
	DescDS.Format = DXGI_FORMAT_D32_FLOAT;
	DescDS.ViewDimension				=D3D11_DSV_DIMENSION_TEXTURE2D;
	DescDS.Texture2D.MipSlice			=0;
 
	D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc;
	ZeroMemory(&depthSRVDesc,sizeof(depthSRVDesc));
	depthSRVDesc.Format						=DXGI_FORMAT_R32_FLOAT;
	depthSRVDesc.ViewDimension				=D3D11_SRV_DIMENSION_TEXTURE2D;
	depthSRVDesc.Texture2D.MipLevels		=MIPLEVELS;
	depthSRVDesc.Texture2D.MostDetailedMip	=0;
	//B_RETURN( pd3dDevice->CreateDepthStencilView(m_pCubeEnvDepthMap, &DescDS, &m_pCubeEnvDepthMapDSV ));
	for(int i=0;i<6;i++)
	{
		dx11::Texture *t=(dx11::Texture *)(m_pCubeEnvDepthMap[i]);
		t->width=Width;
		t->length=Height;
		t->depth=1;
		t->dim=2;
		ID3D11Texture2D *texture=NULL;
		pd3dDevice->CreateTexture2D(&tex2dDesc, NULL,&texture);
		t->texture=texture;
		V_CHECK(pd3dDevice->CreateDepthStencilView(texture		,&DescDS		,&t->depthStencilView));
		V_CHECK(pd3dDevice->CreateShaderResourceView(texture	,&depthSRVDesc	,&t->shaderResourceView));
	}
	// Create the cube map for env map render target
	tex2dDesc.Format = format;
	tex2dDesc.BindFlags =D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	tex2dDesc.MiscFlags =D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;
	tex2dDesc.MipLevels = MIPLEVELS;
 
	V_CHECK(pd3dDevice->CreateTexture2D(&tex2dDesc,NULL,&m_pCubeEnvMap));
	// Create the 6-face render target view
	D3D1x_RENDER_TARGET_VIEW_DESC DescRT;
	DescRT.Format = tex2dDesc.Format;
	DescRT.ViewDimension					=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	DescRT.Texture2DArray.FirstArraySlice	= 0;
	DescRT.Texture2DArray.ArraySize = 6;
	DescRT.Texture2DArray.MipSlice = 0;
	 
	for(int i=0;i<6;i++)
	{
		DescRT.Texture2DArray.FirstArraySlice = i;
		DescRT.Texture2DArray.ArraySize = 1;
		V_CHECK(pd3dDevice->CreateRenderTargetView(m_pCubeEnvMap, &DescRT, &(m_pCubeEnvMapRTV[i])));
	}
	// Create the shader resource view for the cubic env map
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format = tex2dDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = MIPLEVELS;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	 
	V_CHECK( pd3dDevice->CreateShaderResourceView(m_pCubeEnvMap, &SRVDesc, &m_pCubeEnvMapSRV ));
	
	// A single face depth texture:

	/*
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format						=DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = MIPLEVELS;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	 
	V_CHECK(pd3dDevice->CreateShaderResourceView(m_pCubeEnvDepthMap,&SRVDesc,&m_pCubeEnvMapDepthSRV));*/
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
	ID3D11Texture2D* tex		=NULL;
	pd3dDevice->CreateTexture2D(&tex2dDesc,NULL,&tex);
	return tex;
}

void CubemapFramebuffer::InvalidateDeviceObjects()
{
	sphericalHarmonicsConstants.InvalidateDeviceObjects();
	SAFE_RELEASE(m_pCubeEnvMap);
	for(int i=0;i<6;i++)
	{
		SAFE_DELETE(m_pCubeEnvDepthMap[i]);
		SAFE_RELEASE(m_pCubeEnvMapRTV[i]);
	}
	SAFE_RELEASE(m_pCubeEnvMapSRV);
	sphericalHarmonics.release();
	SAFE_RELEASE(sphericalHarmonicsEffect);
	sphericalSamples.release();
}

void CubemapFramebuffer::SetCurrentFace(int i)
{
	current_face=i;
}

ID3D11Texture2D *CubemapFramebuffer::GetCopy(void *context)
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

void CubemapFramebuffer::RecompileShaders()
{
	SAFE_RELEASE(sphericalHarmonicsEffect);
	if(!pd3dDevice)
		return;
	CreateEffect(pd3dDevice,&sphericalHarmonicsEffect,"spherical_harmonics.fx");
	sphericalHarmonicsConstants.LinkToEffect(sphericalHarmonicsEffect,"SphericalHarmonicsConstants");
}

void CubemapFramebuffer::CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	if(!sphericalHarmonicsEffect)
		RecompileShaders();
	int num_coefficients=bands*bands;
	static int sqrt_jitter_samples					=16;
	if(!sphericalHarmonics.size)
	{
		sphericalHarmonics.RestoreDeviceObjects(pd3dDevice,num_coefficients,true);
		sphericalSamples.RestoreDeviceObjects(pd3dDevice,sqrt_jitter_samples*sqrt_jitter_samples,true);
	}
	sphericalHarmonicsConstants.num_bands			=bands;
	sphericalHarmonicsConstants.sqrtJitterSamples	=sqrt_jitter_samples;
	sphericalHarmonicsConstants.numJitterSamples	=sqrt_jitter_samples*sqrt_jitter_samples;
	sphericalHarmonicsConstants.invNumJitterSamples	=1.0f/(float)sphericalHarmonicsConstants.numJitterSamples;
	sphericalHarmonicsConstants.Apply(deviceContext);
	simul::dx11::setUnorderedAccessView	(sphericalHarmonicsEffect,"targetBuffer"	,sphericalHarmonics.unorderedAccessView);
	ID3DX11EffectTechnique *clear		=sphericalHarmonicsEffect->GetTechniqueByName("clear");
	ApplyPass(pContext,clear->GetPassByIndex(0));
	pContext->Dispatch(num_coefficients,1,1);
	{
		// The table of 3D directional sample positions. sqrt_jitter_samples x sqrt_jitter_samples
		// We just fill this texture with random 3d directions.
		ID3DX11EffectTechnique *jitter=sphericalHarmonicsEffect->GetTechniqueByName("jitter");
		simul::dx11::setUnorderedAccessView(sphericalHarmonicsEffect,"samplesBufferRW",sphericalSamples.unorderedAccessView);
		ApplyPass(pContext,jitter->GetPassByIndex(0));
		pContext->Dispatch(sqrt_jitter_samples/8,sqrt_jitter_samples/8,1);
		simul::dx11::setUnorderedAccessView(sphericalHarmonicsEffect,"samplesBufferRW",NULL);
		ApplyPass(pContext,jitter->GetPassByIndex(0));
	}

	ID3DX11EffectTechnique *tech		=sphericalHarmonicsEffect->GetTechniqueByName("encode");
	simul::dx11::setTexture				(sphericalHarmonicsEffect,"cubemapTexture"	,m_pCubeEnvMapSRV);
	simul::dx11::setTexture				(sphericalHarmonicsEffect,"samplesBuffer"	,sphericalSamples.shaderResourceView);
	
	static bool sh_by_samples=false;
	ApplyPass(pContext,tech->GetPassByIndex(0));
	pContext->Dispatch(sh_by_samples?sphericalHarmonicsConstants.numJitterSamples:num_coefficients,1,1);
	simul::dx11::setTexture				(sphericalHarmonicsEffect,"cubemapTexture"	,NULL);
	simul::dx11::setUnorderedAccessView	(sphericalHarmonicsEffect,"targetBuffer"	,NULL);
	simul::dx11::setTexture				(sphericalHarmonicsEffect,"samplesBuffer"	,NULL);
	sphericalHarmonicsConstants.Unbind(pContext);
	ApplyPass(pContext,tech->GetPassByIndex(0));
}

void CubemapFramebuffer::Activate(crossplatform::DeviceContext &deviceContext)
{
	ActivateViewport(deviceContext,0.f,0.f,1.f,1.f);
}

void CubemapFramebuffer::ActivateDepth(crossplatform::DeviceContext &deviceContext)
{
}

void CubemapFramebuffer::ActivateColour(crossplatform::DeviceContext &context,const float viewportXYWH[4])
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context.asD3D11DeviceContext();
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
	pContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],NULL);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width = floorf((float)Width*viewportXYWH[2] + 0.5f);
	viewport.Height = floorf((float)Height*viewportXYWH[3] + 0.5f);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = floorf((float)Width*viewportXYWH[0] + 0.5f);
	viewport.TopLeftY = floorf((float)Height*viewportXYWH[1]+ 0.5f);

	// Create the viewport.
	pContext->RSSetViewports(1, &viewport);
}

void CubemapFramebuffer::ActivateViewport(crossplatform::DeviceContext &deviceContext, float viewportX, float viewportY, float viewportW, float viewportH)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
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
	pContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],m_pCubeEnvDepthMap[current_face]->AsD3D11DepthStencilView());
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

void CubemapFramebuffer::Deactivate(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->OMSetRenderTargets(1,&m_pOldRenderTarget,m_pOldDepthSurface);
	SAFE_RELEASE(m_pOldRenderTarget)
	SAFE_RELEASE(m_pOldDepthSurface)
	// Create the viewport.
	pContext->RSSetViewports(1,m_OldViewports);
}

void CubemapFramebuffer::DeactivateDepth(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->OMSetRenderTargets(1,&m_pCubeEnvMapRTV[current_face],NULL);
}

void CubemapFramebuffer::Clear(void *context,float r,float g,float b,float a,float depth,int mask)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	if(!mask)
		mask=D3D1x_CLEAR_DEPTH|D3D1x_CLEAR_STENCIL;
	// Clear the screen to the colour specified:
    float clearColor[4]={r,g,b,a};
    for(int i=0;i<6;i++)
    {
		pContext->ClearRenderTargetView(m_pCubeEnvMapRTV[i],clearColor);
		if(m_pCubeEnvDepthMap[i]->AsD3D11DepthStencilView())
			pContext->ClearDepthStencilView(m_pCubeEnvDepthMap[i]->AsD3D11DepthStencilView(),mask,depth, 0);
	}
}

void CubemapFramebuffer::ClearColour(void *context,float r,float g,float b,float a)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	float clearColor[4]={r,g,b,a};
	for(int i=0;i<6;i++)
	{
		pContext->ClearRenderTargetView(m_pCubeEnvMapRTV[i],clearColor);
	}
}

void CubemapFramebuffer::GetTextureDimensions(const void* tex, unsigned int& widthOut, unsigned int& heightOut) const
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
