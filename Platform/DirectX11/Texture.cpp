#define NOMINMAX
#include "Texture.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"

#include <string>
using namespace simul;
using namespace dx11;

SamplerState::SamplerState()
{
}
SamplerState::~SamplerState()
{
	SAFE_RELEASE(m_pd3D11SamplerState);
}


dx11::Texture::Texture()
	:texture(NULL)
	,shaderResourceView(NULL)
	,unorderedAccessView(NULL)
	,depthStencilView(NULL)
	,unorderedAccessViewMips(NULL)
	,renderTargetView(NULL)
	,stagingBuffer(NULL)
	,last_context(NULL)
	,m_pOldRenderTarget(NULL)
	,m_pOldDepthSurface(NULL)
	,num_OldViewports(0)
{
	memset(&mapped,0,sizeof(mapped));
}


dx11::Texture::~Texture()
{
	InvalidateDeviceObjects();
}

void dx11::Texture::InvalidateDeviceObjects()
{
	if(last_context&&mapped.pData)
	{
		last_context->Unmap(texture,0);
		memset(&mapped,0,sizeof(mapped));
	}
	SAFE_RELEASE(texture);
	SAFE_RELEASE(shaderResourceView);
	SAFE_RELEASE(unorderedAccessView);
	SAFE_RELEASE(depthStencilView);
	SAFE_RELEASE(renderTargetView);
	SAFE_RELEASE(stagingBuffer);
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
}
// Load a texture file
void dx11::Texture::LoadFromFile(crossplatform::RenderPlatform *renderPlatform,const char *pFilePathUtf8)
{
	InvalidateDeviceObjects();
	SAFE_RELEASE(shaderResourceView);
	std::string str(pFilePathUtf8);
	if(str.find(".rgb")<str.length())
	{
		void *f=NULL;
		unsigned int bytes=0;
		std::string str		=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(pFilePathUtf8,dx11::GetTexturePathsUtf8());
		if(!str.length())
			return;
		base::FileLoader::GetFileLoader()->AcquireFileContents(f,bytes,str.c_str(),false);
		char *src=(char*)f+bytes-32768*4;
		ensureTexture2DSizeAndFormat(renderPlatform,256,128,crossplatform::RGBA_8_UNORM,false,false);
		ID3D11DeviceContext *ctx=NULL;
		renderPlatform->AsD3D11Device()->GetImmediateContext(&ctx);

		char *interleaved=new char[bytes];
		char *target=interleaved;
		char *r=src;
		char *g=src+32768;
		char *b=src+2*32768;
		char *a=src+3*32768;
		for(int i=0;i<32768;i++)
		{
			*(target++)=r[i];
			*(target++)=g[i];
			*(target++)=b[i];
			*(target++)=a[i];
		}
		
		crossplatform::DeviceContext deviceContext;
		deviceContext.renderPlatform=renderPlatform;
		deviceContext.platform_context=ctx;
		setTexels(deviceContext,interleaved,0,32768);
		SAFE_RELEASE(ctx)
		base::FileLoader::GetFileLoader()->ReleaseFileContents(f);
		delete [] interleaved;
		return;
	}
	else
	{
		shaderResourceView	=simul::dx11::LoadTexture(renderPlatform->AsD3D11Device(),pFilePathUtf8);
		SetDebugObjectName(texture,pFilePathUtf8);
	}
}

bool dx11::Texture::IsValid() const
{
	return (shaderResourceView!=NULL);
}

void dx11::Texture::copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels)
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

		deviceContext.renderPlatform->AsD3D11Device()->CreateTexture3D(&stagingBufferDesc,NULL,(ID3D11Texture3D**)(&stagingBuffer));
	}
	deviceContext.asD3D11DeviceContext()->CopyResource(stagingBuffer,texture);
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V_CHECK(deviceContext.asD3D11DeviceContext()->Map( stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource));
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
	deviceContext.asD3D11DeviceContext()->Unmap( stagingBuffer, 0);
}

void dx11::Texture::setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)
{
	last_context=deviceContext.asD3D11DeviceContext();
	if(!mapped.pData)
		last_context->Map(texture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
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

void dx11::Texture::init(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT format)
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
	dim=2;
	SAFE_RELEASE(texture);
	pd3dDevice->CreateTexture2D(&textureDesc,0,(ID3D11Texture2D**)&(texture));
	SAFE_RELEASE(shaderResourceView);
	pd3dDevice->CreateShaderResourceView(texture,NULL,&shaderResourceView);
	SAFE_RELEASE(stagingBuffer);
}

void dx11::Texture::InitFromExternalD3D11Texture2D(ID3D11Texture2D *t,ID3D11ShaderResourceView *srv)
{
	if(shaderResourceView)
		SAFE_RELEASE(shaderResourceView);
	texture=t;
	shaderResourceView=srv;
	if(shaderResourceView)
		shaderResourceView->AddRef();
	if(texture)
	{
		texture->AddRef();
		ID3D11Texture2D* ppd(NULL);
		D3D11_TEXTURE2D_DESC textureDesc;
		if(texture->QueryInterface( __uuidof(ID3D11Texture2D),(void**)&ppd)==S_OK)
		{
			ppd->GetDesc(&textureDesc);
			width=textureDesc.Width;
			length=textureDesc.Height;
		}
		SAFE_RELEASE(ppd);
	}
	depth=1;
	dim=2;
}

void dx11::Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int d,crossplatform::PixelFormat pixelFormat,bool computable,int mips)
{
	DXGI_FORMAT f=dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	dim=3;
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
		InvalidateDeviceObjects();
		memset(&textureDesc,0,sizeof(textureDesc));
		textureDesc.Width			=width=w;
		textureDesc.Height			=length=l;
		textureDesc.Depth			=depth=d;
		textureDesc.Format			=format=f;
		textureDesc.MipLevels		=mips;
		textureDesc.Usage			=computable?D3D11_USAGE_DEFAULT:D3D11_USAGE_DYNAMIC;
		textureDesc.BindFlags		=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0);
		textureDesc.CPUAccessFlags	=computable?0:D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags		=0;
		
		V_CHECK(r->AsD3D11Device()->CreateTexture3D(&textureDesc,0,(ID3D11Texture3D**)(&texture)));

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						= f;
		srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Texture3D.MipLevels		= mips;
		srv_desc.Texture3D.MostDetailedMip	= 0;
		V_CHECK(r->AsD3D11Device()->CreateShaderResourceView(texture, &srv_desc,&shaderResourceView));
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
		V_CHECK(r->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &unorderedAccessView));
	}
}

void dx11::Texture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform
												 ,int w,int l
												 ,crossplatform::PixelFormat pixelFormat
												 ,bool computable,bool rendertarget,bool depthstencil
												 ,int num_samples,int aa_quality)
{
	format=(DXGI_FORMAT)dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	DXGI_FORMAT texture2dFormat=format;
	DXGI_FORMAT srvFormat=format;
	if(texture2dFormat==DXGI_FORMAT_D32_FLOAT)
	{
		texture2dFormat	=DXGI_FORMAT_R32_TYPELESS;
		srvFormat		=DXGI_FORMAT_R32_FLOAT;
	}
	if(texture2dFormat==DXGI_FORMAT_D16_UNORM)
	{
		texture2dFormat	=DXGI_FORMAT_R16_TYPELESS;
		srvFormat		=DXGI_FORMAT_R16_UNORM;
	}
	dim=2;
	ID3D11Device *pd3dDevice=renderPlatform->AsD3D11Device();
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
			if(textureDesc.Width!=w||textureDesc.Height!=l||textureDesc.Format!=texture2dFormat)
				ok=false;
			if(computable!=((textureDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)==D3D11_BIND_UNORDERED_ACCESS))
				ok=false;
			if(rendertarget!=((textureDesc.BindFlags&D3D11_BIND_RENDER_TARGET)==D3D11_BIND_RENDER_TARGET))
				ok=false;
		}
		SAFE_RELEASE(ppd);
	}
	else
		ok=false; 
	if(!ok)
	{
		InvalidateDeviceObjects();

		unsigned int numQualityLevels=0;
		while(numQualityLevels==0&&num_samples>1)
		{
			V_CHECK(renderPlatform->AsD3D11Device()->CheckMultisampleQualityLevels(
				texture2dFormat,
				num_samples,
				&numQualityLevels	));
			//if(aa_quality>=numQualityLevels)
			//	aa_quality=numQualityLevels-1;
			if(numQualityLevels==0)
				num_samples/=2;
		};

		memset(&textureDesc,0,sizeof(textureDesc));
		textureDesc.Width					=width=w;
		textureDesc.Height					=length=l;
		depth								=1;
		textureDesc.Format					=texture2dFormat;
		textureDesc.MipLevels				=1;
		textureDesc.ArraySize				=1;
		textureDesc.Usage					=(computable||rendertarget||depthstencil)?D3D11_USAGE_DEFAULT:D3D11_USAGE_DYNAMIC;
		textureDesc.BindFlags				=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0)|(rendertarget?D3D11_BIND_RENDER_TARGET:0)|(depthstencil?D3D11_BIND_DEPTH_STENCIL:0);
		textureDesc.CPUAccessFlags			=(computable||rendertarget||depthstencil)?0:D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags				=rendertarget?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;
		textureDesc.SampleDesc.Count		=num_samples;
		textureDesc.SampleDesc.Quality		=aa_quality;
		
			V_CHECK(pd3dDevice->CreateTexture2D(&textureDesc,0,(ID3D11Texture2D**)(&texture)));

		
		SetDebugObjectName(texture,"dx11::Texture::ensureTexture2DSizeAndFormat");
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						=srvFormat;
		srv_desc.ViewDimension				=num_samples>1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels		=1;
		srv_desc.Texture2D.MostDetailedMip	=0;
		V_CHECK(pd3dDevice->CreateShaderResourceView(texture,&srv_desc,&shaderResourceView));
		SetDebugObjectName(shaderResourceView,"dx11::Texture::ensureTexture2DSizeAndFormat shaderResourceView");

	}
	if(computable&&(!unorderedAccessView||!ok))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format						=texture2dFormat;
		uav_desc.ViewDimension				=D3D11_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice			=0;

		SAFE_RELEASE(unorderedAccessView);
		V_CHECK(pd3dDevice->CreateUnorderedAccessView(texture,&uav_desc,&unorderedAccessView));
		SetDebugObjectName(unorderedAccessView,"dx11::Texture::ensureTexture2DSizeAndFormat unorderedAccessView");
	}
	if(rendertarget&&(!renderTargetView||!ok))
	{
		// Setup the description of the render target view.
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format				=texture2dFormat;
		renderTargetViewDesc.ViewDimension		=num_samples>1?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice	=0;
		// Create the render target in DX11:
		SAFE_RELEASE(renderTargetView);
		V_CHECK(pd3dDevice->CreateRenderTargetView(texture,&renderTargetViewDesc,&renderTargetView));
		SetDebugObjectName(renderTargetView,"dx11::Texture::ensureTexture2DSizeAndFormat renderTargetView");
	}
	if(depthstencil&&(!depthStencilView||!ok))
	{
		D3D11_TEX2D_DSV dsv;
		dsv.MipSlice=0;
		D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc;
		depthDesc.ViewDimension		=num_samples>1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
		depthDesc.Format			=format;
		depthDesc.Flags				=0;
		depthDesc.Texture2D			=dsv;
		V_CHECK(pd3dDevice->CreateDepthStencilView(texture,&depthDesc,&depthStencilView));
	}
}

void dx11::Texture::ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,crossplatform::PixelFormat pixelFormat,bool computable)
{
	DXGI_FORMAT f=dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	dim=1;
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
		InvalidateDeviceObjects();
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

void dx11::Texture::map(ID3D11DeviceContext *context)
{
	if(mapped.pData!=NULL)
		return;
	last_context=context;
	last_context->Map(texture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
}

bool dx11::Texture::isMapped() const
{
	return (mapped.pData!=NULL);
}

void dx11::Texture::unmap()
{
	if(mapped.pData)
		last_context->Unmap(texture,0);
	mapped.pData=NULL;
	last_context=NULL;
}

vec4 dx11::Texture::GetTexel(crossplatform::DeviceContext &deviceContext,vec2 texCoords,bool wrap)
{
	if(!stagingBuffer)
	{
		//Create a "Staging" Resource to actually copy data to-from the GPU buffer. 
		D3D11_TEXTURE2D_DESC desc;
		memset(&desc,0,sizeof(desc));
		desc.Width			=1;
		desc.Height		=1;
		desc.ArraySize = 1;
		desc.Format		=format;
		desc.Usage			=D3D11_USAGE_STAGING;
		desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
		desc.SampleDesc.Count=1;
		desc.SampleDesc.Quality=0;
		ID3D11Texture2D *tex;
		ID3D11Device *dev=deviceContext.renderPlatform->AsD3D11Device();
		deviceContext.renderPlatform->AsD3D11Device()->CreateTexture2D(&desc,NULL,&tex);
		stagingBuffer=tex;
	}
	if(wrap)
	{
	/*	int X=(int)(texCoords.x-0.5f);
		int Y=(int)(texCoords.y-0.5f);
		texCoords.x-=(float)X;
		texCoords.y-=(float)Y;*/
		float u,v;
		texCoords.x=modf(texCoords.x+1.0f,&u);
		texCoords.y=modf(texCoords.y+1.0f,&v);
		if(texCoords.x<0.0f)
			texCoords.x+=1.0f;
		if(texCoords.y<0.0f)
			texCoords.y+=1.0f;
		static bool flip=false;
		if(flip)
			texCoords.y=1.0f-texCoords.y;
	}
	else
	{
		texCoords.x=std::max(0.0f,texCoords.x);
		texCoords.y=std::max(0.0f,texCoords.y);
		texCoords.x=std::min(1.0f,texCoords.x);
		texCoords.y=std::min(1.0f,texCoords.y);
	}
	D3D11_BOX srcBox;
	srcBox.left		=(int)(texCoords.x*width);
	srcBox.top		=(int)(texCoords.y*length);
	
	srcBox.left		=std::max((int)0		,(int)srcBox.left);
	srcBox.top		=std::max((int)0		,(int)srcBox.top);
	srcBox.left		=std::min((int)srcBox.left		,(int)width-1);
	srcBox.top		=std::min((int)srcBox.top		,(int)length-1);
	srcBox.right	=srcBox.left + 1,width-1;
	srcBox.bottom	=srcBox.top + 1,length-1;

	srcBox.back		=1;
	srcBox.front	=0;
	void *pixel;
	if(!texture)
		return vec4(0,0,0,0);
	try
	{
		deviceContext.asD3D11DeviceContext()->CopySubresourceRegion(stagingBuffer, 0, 0, 0, 0, texture, 0, &srcBox);

		D3D11_MAPPED_SUBRESOURCE msr;
		deviceContext.asD3D11DeviceContext()->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &msr);
		pixel = msr.pData;
		// copy data
		deviceContext.asD3D11DeviceContext()->Unmap(stagingBuffer, 0);
	}
	catch(...)
	{
	if(!pixel)
		return vec4(0,0,0,0);
	}
	if(!pixel)
		return vec4(0,0,0,0);
	return vec4((const float*)pixel);
}
void dx11::Texture::activateRenderTarget(crossplatform::DeviceContext &deviceContext)
{
	if(!deviceContext.asD3D11DeviceContext())
		return;
	last_context=deviceContext.asD3D11DeviceContext();
	{
		num_OldViewports=0;
		last_context->RSGetViewports(&num_OldViewports,NULL);
		SIMUL_ASSERT(num_OldViewports>=0&&num_OldViewports<=16)
		if(num_OldViewports>0)
			last_context->RSGetViewports(&num_OldViewports,m_OldViewports);
		SAFE_RELEASE(m_pOldRenderTarget);
		SAFE_RELEASE(m_pOldDepthSurface);
		last_context->OMGetRenderTargets(	1,
											&m_pOldRenderTarget,
											&m_pOldDepthSurface
											);
	}
	SIMUL_ASSERT(renderTargetView!=NULL);
	last_context->OMSetRenderTargets(1,&renderTargetView,NULL);
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
		last_context->RSSetViewports(1, &viewport);
	}
}

void dx11::Texture::deactivateRenderTarget()
{
	if(!last_context)
		return;
	last_context->OMSetRenderTargets(	num_OldViewports,
										&m_pOldRenderTarget,
										m_pOldDepthSurface
										);
	last_context->RSSetViewports(num_OldViewports,m_OldViewports);
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
}

int dx11::Texture::GetSampleCount() const
{
	if(!shaderResourceView)
		return 0;
	bool msaa=false;
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	shaderResourceView->GetDesc(&desc);
	msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	if(!msaa)
		return 0;
	D3D11_TEXTURE2D_DESC t2d_desc;
	((ID3D11Texture2D*)texture)->GetDesc(&t2d_desc);
	return t2d_desc.SampleDesc.Count;
}