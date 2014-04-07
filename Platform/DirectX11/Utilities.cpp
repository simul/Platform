#define NOMINMAX
#include "Utilities.h"
#include "MacrosDX1x.h"
#include "TextRenderer.h"
#include "Simul\Base\StringToWString.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Math/Vector3.h"
#include <d3dx11.h>
#include <algorithm>			// for std::min / max
using namespace simul;
using namespace dx11;

// Stored states
static ID3D11DepthStencilState* m_pDepthStencilStateStored11=NULL;
static ID3D11RasterizerState* m_pRasterizerStateStored11=NULL;
static ID3D11BlendState* m_pBlendStateStored11=NULL;
static ID3D11SamplerState* m_pSamplerStateStored11=NULL;
static UINT m_StencilRefStored11;
static float m_BlendFactorStored11[4];
static UINT m_SampleMaskStored11;

namespace simul
{
	namespace dx11
	{
		TextRenderer textRenderer;
	}
}

TextureStruct::TextureStruct()
	:texture(NULL)
	,shaderResourceView(NULL)
	,unorderedAccessView(NULL)
	,renderTargetView(NULL)
	,stagingBuffer(NULL)
	,width(0)
	,length(0)
	,last_context(NULL)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
{
	memset(&mapped,0,sizeof(mapped));
}

TextureStruct::~TextureStruct()
{
	release();
}

void TextureStruct::release()
{
	if(last_context&&mapped.pData)
	{
		last_context->Unmap(texture,0);
		memset(&mapped,0,sizeof(mapped));
	}
	SAFE_RELEASE(texture);
	SAFE_RELEASE(shaderResourceView);
	SAFE_RELEASE(unorderedAccessView);
	SAFE_RELEASE(renderTargetView);
	SAFE_RELEASE(stagingBuffer);
}

void TextureStruct::copyToMemory(ID3D11Device *pd3dDevice,ID3D11DeviceContext *pContext,void *target,int start_texel,int num_texels)
{
	int byteSize=simul::dx11::ByteSizeOfFormatElement(format);
	if(!stagingBuffer)
	{
		//Create a "Staging" Resource to actually copy data to-from the GPU buffer. 
		D3D11_TEXTURE3D_DESC stagingBufferDesc;

		stagingBufferDesc.Width			=width;
		stagingBufferDesc.Height		=length;
		stagingBufferDesc.Depth			=depth;
		stagingBufferDesc.Format		=format;
		stagingBufferDesc.MipLevels		=1;
		stagingBufferDesc.Usage			=D3D11_USAGE_STAGING;
		stagingBufferDesc.BindFlags		=0;
		stagingBufferDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
		stagingBufferDesc.MiscFlags		=0;

		pd3dDevice->CreateTexture3D(&stagingBufferDesc,NULL,(ID3D11Texture3D**)(&stagingBuffer));
	}
	pContext->CopyResource(stagingBuffer,texture);
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V_CHECK(pContext->Map( stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource));
	unsigned char *source = (unsigned char *)(mappedResource.pData);
	
	int expected_pitch=byteSize*width;
	char *dest=(char*)target;
	if(mappedResource.RowPitch==expected_pitch)
	{
		source+=start_texel*byteSize;
		dest+=start_texel*byteSize;
		memcpy(dest,source,num_texels*byteSize);
	}
	else
	{
		//num_texels=start_texel+num_texels;
		//start_texel=0;
		dest+=start_texel*byteSize;

		int row		=start_texel/width;
		int last_row=(start_texel+num_texels)/width;
		int col		=start_texel-row*width;
		source		+=row*mappedResource.RowPitch;
		if(col>0)
		{
			source		+=col*byteSize;
			int columns	=std::min(num_texels,width-col);
			memcpy(dest,source,columns*byteSize);
			source		+=mappedResource.RowPitch;
			dest		+=columns*byteSize;
			row++;
		}
		for(int r=row;r<last_row;r++)
		{
			memcpy(dest,source,width*byteSize);
			source		+=mappedResource.RowPitch;
			dest		+=width*byteSize;
		}
		int end_columns=start_texel+num_texels-last_row*width;
		if(end_columns>0)
			memcpy(dest,source,end_columns*byteSize);
	}
	pContext->Unmap( stagingBuffer, 0);
}

void TextureStruct::setTexels(ID3D11DeviceContext *context,const void *src,int texel_index,int num_texels)
{
	last_context=context;
	if(!mapped.pData)
		context->Map(texture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
	if(!mapped.pData)
		return;
	int byteSize=simul::dx11::ByteSizeOfFormatElement(format);
	const unsigned char *source=(const unsigned char*)src;
	unsigned char *target=(unsigned char*)mapped.pData;
	int expected_pitch=byteSize*width;
	if(mapped.RowPitch==expected_pitch)
	{
		target+=texel_index*byteSize;
		memcpy(target,source,num_texels*byteSize);
	}
	else
	{
		int block	=mapped.RowPitch/byteSize;
		int row		=texel_index/width;
		int last_row=(texel_index+num_texels)/width;
		int col		=texel_index-row*width;
		target		+=row*block*byteSize;
		source		+=col*byteSize;
		int columns=std::min(num_texels,width-col);
		memcpy(target,source,columns*byteSize);
		source		+=columns*byteSize;
		target		+=block*byteSize;
		for(int r=row+1;r<last_row;r++)
		{
			memcpy(target,source,width*byteSize);
			target		+=block*byteSize;
			source		+=width*byteSize;
		}
		int end_columns=texel_index+num_texels-last_row*width;
		if(end_columns>0)
			memcpy(target,source,end_columns*byteSize);
	}
	if(texel_index+num_texels>=width*length)
	{
		last_context->Unmap(texture,0);
		memset(&mapped,0,sizeof(mapped));
	}
}

void TextureStruct::init(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT format)
{
	D3D11_TEXTURE2D_DESC textureDesc=
	{
		w,l,
		1,1,
		format,
		{1,0}
		,D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0
	};
	width=w;
	length=l;
	SAFE_RELEASE(texture);
	pd3dDevice->CreateTexture2D(&textureDesc,0,(ID3D11Texture2D**)&(texture));
	SAFE_RELEASE(shaderResourceView);
	pd3dDevice->CreateShaderResourceView(texture,NULL,&shaderResourceView);
	SAFE_RELEASE(stagingBuffer);
}

void TextureStruct::ensureTexture3DSizeAndFormat(ID3D11Device *pd3dDevice,int w,int l,int d,DXGI_FORMAT f,bool computable)
{
	D3D11_TEXTURE3D_DESC textureDesc;
	bool ok=true;
	if(texture)
	{
		ID3D11Texture3D* ppd(NULL);
		if(texture->QueryInterface( __uuidof(ID3D11Texture3D),(void**)&ppd)!=S_OK)
			ok=false;
		else
		{
			ppd->GetDesc(&textureDesc);
			if(textureDesc.Width!=w||textureDesc.Height!=l||textureDesc.Depth!=d||textureDesc.Format!=f)
				ok=false;
			if(computable!=((textureDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)==D3D11_BIND_UNORDERED_ACCESS))
				ok=false;
		}
		SAFE_RELEASE(ppd);
	}
	else
		ok=false;
	if(!ok)
	{
		release();
		memset(&textureDesc,0,sizeof(textureDesc));
		textureDesc.Width			=width=w;
		textureDesc.Height			=length=l;
		textureDesc.Depth			=depth=d;
		textureDesc.Format			=format=f;
		textureDesc.MipLevels		=1;
		textureDesc.Usage			=computable?D3D11_USAGE_DEFAULT:D3D11_USAGE_DYNAMIC;
		textureDesc.BindFlags		=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0);
		textureDesc.CPUAccessFlags	=computable?0:D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags		=0;
		
		V_CHECK(pd3dDevice->CreateTexture3D(&textureDesc,0,(ID3D11Texture3D**)(&texture)));

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						= f;
		srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Texture3D.MipLevels		= 1;
		srv_desc.Texture3D.MostDetailedMip	= 0;
		V_CHECK(pd3dDevice->CreateShaderResourceView(texture, &srv_desc,&shaderResourceView));
	}
	if(computable&&(!unorderedAccessView||!ok))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format				= f;
		uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE3D;
		uav_desc.Texture3D.MipSlice	= 0;
		uav_desc.Texture3D.WSize	= d;
		uav_desc.Texture3D.FirstWSlice=0;

		SAFE_RELEASE(unorderedAccessView);
		V_CHECK(pd3dDevice->CreateUnorderedAccessView(texture, &uav_desc, &unorderedAccessView));
	}
}

void TextureStruct::ensureTexture2DSizeAndFormat(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f,bool computable,bool rendertarget)
{
	D3D11_TEXTURE2D_DESC textureDesc;
	bool ok=true;
	if(texture)
	{
		ID3D11Texture2D* ppd(NULL);
		if(texture->QueryInterface( __uuidof(ID3D11Texture2D),(void**)&ppd)!=S_OK)
			ok=false;
		else
		{
			ppd->GetDesc(&textureDesc);
			if(textureDesc.Width!=w||textureDesc.Height!=l||textureDesc.Format!=f)
				ok=false;
			if(computable!=((textureDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)==D3D11_BIND_UNORDERED_ACCESS))
				ok=false;
		}
		SAFE_RELEASE(ppd);
	}
	else
		ok=false;
	if(!ok)
	{
		release();
		memset(&textureDesc,0,sizeof(textureDesc));
		textureDesc.Width				=width=w;
		textureDesc.Height				=length=l;
		depth							=1;
		textureDesc.Format				=format=f;
		textureDesc.MipLevels			=1;
		textureDesc.ArraySize			=1;
		textureDesc.Usage				=(computable||rendertarget)?D3D11_USAGE_DEFAULT:D3D11_USAGE_DYNAMIC;
		textureDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0)|(rendertarget?D3D11_BIND_RENDER_TARGET:0);
		textureDesc.CPUAccessFlags		=(computable||rendertarget)?0:D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags			=rendertarget?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;
		textureDesc.SampleDesc.Count	= 1;
		V_CHECK(pd3dDevice->CreateTexture2D(&textureDesc,0,(ID3D11Texture2D**)(&texture)));
		SetDebugObjectName(texture,"TextureStruct::ensureTexture2DSizeAndFormat");
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						= f;
		srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels		= 1;
		srv_desc.Texture2D.MostDetailedMip	= 0;
		V_CHECK(pd3dDevice->CreateShaderResourceView(texture,&srv_desc,&shaderResourceView));
		SetDebugObjectName(shaderResourceView,"TextureStruct::ensureTexture2DSizeAndFormat shaderResourceView");
	}
	if(computable&&(!unorderedAccessView||!ok))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format				= f;
		uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice	= 0;

		SAFE_RELEASE(unorderedAccessView);
		V_CHECK(pd3dDevice->CreateUnorderedAccessView(texture,&uav_desc,&unorderedAccessView));
		SetDebugObjectName(unorderedAccessView,"TextureStruct::ensureTexture2DSizeAndFormat unorderedAccessView");
	}
	if(rendertarget&&(!renderTargetView||!ok))
	{
		// Setup the description of the render target view.
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format				=f;
		renderTargetViewDesc.ViewDimension		=D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice	=0;
		// Create the render target in DX11:
		V_CHECK(pd3dDevice->CreateRenderTargetView(texture,&renderTargetViewDesc,&renderTargetView));
		SetDebugObjectName(renderTargetView,"TextureStruct::ensureTexture2DSizeAndFormat renderTargetView");
	}
}

void TextureStruct::ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,DXGI_FORMAT f,bool computable)
{
	D3D11_TEXTURE1D_DESC textureDesc;
	bool ok=true;
	if(texture)
	{
		ID3D11Texture1D* ppd(NULL);
		if(texture->QueryInterface( __uuidof(ID3D11Texture1D),(void**)&ppd)!=S_OK)
			ok=false;
		else
		{
			ppd->GetDesc(&textureDesc);
			if(textureDesc.Width!=w||textureDesc.Format!=f)
				ok=false;
			if(computable!=((textureDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)==D3D11_BIND_UNORDERED_ACCESS))
				ok=false;
		}
		SAFE_RELEASE(ppd);
	}
	else
		ok=false;
	if(!ok)
	{
		release();
		memset(&textureDesc,0,sizeof(textureDesc));
		textureDesc.Width			=width=w;
		length						=depth=1;
		textureDesc.Format			=format=f;
		textureDesc.MipLevels		=1;
		textureDesc.ArraySize		=1;
		textureDesc.Usage			=computable?D3D11_USAGE_DEFAULT:D3D11_USAGE_DYNAMIC;
		textureDesc.BindFlags		=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0);
		textureDesc.CPUAccessFlags	=computable?0:D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags		=0;
		
		V_CHECK(pd3dDevice->CreateTexture1D(&textureDesc,0,(ID3D11Texture1D**)(&texture)));

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						= f;
		srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE1D;
		srv_desc.Texture1D.MipLevels		= 1;
		srv_desc.Texture1D.MostDetailedMip	= 0;
		V_CHECK(pd3dDevice->CreateShaderResourceView(texture,&srv_desc,&shaderResourceView));
	}
	if(computable&&(!unorderedAccessView||!ok))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format				= f;
		uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE1D;
		uav_desc.Texture1D.MipSlice	= 0;
		
		SAFE_RELEASE(unorderedAccessView);
		V_CHECK(pd3dDevice->CreateUnorderedAccessView(texture,&uav_desc,&unorderedAccessView));
	}
}

void TextureStruct::map(ID3D11DeviceContext *context)
{
	if(mapped.pData!=NULL)
		return;
	last_context=context;
	last_context->Map(texture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
}

bool TextureStruct::isMapped() const
{
	return (mapped.pData!=NULL);
}

void TextureStruct::unmap()
{
	if(mapped.pData)
		last_context->Unmap(texture,0);
	mapped.pData=NULL;
	last_context=NULL;
}

void TextureStruct::activateRenderTarget(ID3D11DeviceContext *pContext)
{
	if(!pContext)
		return;
	last_context=pContext;
	{
		uint num_v=0;
		pContext->RSGetViewports(&num_v,NULL);
		if(num_v>0)
			pContext->RSGetViewports(&num_v,m_OldViewports);
		m_pOldRenderTarget	=NULL;
		m_pOldDepthSurface	=NULL;
		pContext->OMGetRenderTargets(	1,
										&m_pOldRenderTarget,
										&m_pOldDepthSurface
										);
	}
	pContext->OMSetRenderTargets(1,&renderTargetView,NULL);
	{
		ID3D11Texture2D* ppd(NULL);
		if(texture->QueryInterface( __uuidof(ID3D11Texture2D),(void**)&ppd)!=S_OK)
			return;
		D3D11_TEXTURE2D_DESC textureDesc;
		ppd->GetDesc(&textureDesc);
		SAFE_RELEASE(ppd);
		D3D11_VIEWPORT viewport;
		viewport.Width = (float)textureDesc.Width;
		viewport.Height = (float)textureDesc.Height;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		pContext->RSSetViewports(1, &viewport);
	}
}

void TextureStruct::deactivateRenderTarget()
{
	if(!last_context)
		return;
	last_context->OMSetRenderTargets(	1,
										&m_pOldRenderTarget,
										m_pOldDepthSurface
										);
	last_context->RSSetViewports(1,m_OldViewports);
}

ComputableTexture::ComputableTexture()
	:g_pTex_Output(NULL)
	,g_pUAV_Output(NULL)
	,g_pSRV_Output(NULL)
{
}

ComputableTexture::~ComputableTexture()
{
}

void ComputableTexture::release()
{
	SAFE_RELEASE(g_pTex_Output);
	SAFE_RELEASE(g_pUAV_Output);
	SAFE_RELEASE(g_pSRV_Output);
}

void ComputableTexture::init(ID3D11Device *pd3dDevice,int w,int h)
{
	release();
    D3D11_TEXTURE2D_DESC tex_desc;
	ZeroMemory(&tex_desc, sizeof(D3D11_TEXTURE2D_DESC));
	tex_desc.ArraySize			= 1;
    tex_desc.BindFlags			= D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    tex_desc.Usage				= D3D11_USAGE_DEFAULT;
    tex_desc.Width				= w;
    tex_desc.Height				= h;
    tex_desc.MipLevels			= 1;
    tex_desc.SampleDesc.Count	= 1;
	tex_desc.Format				= DXGI_FORMAT_R32_UINT;
    pd3dDevice->CreateTexture2D(&tex_desc, NULL, &g_pTex_Output);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	uav_desc.Format				= tex_desc.Format;
	uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE2D;
	uav_desc.Texture2D.MipSlice	= 0;
	pd3dDevice->CreateUnorderedAccessView(g_pTex_Output, &uav_desc, &g_pUAV_Output);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    srv_desc.Format						= tex_desc.Format;
    srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels		= 1;
    srv_desc.Texture2D.MostDetailedMip	= 0;
    pd3dDevice->CreateShaderResourceView(g_pTex_Output, &srv_desc, &g_pSRV_Output);
}

/*
create the associated Texture2D resource as a Texture2D array resource, and then create a shader resource view for that resource.

1. Load all of the textures using D3DX10CreateTextureFromFile, with a D3DX10_IMAGE_LOAD_INFO specifying that you want D3D10_USAGE_STAGING.
		all of your textures need to be the same size, have the same format, and have the same number of mip levels
2. Map every mip level of every texture
3. Set up an array of D3D10_SUBRESOURCE_DATA's with number of elements == number of textures * number of mip levels. 
4. For each texture, set the pSysMem member of a D3D10_SUBRESOURCE_DATA to the pointer you got when you mapped each mip level of each texture. Make sure you also set the SysMemPitch to the pitch you got when you mapped that mip level
5. Call CreateTexture2D with the desired array size, and pass the array of D3D10_SUBRESOURCE_DATA's
6. Create a shader resource view for the texture 
*/
void ArrayTexture::create(ID3D11Device *pd3dDevice,const std::vector<std::string> &texture_files)
{
	release();
	std::vector<ID3D11Texture2D *> textures;
	for(unsigned i=0;i<texture_files.size();i++)
	{
		textures.push_back(simul::dx11::LoadStagingTexture(pd3dDevice,texture_files[i].c_str()));
	}
	D3D11_TEXTURE2D_DESC desc;
	D3D11_SUBRESOURCE_DATA *subResources=new D3D11_SUBRESOURCE_DATA[textures.size()];
	ID3D11DeviceContext *pImmediateContext=NULL;
	pd3dDevice->GetImmediateContext(&pImmediateContext);
	for(int i=0;i<(int)textures.size();i++)
	{
		if(!textures[i])
			return;
		textures[i]->GetDesc(&desc);
		D3D11_MAPPED_SUBRESOURCE mapped_res;
		pImmediateContext->Map(textures[i],0,D3D11_MAP_READ,0,&mapped_res);	
		subResources[i].pSysMem			=mapped_res.pData;
		subResources[i].SysMemPitch		=mapped_res.RowPitch;
		subResources[i].SysMemSlicePitch=mapped_res.DepthPitch;
	}
	static int num_mips=5;
	desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
	desc.Usage=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags=0;
	desc.ArraySize=(UINT)textures.size();
	desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.MipLevels=num_mips;
	pd3dDevice->CreateTexture2D(&desc,NULL,&m_pArrayTexture);

	if(m_pArrayTexture)
	for(unsigned i=0;i<textures.size();i++)
	{
		pImmediateContext->UpdateSubresource(m_pArrayTexture,i*num_mips, NULL,subResources[i].pSysMem,subResources[i].SysMemPitch,subResources[i].SysMemSlicePitch);
	}
	pd3dDevice->CreateShaderResourceView(m_pArrayTexture,NULL,&m_pArrayTexture_SRV);
	delete [] subResources;
	for(unsigned i=0;i<textures.size();i++)
	{
		pImmediateContext->Unmap(textures[i],0);
		SAFE_RELEASE(textures[i])
	}
	pImmediateContext->GenerateMips(m_pArrayTexture_SRV);
	SAFE_RELEASE(pImmediateContext)
}

void ArrayTexture::create(ID3D11Device *pd3dDevice,int w,int l,int num,DXGI_FORMAT f,bool computable)
{
	release();
	D3D11_TEXTURE2D_DESC desc;
//	D3D11_SUBRESOURCE_DATA *subResources=new D3D11_SUBRESOURCE_DATA[num];
	//ID3D11DeviceContext *pImmediateContext=NULL;
	//pd3dDevice->GetImmediateContext(&pImmediateContext);
	static int num_mips		=5;
	desc.Width				=w;
	desc.Height				=l;
	desc.Format				=f;
	desc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_RENDER_TARGET ;
	desc.Usage				=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags		=0;
	desc.ArraySize			=num;
	desc.MiscFlags			=D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.MipLevels			=num_mips;
	desc.SampleDesc.Count	=1;
	desc.SampleDesc.Quality	=0;
	V_CHECK(pd3dDevice->CreateTexture2D(&desc,NULL,&m_pArrayTexture));
	V_CHECK(pd3dDevice->CreateShaderResourceView(m_pArrayTexture,NULL,&m_pArrayTexture_SRV));
	V_CHECK(pd3dDevice->CreateUnorderedAccessView(m_pArrayTexture,NULL,&unorderedAccessView));
	//SAFE_RELEASE(pImmediateContext)
}

int UtilityRenderer::instance_count=0;
int UtilityRenderer::screen_width=0;
int UtilityRenderer::screen_height=0;
D3DXMATRIX UtilityRenderer::view,UtilityRenderer::proj;
ID3DX11Effect *UtilityRenderer::m_pDebugEffect=NULL;
ID3D11InputLayout *UtilityRenderer::m_pCubemapVtxDecl=NULL;
ID3D1xBuffer* UtilityRenderer::m_pVertexBuffer=NULL;
ID3D11Device* UtilityRenderer::m_pd3dDevice=NULL;
UtilityRenderer utilityRenderer;

UtilityRenderer::UtilityRenderer()
{
	instance_count++;
}

UtilityRenderer::~UtilityRenderer()
{
	// Now calling this manually instead to avoid global destruction when memory has already been freed.
	//InvalidateDeviceObjects();
}

struct Vertext
{
	D3DXVECTOR4 pos;
	D3DXVECTOR2 tex;
};
struct Vertex3_t
{
	float x,y,z;
};

static const float size=5.f;
static Vertex3_t vertices[36] =
{
	{-size,		-size,	size},
	{size,		size,	size},
	{size,		-size,	size},
	{size,		size,	size},
	{-size,		-size,	size},
	{-size,		size,	size},
	
	{-size,		-size,	-size},
	{ size,		-size,	-size},
	{ size,		 size,	-size},
	{ size,		 size,	-size},
	{-size,		 size,	-size},
	{-size,		-size,	-size},
	
	{-size,		size,	-size},
	{ size,		size,	-size},
	{ size,		size,	 size},
	{ size,		size,	 size},
	{-size,		size,	 size},
	{-size,		size,	-size},
				
	{-size,		-size,  -size},
	{ size,		-size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
	{-size,		-size,  -size},
	{-size,		-size,	 size},
	
	{ size,		-size,	-size},
	{ size,		 size,	 size},
	{ size,		 size,	-size},
	{ size,		 size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
				
	{-size,		-size,	-size},
	{-size,		 size,	-size},
	{-size,		 size,	 size},
	{-size,		 size,	 size},
	{-size,		-size,	 size},
	{-size,		-size,	-size},
};

void UtilityRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D11Device *)dev;
	RecompileShaders();
	textRenderer.RestoreDeviceObjects(m_pd3dDevice);
	SAFE_RELEASE(m_pVertexBuffer);
	// Vertex declaration
	{
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("vec3_input_signature");
		tech->GetPassByIndex(0)->GetDesc(&PassDesc);
		D3D1x_INPUT_ELEMENT_DESC decl[]=
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
		};
		SAFE_RELEASE(m_pCubemapVtxDecl);
		V_CHECK(m_pd3dDevice->CreateInputLayout(decl,1,PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pCubemapVtxDecl));
	}
	D3D1x_BUFFER_DESC desc=
	{
        36*sizeof(vec3),
        D3D1x_USAGE_DEFAULT,
        D3D1x_BIND_VERTEX_BUFFER,
        0,
        0
	};
	
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem		=vertices;
    InitData.SysMemPitch	=sizeof(vec3);
	V_CHECK(m_pd3dDevice->CreateBuffer(&desc,&InitData,&m_pVertexBuffer));
}

void UtilityRenderer::RecompileShaders()
{
	SAFE_RELEASE(m_pDebugEffect);
	CreateEffect(m_pd3dDevice,&m_pDebugEffect,"simul_debug.fx");
	textRenderer.RecompileShaders();
}

void UtilityRenderer::InvalidateDeviceObjects()
{
	textRenderer.InvalidateDeviceObjects();
	SAFE_RELEASE(m_pCubemapVtxDecl);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pDebugEffect);
    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}

ID3DX11Effect		*UtilityRenderer::GetDebugEffect()
{
	return m_pDebugEffect;
}

void UtilityRenderer::SetMatrices(D3DXMATRIX v,D3DXMATRIX p)
{
	view=v;
	proj=p;
}

void UtilityRenderer::SetScreenSize(int w,int h)
{
	screen_width=w;
	screen_height=h;
}

void UtilityRenderer::GetScreenSize(int& w,int& h)
{
	w=screen_width;
	h=screen_height;
}

void UtilityRenderer::Print(ID3D11DeviceContext* pd3dImmediateContext,int x,int y,const char *text,const float *clr)
{
	UtilityRenderer::Print(pd3dImmediateContext,(float)x,(float)y,text,clr);
}

void UtilityRenderer::Print(ID3D11DeviceContext* pd3dImmediateContext,float x,float y,const char *text,const float *clr)
{
	textRenderer.Render(pd3dImmediateContext,x,y,(float)screen_width,(float)screen_height,text,clr);
}

void UtilityRenderer::PrintAt3dPos(ID3D11DeviceContext* pd3dImmediateContext,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
}

void UtilityRenderer::DrawLines(ID3D11DeviceContext* m_pImmediateContext,VertexXyzRgba *vertices,int vertex_count,bool strip)
{
	if(!vertex_count)
		return;
	PIXWrapper(0xFF0000FF,"DrawLines")
	{
		HRESULT hr=S_OK;
		D3DXMATRIX world, tmp1, tmp2;
		D3DXMatrixIdentity(&world);
		ID3DX11EffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("simul_debug");
		ID3D1xEffectMatrixVariable*	worldViewProj=m_pDebugEffect->GetVariableByName("worldViewProj")->AsMatrix();

		D3DXMATRIX wvp;
		camera::MakeWorldViewProjMatrix((float*)&wvp,world,view,proj);
		worldViewProj->SetMatrix(&wvp._11);
	
		ID3D1xBuffer *					vertexBuffer=NULL;
		// Create the vertex buffer:
		D3D1x_BUFFER_DESC desc=
		{
			vertex_count*sizeof(VertexXyzRgba),
			D3D1x_USAGE_DYNAMIC,
			D3D1x_BIND_VERTEX_BUFFER,
			D3D1x_CPU_ACCESS_WRITE,
			0
		};
		D3D1x_SUBRESOURCE_DATA InitData;
		ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
		InitData.pSysMem = vertices;
		InitData.SysMemPitch = sizeof(VertexXyzRgba);
		hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D1x_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3D1x_PASS_DESC PassDesc;
		ID3D1xEffectPass *pass=tech->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		ID3D1xInputLayout*				m_pVtxDecl=NULL;
		SAFE_RELEASE(m_pVtxDecl);
		hr=m_pd3dDevice->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
		ID3D11InputLayout* previousInputLayout;
		m_pImmediateContext->IAGetInputLayout( &previousInputLayout );
		D3D10_PRIMITIVE_TOPOLOGY previousTopology;
		m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
		m_pImmediateContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(VertexXyzRgba);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		m_pImmediateContext->IASetVertexBuffers(	0,				// the first input slot for binding
													1,				// the number of buffers in the array
													&vertexBuffer,	// the array of vertex buffers
													&stride,		// array of stride values, one for each buffer
													&offset);		// array of 
		hr=ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
		m_pImmediateContext->Draw(vertex_count,0);
		m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
		m_pImmediateContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
		SAFE_RELEASE(m_pVtxDecl);
	}
}

void UtilityRenderer::DrawTexture(ID3D11DeviceContext *pContext,int x1,int y1,int dx,int dy,ID3D11ShaderResourceView *t,float mult)
{
	if(!t)
		return;
	simul::dx11::setTexture(m_pDebugEffect,"imageTexture",t);
	simul::dx11::setParameter(m_pDebugEffect,"multiplier",mult);
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	t->GetDesc(&desc);
	bool msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	if(msaa)
		DrawTextureMS(pContext,x1,y1,dx,dy,t,mult);
	else if(m_pDebugEffect)
		UtilityRenderer::DrawQuad2(pContext,x1,y1,dx,dy,m_pDebugEffect,m_pDebugEffect->GetTechniqueByName("textured"));
	simul::dx11::setTexture(m_pDebugEffect,"imageTexture",NULL);
}

void UtilityRenderer::DrawTextureMS(ID3D11DeviceContext *pContext,int x1,int y1,int dx,int dy,ID3D11ShaderResourceView *t,float brightnessMultiplier)
{
	simul::dx11::setTexture(m_pDebugEffect,"imageTextureMS",t);
	//simul::dx11::setParameter(m_pDebugEffect,"textureMultiplier",brightnessMultiplier);
	simul::dx11::setParameter(m_pDebugEffect,"multiplier",brightnessMultiplier);
	if(m_pDebugEffect)
		UtilityRenderer::DrawQuad2(pContext,x1,y1,dx,dy,m_pDebugEffect,m_pDebugEffect->GetTechniqueByName("texturedMS"));
	simul::dx11::setTexture(m_pDebugEffect,"imageTextureMS",NULL);
}

void UtilityRenderer::DrawQuad(ID3D11DeviceContext *m_pImmediateContext)
{
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pImmediateContext->IASetInputLayout(NULL);
	m_pImmediateContext->Draw(4,0);
	m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
}			

void UtilityRenderer::DrawQuad2(ID3D11DeviceContext *m_pImmediateContext,int x1,int y1,int dx,int dy,ID3DX11Effect* eff,ID3DX11EffectTechnique* tech)
{
	DrawQuad2(m_pImmediateContext
		,2.f*(float)x1/(float)screen_width-1.f
		,1.f-2.f*(float)(y1+dy)/(float)screen_height
		,2.f*(float)dx/(float)screen_width
		,2.f*(float)dy/(float)screen_height
		,eff,tech);
}

void UtilityRenderer::DrawQuad2(ID3D11DeviceContext *m_pImmediateContext,float x1,float y1,float dx,float dy,ID3DX11Effect* eff,ID3DX11EffectTechnique* tech)
{
	HRESULT hr=S_OK;
	setParameter(eff,"rect",x1,y1,dx,dy);
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
	m_pImmediateContext->Draw(4,0);
	m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
}

void UtilityRenderer::RenderAngledQuad(ID3D11DeviceContext *pImmediateContext
									   ,const float *dr
									   ,float half_angle_radians
										,ID3DX11Effect* effect
										,ID3DX11EffectTechnique* tech
										,D3DXMATRIX view
										,D3DXMATRIX proj
										,D3DXVECTOR3 sun_dir)
{
	// If y is vertical, we have LEFT-HANDED rotations, otherwise right.
	// But D3DXMatrixRotationYawPitchRoll uses only left-handed, hence the change of sign below.
	if(effect)
	{
//		setMatrix(effect,"worldViewProj",tmp1);
		//setParameter(effect,"lightDir",sun2);
	//	setParameter(effect,"radiusRadians",half_angle_radians);
	}
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ApplyPass(pImmediateContext,tech->GetPassByIndex(0));
	pImmediateContext->Draw(4,0);
	pImmediateContext->IASetPrimitiveTopology(previousTopology);
}


void UtilityRenderer::DrawCube(void *context)
{
	UINT stride = sizeof(vec3);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout( &previousInputLayout );

	pContext->IASetVertexBuffers(	0,					// the first input slot for binding
									1,					// the number of buffers in the array
									&m_pVertexBuffer,	// the array of vertex buffers
									&stride,			// array of stride values, one for each buffer
									&offset );			// array of offset values, one for each buffer

	// Set the input layout
	pContext->IASetInputLayout(m_pCubemapVtxDecl);

	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pContext->Draw(36,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
}

void UtilityRenderer::DrawSphere(void *context,int latitudes,int longitudes)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	// The number of indices per lat strip is (longitudes+1)*2.
	// The number of lat strips is (latitudes+1)
	pContext->Draw((longitudes+1)*(latitudes+1)*2,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void UtilityRenderer::DrawCubemap(void *context,ID3D1xShaderResourceView *m_pCubeEnvMapSRV,D3DXMATRIX view,D3DXMATRIX proj,float offsetx,float offsety)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	unsigned int num_v=0;
	D3D11_VIEWPORT								m_OldViewports[4];
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		pContext->RSGetViewports(&num_v,m_OldViewports);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width		=m_OldViewports[0].Width*0.3f;
	viewport.Height		=m_OldViewports[0].Height*0.3f;
	viewport.MinDepth	=0.0f;
	viewport.MaxDepth	=1.0f;
	viewport.TopLeftX	=0.5f*(1.f+offsetx)*m_OldViewports[0].Width-viewport.Width/2;
	viewport.TopLeftY	=0.5f*(1.f-offsety)*m_OldViewports[0].Height-viewport.Height/2;
	pContext->RSSetViewports(1,&viewport);

	// Create the viewport.
	D3DXMATRIX wvp,world;
	D3DXMatrixIdentity(&world);
	float tan_x=1.0f/proj(0, 0);
	float tan_y=1.0f/proj(1, 1);
	float size_req=tan_x*.5f;
	static float size=3.f;
	float d=2.0f*size/size_req;
	//simul::math::Vector3 offs0(offsetx*(tan_x-size_req)*d,offsety*(tan_y-size_req)*d,-d);
	simul::math::Vector3 offs0(0,0,-d);
	simul::math::Vector3 offs;
	Multiply3(offs,*((const simul::math::Matrix4x4*)(const float*)view),offs0);

	world._41=offs.x;
	world._42=offs.y;
	world._43=offs.z;
	view._41=0;
	view._42=0;
	view._43=0;
	camera::MakeWorldViewProjMatrix((float*)&wvp,world,view,proj);
	simul::dx11::setMatrix(m_pDebugEffect,"worldViewProj",&wvp._11);
	//ID3DX11EffectTechnique*			tech		=m_pDebugEffect->GetTechniqueByName("draw_cubemap");
	ID3DX11EffectTechnique*				tech		=m_pDebugEffect->GetTechniqueByName("draw_cubemap_sphere");
	ID3D1xEffectShaderResourceVariable*	cubeTexture	=m_pDebugEffect->GetVariableByName("cubeTexture")->AsShaderResource();
	cubeTexture->SetResource(m_pCubeEnvMapSRV);
	HRESULT hr=ApplyPass(pContext,tech->GetPassByIndex(0));
	simul::dx11::setParameter(m_pDebugEffect,"latitudes",16);
	simul::dx11::setParameter(m_pDebugEffect,"longitudes",32);
	static float rr=6.f;
	simul::dx11::setParameter(m_pDebugEffect,"radius",rr);
	UtilityRenderer::DrawSphere(context,16,32);
	pContext->RSSetViewports(num_v,m_OldViewports);
}


void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMGetDepthStencilState( &m_pDepthStencilStateStored11, &m_StencilRefStored11 );
	SetDebugObjectName(m_pDepthStencilStateStored11,"m_pDepthStencilStateStored11");
    pd3dImmediateContext->RSGetState( &m_pRasterizerStateStored11 );
	SetDebugObjectName(m_pRasterizerStateStored11,"m_pRasterizerStateStored11");
    pd3dImmediateContext->OMGetBlendState( &m_pBlendStateStored11, m_BlendFactorStored11, &m_SampleMaskStored11 );
	SetDebugObjectName(m_pBlendStateStored11,"m_pBlendStateStored11");
    pd3dImmediateContext->PSGetSamplers( 0, 1, &m_pSamplerStateStored11 );
	SetDebugObjectName(m_pSamplerStateStored11,"m_pSamplerStateStored11");
}

void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilStateStored11, m_StencilRefStored11 );
    pd3dImmediateContext->RSSetState( m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMSetBlendState( m_pBlendStateStored11, m_BlendFactorStored11, m_SampleMaskStored11 );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSamplerStateStored11 );

    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}
