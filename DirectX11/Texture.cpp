
#include "Texture.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/Framebuffer.h"
#include <string>
#include <math.h>
#include <algorithm>

using namespace platform;
using namespace dx11;

#pragma optimize("",off)

SamplerState::SamplerState(crossplatform::SamplerStateDesc *)
	:m_pd3D11SamplerState(NULL)
{
}

SamplerState::~SamplerState()
{
	InvalidateDeviceObjects();
}

void SamplerState::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pd3D11SamplerState);
}

void DepthFormatToResourceAndSrvFormats(DXGI_FORMAT& texture2dFormat, DXGI_FORMAT& srvFormat);

Texture::Texture()
	:stagingBuffer(NULL)
	,last_context(NULL)
	,texture(NULL)
	,external_copy_source(nullptr)
{
	memset(&mapped,0,sizeof(mapped));
}


Texture::~Texture()
{
	InvalidateDeviceObjects();
}

void Texture::FreeRTVTables()
{
	for (auto& rtv : renderTargetViews)
		SAFE_RELEASE(rtv.second);
	renderTargetViews.clear();
}

void Texture::FreeDSVTables()
{
	for (auto& dsv : depthStencilViews)
		SAFE_RELEASE(dsv.second);
	depthStencilViews.clear();
}

void Texture::FreeUAVTables()
{
	for (auto& uav : unorderedAccessViews)
		SAFE_RELEASE(uav.second);
	unorderedAccessViews.clear();
}

void Texture::FreeSRVTables()
{
	for (auto& srv : shaderResourceViews)
		SAFE_RELEASE(srv.second);
	shaderResourceViews.clear();
}

void Texture::InvalidateDeviceObjects()
{
	FreeRTVTables();
	FreeDSVTables();
	FreeUAVTables();
	FreeSRVTables();
	if(last_context&&mapped.pData)
	{
		last_context->Unmap(texture,0);
		memset(&mapped,0,sizeof(mapped));
	}
	if(renderPlatform&&renderPlatform->GetMemoryInterface())
	{
		if(!external_texture)
			renderPlatform->GetMemoryInterface()->UntrackVideoMemory(texture);
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(stagingBuffer);
	}
	SAFE_RELEASE(texture);
	SAFE_RELEASE(stagingBuffer);
	SAFE_RELEASE(external_copy_source);
	textureUploadComplete=true;
	external_texture=false;
	arraySize=0;
	mips=0;
}

// Load a texture file
void Texture::LoadFromFile(crossplatform::RenderPlatform *renderPlatform,const char *pFilePathUtf8, bool gen_mips)
{
	ERRNO_BREAK
	const std::vector<std::string> &pathsUtf8=renderPlatform->GetTexturePathsUtf8();
	InvalidateDeviceObjects();
	ID3D11Texture2D *t	=platform::dx11::LoadTexture(renderPlatform->AsD3D11Device(),pFilePathUtf8,pathsUtf8);

	InitFromExternalTexture2D(renderPlatform, t, 0, 0, crossplatform::PixelFormat::UNKNOWN);
	if(renderPlatform&&renderPlatform->GetMemoryInterface()&&t)
	{
		renderPlatform->GetMemoryInterface()->TrackVideoMemory(t,width*length*4,name.c_str());
	}
	SAFE_RELEASE(t);
	external_texture = false;
	SetDebugObjectName(texture,pFilePathUtf8);
}

int Texture::GetMemorySize() const
{
	int w=width,l=length,d=depth;
	int mem=0;
	for(int i=0;i<mips;i++)
	{
		mem+=w*l*d*arraySize;
		w/=2;
		l/=2;
		d/=2;
	}
	return mem*ByteSizeOfFormatElement(dxgi_format);
}

void Texture::LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files, bool gen_mips)
{
	renderPlatform=r;
	const std::vector<std::string> &pathsUtf8=r->GetTexturePathsUtf8();
	InvalidateDeviceObjects();
	std::vector<ID3D11Texture2D *> textures;
	for(unsigned i=0;i<texture_files.size();i++)
	{
		textures.push_back(platform::dx11::LoadStagingTexture(r->AsD3D11Device(),texture_files[i].c_str(),pathsUtf8));
	}
	D3D11_TEXTURE2D_DESC desc;
	ID3D11DeviceContext *pContext=NULL;
	r->AsD3D11Device()->GetImmediateContext(&pContext);
	int w=1,l=1;
	bool ok=true;
	for(int i=0;i<(int)textures.size();i++)
	{
		if(!textures[i])
		{
			ok=false;
			continue;
		}
		textures[i]->GetDesc(&desc);
		w=desc.Width;
		l=desc.Height;
	}
	if(ok)
	{
		int m = 1;
		if (gen_mips)
			m = 100;
		m = std::min(m, int(floor(log2(std::max(w, l)))) + 1);
		m = std::min(16, std::max(1, m));
		if (m < 0 || m>16)
			m = 1;
		auto format = RenderPlatform::FromDxgiFormat(desc.Format);

		ensureTextureArraySizeAndFormat(r,desc.Width,desc.Height,(int)textures.size(),m,format,nullptr,false,true);
	
		external_texture=false;
		//if(renderPlatform->GetMemoryInterface())
		//	renderPlatform->GetMemoryInterface()->TrackVideoMemory(texture,GetMemorySize(),name.c_str());
		if(texture)
		for(unsigned i=0;i<textures.size();i++)
		{
			// Copy the resource directly, no CPU mapping
			pContext->CopySubresourceRegion(
							texture
							,i*m
							,0
							,0
							,0
							,textures[i]
							,0
							,NULL
							);
		}
		//void FreeSRVTables();
		//void FreeRTVTables();
		//InitRTVTables(texture_files.size(),m);
		//V_CHECK(r->AsD3D11Device()->CreateShaderResourceView(tex,NULL,&mainShaderResourceView));
	}
	for(unsigned i=0;i<textures.size();i++)
	{
		SAFE_RELEASE(textures[i])
	}
	//pContext->GenerateMips(mainShaderResourceView);
	SAFE_RELEASE(pContext)
	//mips=m;
	//arraySize=(int)texture_files.size();
}

bool Texture::IsValid() const
{
	return (texture!=NULL);
}

ID3D11ShaderResourceView* Texture::AsD3D11ShaderResourceView(const crossplatform::TextureView& textureView)
{
	if (!ValidateTextureView(textureView))
		return nullptr;

	ID3D11ShaderResourceView* srv = nullptr;
	uint64_t hash = textureView.GetHash();
	if (shaderResourceViews.find(hash) != shaderResourceViews.end())
		return shaderResourceViews[hash];

	const UINT& baseMipLevel = textureView.subresourceRange.baseMipLevel;
	const UINT& mipLevelCount = textureView.subresourceRange.mipLevelCount == -1 ? mips - baseMipLevel : textureView.subresourceRange.mipLevelCount;
	const UINT& baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	const UINT& arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() - baseArrayLayer : textureView.subresourceRange.arrayLayerCount;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	DXGI_FORMAT srvFormat = TypelessToSrvFormat(dxgi_format);
	DepthFormatToResourceAndSrvFormats(srvFormat, srvDesc.Format);

	if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		srvDesc.Texture1D.MostDetailedMip = baseMipLevel;
		srvDesc.Texture1D.MipLevels = mipLevelCount;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D_ARRAY)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
		srvDesc.Texture1DArray.MostDetailedMip = baseMipLevel;
		srvDesc.Texture1DArray.MipLevels = mipLevelCount;
		srvDesc.Texture1DArray.FirstArraySlice = baseArrayLayer;
		srvDesc.Texture1DArray.ArraySize = arrayLayerCount;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = baseMipLevel;
		srvDesc.Texture2D.MipLevels = mipLevelCount;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = baseMipLevel;
		srvDesc.Texture2DArray.MipLevels = mipLevelCount;
		srvDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
		srvDesc.Texture2DArray.ArraySize = arrayLayerCount;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS_ARRAY)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
		srvDesc.Texture2DMSArray.FirstArraySlice = baseArrayLayer;
		srvDesc.Texture2DMSArray.ArraySize = arrayLayerCount;

	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_3D)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MostDetailedMip = baseMipLevel;
		srvDesc.Texture3D.MipLevels = mipLevelCount;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_CUBE)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = baseMipLevel;
		srvDesc.TextureCube.MipLevels = mipLevelCount;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		srvDesc.TextureCubeArray.MostDetailedMip = baseMipLevel;
		srvDesc.TextureCubeArray.MipLevels = mipLevelCount;
		srvDesc.TextureCubeArray.First2DArrayFace = 0;
		srvDesc.TextureCubeArray.NumCubes = NumFaces() / 6;
	}
	else
	{
		SIMUL_BREAK_ONCE("Unsupported crossplatform::ShaderResourceType.");
	}
	V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture, &srvDesc, &srv));
	shaderResourceViews[hash] = srv;
	return srv;
}

ID3D11UnorderedAccessView* Texture::AsD3D11UnorderedAccessView(const crossplatform::TextureView& textureView)
{
	if (!ValidateTextureView(textureView))
		return nullptr;

	if (!computable)
	{
		SIMUL_BREAK_ONCE("Texture doesn't support UnorderedAccessViews.");
		return nullptr;
	}

	ID3D11UnorderedAccessView* uav = nullptr;
	uint64_t hash = textureView.GetHash();
	if (unorderedAccessViews.find(hash) != unorderedAccessViews.end())
		return unorderedAccessViews[hash];

	const UINT& mipLevel = textureView.subresourceRange.baseMipLevel;
	const UINT& baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	const UINT& arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() - baseArrayLayer : textureView.subresourceRange.arrayLayerCount;

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Format = TypelessToSrvFormat(dxgi_format);
	if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_1D)
	{
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		uavDesc.Texture1D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_1D_ARRAY)
	{
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
		uavDesc.Texture1DArray.FirstArraySlice = baseArrayLayer;
		uavDesc.Texture1DArray.ArraySize = arrayLayerCount;
		uavDesc.Texture1DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_2D)
	{
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_2D_ARRAY)
	{
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
		uavDesc.Texture2DArray.ArraySize = arrayLayerCount;
		uavDesc.Texture2DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_3D)
	{
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = depth >> mipLevel;
		uavDesc.Texture3D.MipSlice = mipLevel;
	}
	else
	{
		SIMUL_BREAK_ONCE("Unsupported crossplatform::ShaderResourceType.");
	}
	V_CHECK(renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(texture, &uavDesc, &uav));
	unorderedAccessViews[hash] = uav;
	return uav;
}

ID3D11DepthStencilView* Texture::AsD3D11DepthStencilView(const crossplatform::TextureView& textureView)
{
	if (!ValidateTextureView(textureView))
		return nullptr;

	if (!depthStencil)
	{
		SIMUL_BREAK_ONCE("Texture doesn't support DepthStencilViews.");
		return nullptr;
	}

	ID3D11DepthStencilView* dsv = nullptr;
	uint64_t hash = textureView.GetHash();
	if (depthStencilViews.find(hash) != depthStencilViews.end())
		return depthStencilViews[hash];

	const UINT& mipLevel = textureView.subresourceRange.baseMipLevel;
	const UINT& baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	const UINT& arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() - baseArrayLayer : textureView.subresourceRange.arrayLayerCount;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = ResourceToDsvFormat(dxgi_format);
	dsvDesc.Flags = 0;
	if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D)
	{
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
		dsvDesc.Texture1D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D_ARRAY)
	{
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
		dsvDesc.Texture1DArray.FirstArraySlice = baseArrayLayer;
		dsvDesc.Texture1DArray.ArraySize = arrayLayerCount;
		dsvDesc.Texture1DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D)
	{
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
	{
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
		dsvDesc.Texture2DArray.ArraySize = arrayLayerCount;
		dsvDesc.Texture2DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS)
	{
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS_ARRAY)
	{
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		dsvDesc.Texture2DMSArray.FirstArraySlice = baseArrayLayer;
		dsvDesc.Texture2DMSArray.ArraySize = arrayLayerCount;
	}
	else
	{
		SIMUL_BREAK_ONCE("Unknown crossplatform::ShaderResourceType.");
	}
	V_CHECK(renderPlatform->AsD3D11Device()->CreateDepthStencilView(texture, &dsvDesc, &dsv));
	depthStencilViews[hash] = dsv;
	return dsv;
}

ID3D11RenderTargetView* Texture::AsD3D11RenderTargetView(const crossplatform::TextureView& textureView)
{
	if (!ValidateTextureView(textureView))
		return nullptr;

	if (!renderTarget)
	{
		SIMUL_BREAK_ONCE("Texture doesn't support RenderTargetViews.");
		return nullptr;
	}

	ID3D11RenderTargetView* rtv = nullptr;
	uint64_t hash = textureView.GetHash();
	if (renderTargetViews.find(hash) != renderTargetViews.end())
		return renderTargetViews[hash];

	const UINT& mipLevel = textureView.subresourceRange.baseMipLevel;
	const UINT& baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	const UINT& arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() - baseArrayLayer : textureView.subresourceRange.arrayLayerCount;

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = TypelessToSrvFormat(dxgi_format);
	if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D)
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
		rtvDesc.Texture1D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D_ARRAY)
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
		rtvDesc.Texture1DArray.FirstArraySlice = baseArrayLayer;
		rtvDesc.Texture1DArray.ArraySize = arrayLayerCount;
		rtvDesc.Texture1DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D)
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
		rtvDesc.Texture2DArray.ArraySize = arrayLayerCount;
		rtvDesc.Texture2DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS)
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS_ARRAY)
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		rtvDesc.Texture2DMSArray.FirstArraySlice = baseArrayLayer;
		rtvDesc.Texture2DMSArray.ArraySize = arrayLayerCount;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_3D)
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		rtvDesc.Texture3D.FirstWSlice = 0;
		rtvDesc.Texture3D.WSize = depth >> mipLevel;
		rtvDesc.Texture3D.MipSlice = mipLevel;
	}
	else
	{
		SIMUL_BREAK_ONCE("Unknown crossplatform::ShaderResourceType.");
	}
	V_CHECK(renderPlatform->AsD3D11Device()->CreateRenderTargetView(texture, &rtvDesc, &rtv));
	renderTargetViews[hash] = rtv;
	return rtv;
}

void Texture::copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels)
{
	int byteSize=platform::dx11::ByteSizeOfFormatElement(dxgi_format);
	if(!stagingBuffer)
	{
		//Create a "Staging" Resource to actually copy data to-from the GPU buffer. 
		D3D11_TEXTURE3D_DESC stagingBufferDesc;

		stagingBufferDesc.Width			=width;
		stagingBufferDesc.Height		=length;
		stagingBufferDesc.Depth			=depth;
		stagingBufferDesc.Format		=dxgi_format;
		stagingBufferDesc.MipLevels		=1;
		stagingBufferDesc.Usage			=D3D11_USAGE_STAGING;
		stagingBufferDesc.BindFlags		=0;
		stagingBufferDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
		stagingBufferDesc.MiscFlags		=0;

		deviceContext.renderPlatform->AsD3D11Device()->CreateTexture3D(&stagingBufferDesc,NULL,(ID3D11Texture3D**)(&stagingBuffer));
		if(renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(stagingBuffer,GetMemorySize(),name.c_str());
	}
	deviceContext.asD3D11DeviceContext()->CopyResource(stagingBuffer,texture);
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V_CHECK(deviceContext.asD3D11DeviceContext()->Map( stagingBuffer, 0, D3D11_MAP_READ, SIMUL_D3D11_MAP_FLAGS, &mappedResource));
	unsigned char *source = (unsigned char *)(mappedResource.pData);
	
	int expected_pitch=byteSize*width;
	int expected_depth_pitch=expected_pitch*length;
	char *dest=(char*)target;
	if(mappedResource.RowPitch==(UINT)expected_pitch&&mappedResource.DepthPitch==(UINT)expected_depth_pitch)
	{
		source+=start_texel*byteSize;
		dest+=start_texel*byteSize;
		memcpy(dest,source,num_texels*byteSize);
	}
	else
	{
		for(int z=0;z<depth;z++)
		{
			unsigned char *s=source;
			for(int y=0;y<length;y++)
			{
				memcpy(dest,source,width*byteSize);
				source		+=mappedResource.RowPitch;
				dest		+=width*byteSize;
			}
			source=s;
			source		+=mappedResource.DepthPitch;
		}
	}
	deviceContext.asD3D11DeviceContext()->Unmap( stagingBuffer, 0);
}

void Texture::setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)
{
	last_context=deviceContext.asD3D11DeviceContext();
#ifdef _XBOX_ONE
	
// block ME until all shader stages are done at EOP, i.e. GPU idling
((ID3D11DeviceContextX*)last_context)->InsertWaitUntilIdle(0);

// flush and invalidate all caches at PFP
((ID3D11DeviceContextX*)last_context)->FlushGpuCacheRange(  D3D11_FLUSH_TEXTURE_L1_INVALIDATE  |
		     D3D11_FLUSH_TEXTURE_L2_INVALIDATE |
		     D3D11_FLUSH_COLOR_BLOCK_INVALIDATE |
		     D3D11_FLUSH_DEPTH_BLOCK_INVALIDATE |
		     D3D11_FLUSH_KCACHE_INVALIDATE |
		     D3D11_FLUSH_ICACHE_INVALIDATE |
		     D3D11_FLUSH_ENGINE_PFP
				,nullptr,0);
#endif
	D3D11_MAP map_type=D3D11_MAP_WRITE_DISCARD;
/*	if(((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
		map_type=D3D11_MAP_WRITE;*/
	if(!mapped.pData)
		last_context->Map(texture,0,map_type,SIMUL_D3D11_MAP_FLAGS,&mapped);
	if(!mapped.pData)
	{
		SIMUL_CERR<<"Failed to set texels on texture "<<name.c_str()<<std::endl;
		return;
	}
	int byteSize=platform::dx11::ByteSizeOfFormatElement(dxgi_format);
	const unsigned char *source=(const unsigned char*)src;
	unsigned char *target=(unsigned char*)mapped.pData;
	int expected_pitch=byteSize*width;
	if(mapped.RowPitch==(UINT)expected_pitch)
	{
		target+=texel_index*byteSize;
		memcpy(target,source,num_texels*byteSize);
	}
	else if(depth>1)
	{
		for(int z=0;z<depth;z++)
		{
			unsigned char *t=target;
			for(int r=0;r<length;r++)
			{
				memcpy(t,source,width*byteSize);
				t		+=mapped.RowPitch;
				source	+=width*byteSize;
			}
			target+=mapped.DepthPitch;
		}
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
	// Wow. We don't want to do this, right?
	//if(texel_index+num_texels>=width*length)
	{
		last_context->Unmap(texture,0);
		memset(&mapped,0,sizeof(mapped));
	}
#ifdef _XBOX_ONE
	
// block ME until all shader stages are done at EOP, i.e. GPU idling
((ID3D11DeviceContextX*)last_context)->InsertWaitUntilIdle(0);

// flush and invalidate all caches at PFP
((ID3D11DeviceContextX*)last_context)->FlushGpuCacheRange(  D3D11_FLUSH_TEXTURE_L1_INVALIDATE  |
		     D3D11_FLUSH_TEXTURE_L2_INVALIDATE |
		     D3D11_FLUSH_COLOR_BLOCK_INVALIDATE |
		     D3D11_FLUSH_DEPTH_BLOCK_INVALIDATE |
		     D3D11_FLUSH_KCACHE_INVALIDATE |
		     D3D11_FLUSH_ICACHE_INVALIDATE |
		     D3D11_FLUSH_ENGINE_PFP
				,nullptr,0);
#endif
}

bool Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform* r, void* T, int w, int l, crossplatform::PixelFormat f, bool make_rt, bool setDepthStencil, int numOfSamples)
{
	ID3D11Texture2D* t = reinterpret_cast<ID3D11Texture2D*>(T);
	//Check that the texture and srv pointers are valid.
	if (t)
	{
		ID3D11Texture2D* _t;
		if (t->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&_t) != S_OK)
		{
			SIMUL_CERR << "Can't initialise from external texture: 0x" << std::hex << t << std::dec << std::endl;
			SIMUL_BREAK_ONCE("Not a valid D3D Texture");
			return false;
		}
		SAFE_RELEASE(_t);
	}
	make_rt&=(!setDepthStencil);
	// If it's the same as before, return.
	if (texture == t && make_rt == renderTarget)
		return true;
	// If it's the same texture, and we created our own srv or don't need one, that's fine, return.
	if (texture != NULL && texture == t && !setDepthStencil)
		return true;
	renderPlatform=r;
	if(external_copy_source==t&& external_copy_source)
	{
		r->GetImmediateContext().asD3D11DeviceContext()->CopyResource(texture,external_copy_source);
		return true;
	}
	external_texture=true;
	FreeSRVTables();
	if(!external_texture&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(texture);
	SAFE_RELEASE(texture);
	SAFE_RELEASE(external_copy_source);
	if(t)
	{
		int refct=t->AddRef();
		if (platform::core::SimulInternalChecks)
		{
			//SIMUL_COUT << refct << std::endl;
		}
		ID3D11Texture2D* ppd(NULL);
		D3D11_TEXTURE2D_DESC textureDesc;
		if(t->QueryInterface( __uuidof(ID3D11Texture2D),(void**)&ppd)==S_OK)
		{
			FreeRTVTables();
			ppd->GetDesc(&textureDesc);
			// Can this texture have SRV's? If not we must COPY the resource.
			if(((textureDesc.BindFlags&D3D11_BIND_SHADER_RESOURCE)==D3D11_BIND_SHADER_RESOURCE))
			{
				texture=t;
			}
			else
			{
				external_copy_source=t;
				textureDesc.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
				if(make_rt)
					textureDesc.BindFlags|=D3D11_BIND_RENDER_TARGET;
				if(setDepthStencil&&(textureDesc.BindFlags&D3D11_BIND_RENDER_TARGET)==0)
					textureDesc.BindFlags|=D3D11_BIND_DEPTH_STENCIL;
				V_CHECK(renderPlatform->AsD3D11Device()->CreateTexture2D(&textureDesc,0,(ID3D11Texture2D**)(&texture)));
				r->GetImmediateContext().asD3D11DeviceContext()->CopyResource(texture,external_copy_source);
			}
			
			// ASSUME it's a cubemap if it's an array of six.
			if(textureDesc.ArraySize==6)
			{
				cubemap=(textureDesc.ArraySize==6);
				textureDesc.ArraySize=1;
			}
			else
				cubemap=false;

			renderTarget = make_rt && (textureDesc.BindFlags & D3D11_BIND_RENDER_TARGET);
			depthStencil = setDepthStencil && (textureDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL);
			dxgi_format=textureDesc.Format;
			genericDxgiFormat=dxgi_format;
			pixelFormat=RenderPlatform::FromDxgiFormat(textureDesc.Format);
			width=textureDesc.Width;
			length=textureDesc.Height;
			depth=textureDesc.ArraySize;
			arraySize=textureDesc.ArraySize;
			mips=textureDesc.MipLevels;
		}
		else
		{
			SIMUL_BREAK_ONCE("Not a valid D3D Texture");
			return false;
		}
		SAFE_RELEASE(ppd);
	}
	else
	{
		return false;
	}
	dim=2;
	return true;
}

bool Texture::InitFromExternalTexture3D(crossplatform::RenderPlatform *r,void *ta,bool make_uav)
{
	//Check that the texture and srv pointers are valid.
	if (ta)
	{
		ID3D11Texture3D* _ta;
		if (reinterpret_cast<ID3D11Texture3D*>(ta)->QueryInterface(__uuidof(ID3D11Texture3D), (void**)&_ta) != S_OK)
		{
			SIMUL_CERR << "Can't initialise from external texture: 0x" << std::hex << ta << std::dec << std::endl;
			SIMUL_BREAK_ONCE("Not a valid D3D Texture");
			return false;
		}
		SAFE_RELEASE(_ta);
	}
	// If it's the same as before, return.
	if (texture == ta && make_uav == computable)
		return true;
	// If it's the same texture, and we created our own srv, that's fine, return.
	if (texture!=NULL&&texture == ta)
		return true;
	FreeSRVTables();
	renderPlatform=r;
	if(!external_texture&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(texture);
	external_texture=true;
	SAFE_RELEASE(texture);
	SAFE_RELEASE(external_copy_source);
	texture=(ID3D11Resource*)ta;
	if(texture)
	{
		texture->AddRef();
		ID3D11Texture3D* ppd3(NULL);
		D3D11_TEXTURE3D_DESC textureDesc3;
		if(texture->QueryInterface( __uuidof(ID3D11Texture3D),(void**)&ppd3)==S_OK)
		{
			ppd3->GetDesc(&textureDesc3);
			dxgi_format=TypelessToSrvFormat(textureDesc3.Format);
			pixelFormat=RenderPlatform::FromDxgiFormat(dxgi_format);
			width=textureDesc3.Width;
			length=textureDesc3.Height;
			arraySize = 1;
			mips = textureDesc3.MipLevels;
			depth=textureDesc3.Depth;
			FreeUAVTables();
			computable = make_uav;
		}
		else
		{
			SIMUL_BREAK_ONCE("Not a valid D3D Texture");
			return false;
		}
		SAFE_RELEASE(ppd3);
	}
	dim=3;
	return true;
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int d,crossplatform::PixelFormat pf,bool computable,int m,bool rendertargets)
{
	pixelFormat = pf;
	DXGI_FORMAT dxgiFormat=dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	genericDxgiFormat=dxgi_format;
	dim=3;
	D3D11_TEXTURE3D_DESC textureDesc;
	bool ok=true;
	DXGI_FORMAT srvFormat = dxgiFormat;
	DXGI_FORMAT uavFormat = dxgiFormat;
	DXGI_FORMAT altUavFormat = dxgiFormat;
	if (dxgiFormat == DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		srvFormat = dxgiFormat;
		dxgiFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
#if PLATFORM_TYPED_UAV_FORMATS
		altUavFormat = uavFormat;
#else
		altUavFormat = DXGI_FORMAT_R32_UINT;
#endif
	}
	if(texture)
	{
		ID3D11Texture3D* ppd(NULL);
		if(texture->QueryInterface( __uuidof(ID3D11Texture3D),(void**)&ppd)!=S_OK)
			ok=false;
		else
		{
			ppd->GetDesc(&textureDesc);
			if(textureDesc.Width!=(UINT)w||textureDesc.Height!=(UINT)l||textureDesc.Depth!=(UINT)d||textureDesc.Format!=dxgiFormat||mips!=m)
				ok=false;
			if(computable!=((textureDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)==D3D11_BIND_UNORDERED_ACCESS))
				ok=false;
			if(rendertargets!=((textureDesc.BindFlags&D3D11_BIND_RENDER_TARGET)==D3D11_BIND_RENDER_TARGET))
				ok=false;
		}
		SAFE_RELEASE_SILENT(ppd);
	}
	else
		ok=false;
	bool changed=!ok;
	if(!ok)
	{
		renderPlatform=r;
		SIMUL_ASSERT(w > 0 && l > 0 && w <= 65536 && l <= 65536);
		InvalidateDeviceObjects();
		memset(&textureDesc,0,sizeof(textureDesc));
		textureDesc.Width			=width=w;
		textureDesc.Height			=length=l;
		textureDesc.Depth			=depth=d;
		textureDesc.Format			=dxgi_format=dxgiFormat;
		textureDesc.MipLevels		=mips=m;
		arraySize=1;
		D3D11_USAGE usage			=D3D11_USAGE_DYNAMIC;
		//if(((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
		//	usage=D3D11_USAGE_DEFAULT;
		textureDesc.Usage			=(computable|rendertargets)?D3D11_USAGE_DEFAULT:usage;
		textureDesc.BindFlags		=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0)|(rendertargets?D3D11_BIND_RENDER_TARGET:0);
		textureDesc.CPUAccessFlags	=(computable|rendertargets)?0:D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags		=0;
		
		V_CHECK(r->AsD3D11Device()->CreateTexture3D(&textureDesc,0,(ID3D11Texture3D**)(&texture)));
		external_texture=false;
		if(renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(texture,GetMemorySize(),name.c_str());
	}
	if(computable&&(!ok))
	{
		FreeUAVTables();
		this->computable = computable;
		changed=true;
	}
	if(d<=8&&rendertargets&&(!ok))
	{
		SIMUL_BREAK("Render targets for 3D textures are not currently supported.");
	}
	return changed;
}

void DepthFormatToResourceAndSrvFormats(DXGI_FORMAT &genericDxgiFormat,DXGI_FORMAT &srvFormat)
{
	switch (genericDxgiFormat)
	{
	case DXGI_FORMAT_D32_FLOAT:
		genericDxgiFormat = DXGI_FORMAT_R32_TYPELESS;
		srvFormat = DXGI_FORMAT_R32_FLOAT;
		break;
	case DXGI_FORMAT_D16_UNORM:
		genericDxgiFormat = DXGI_FORMAT_R16_TYPELESS;
		srvFormat = DXGI_FORMAT_R16_UNORM;
		break;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		genericDxgiFormat = DXGI_FORMAT_R24G8_TYPELESS;
		srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		break;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		genericDxgiFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
		srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		break;
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		srvFormat = genericDxgiFormat;
		genericDxgiFormat = DXGI_FORMAT_BC1_TYPELESS;
		break;
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		srvFormat = genericDxgiFormat;
		genericDxgiFormat = DXGI_FORMAT_BC2_TYPELESS;
		break;
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		srvFormat = genericDxgiFormat;
		genericDxgiFormat = DXGI_FORMAT_BC3_TYPELESS;
		break;
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		srvFormat = genericDxgiFormat;
		genericDxgiFormat = DXGI_FORMAT_BC4_TYPELESS;
		break;
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
		srvFormat = genericDxgiFormat;
		genericDxgiFormat = DXGI_FORMAT_BC5_TYPELESS;
		break;
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
		srvFormat = genericDxgiFormat;
		genericDxgiFormat = DXGI_FORMAT_BC6H_TYPELESS;
		break;
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		srvFormat = genericDxgiFormat;
		genericDxgiFormat = DXGI_FORMAT_BC7_TYPELESS;
		break;
	default:
		srvFormat = genericDxgiFormat;
		break;
	};
}


bool Texture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *r
	, int w, int l, int m
	, crossplatform::PixelFormat f
	, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
	, bool computable, bool rendertarget, bool depthstencil
	, int num_samples, int aa_quality, bool
	, vec4 clear, float clearDepth, uint clearStencil, bool shared
	, crossplatform::CompressionFormat compressionFormat)
{
	return EnsureTexture2DSizeAndFormat(r, w, l,m, f, data,computable, rendertarget, depthstencil, num_samples, aa_quality, false, clear, clearDepth, clearStencil,compressionFormat);
}

bool Texture::EnsureTexture2DSizeAndFormat(crossplatform::RenderPlatform *r
	, int w, int l,int m
	, crossplatform::PixelFormat f
	, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
	, bool computable, bool rendertarget, bool depthstencil
	, int num_samples, int aa_quality, bool
	, vec4 clear, float clearDepth, uint clearStencil
	, crossplatform::CompressionFormat compressionFormat
	)
{
	renderPlatform=r;
	pixelFormat=f;
	dxgi_format=(DXGI_FORMAT)dx11::RenderPlatform::ToDxgiFormat(pixelFormat, compressionFormat);
	genericDxgiFormat=dxgi_format;
	srvFormat=dxgi_format;
	DepthFormatToResourceAndSrvFormats(genericDxgiFormat,srvFormat);
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
			if(textureDesc.Width!=(UINT)w||textureDesc.Height!= (UINT)l||textureDesc.Format!=genericDxgiFormat)
				ok=false;
			if(computable!=((textureDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)==D3D11_BIND_UNORDERED_ACCESS))
				ok=false;
			if(rendertarget!=((textureDesc.BindFlags&D3D11_BIND_RENDER_TARGET)==D3D11_BIND_RENDER_TARGET))
				ok=false;
		}
		SAFE_RELEASE_SILENT(ppd);
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
				genericDxgiFormat,
				num_samples,
				&numQualityLevels	));
			//if(aa_quality>=numQualityLevels)
			//	aa_quality=numQualityLevels-1;
			if(numQualityLevels==0)
				num_samples/=2;
		};
		if(w*l>0)
		{
			bool compressed = compressionFormat != crossplatform::CompressionFormat::UNCOMPRESSED;
			memset(&textureDesc,0,sizeof(textureDesc));
			textureDesc.Width					=width=w;
			textureDesc.Height					=length=l;
			depth								=1;
			textureDesc.Format					=genericDxgiFormat;
			textureDesc.MipLevels				=mips=m;
			textureDesc.ArraySize				=arraySize=1;
			D3D11_USAGE usage					=D3D11_USAGE_DYNAMIC;
			if(compressed||m>1)
				usage							=D3D11_USAGE_DEFAULT;
			textureDesc.Usage					=(computable||rendertarget||depthstencil)?D3D11_USAGE_DEFAULT:usage;
			textureDesc.BindFlags				=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0)|(rendertarget?D3D11_BIND_RENDER_TARGET:0)|(depthstencil?D3D11_BIND_DEPTH_STENCIL:0);
			textureDesc.CPUAccessFlags			=(computable||rendertarget||depthstencil||compressed||usage!=D3D11_USAGE_DYNAMIC)?0:D3D11_CPU_ACCESS_WRITE;
			textureDesc.MiscFlags				=rendertarget?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;
			textureDesc.SampleDesc.Count		=num_samples;
			textureDesc.SampleDesc.Quality		=aa_quality;
			
			std::vector<D3D11_SUBRESOURCE_DATA> initialSubresourceData(m*arraySize);
			int mip_width=width;
			int mip_length=length;
			if(data)
			{
				for(size_t i=0;i<m;i++)
				{
					for(size_t j=0;j<arraySize;j++)
					{
						size_t n=i*arraySize+j;
						D3D11_SUBRESOURCE_DATA& subresource_data =initialSubresourceData[n];
						subresource_data.pSysMem			= (*data)[n].data();
						subresource_data.SysMemPitch		=mip_width*ByteSizeOfFormatElement(dxgi_format);
						static int uu = 4;
						subresource_data.SysMemSlicePitch = subresource_data.SysMemPitch*mip_length;
						switch (compressionFormat)
						{
						case crossplatform::CompressionFormat::BC1:
						case crossplatform::CompressionFormat::BC3:
						case crossplatform::CompressionFormat::BC5:
							subresource_data.SysMemPitch				= ByteSizeOfFormatElement(dxgi_format)*std::max(1,mip_width / uu);
							subresource_data.SysMemSlicePitch			= subresource_data.SysMemPitch*std::max(1,mip_length/4);
							break;
						default:
							break;
						};
					}
					mip_width=(mip_width+1)/2;
					mip_length=(mip_length+1)/2;
				}
				SAFE_RELEASE(stagingBuffer);
				//Create a "Staging" Resource to actually copy data to-from the GPU buffer. 
				D3D11_TEXTURE2D_DESC desc;
				memset(&desc,0,sizeof(desc));
				desc.Width			=width;
				desc.Height			=length;
				desc.MipLevels		=m;
				desc.ArraySize		=1;
				desc.Format			=dxgi_format;
				desc.Usage			=D3D11_USAGE_STAGING;
				desc.CPUAccessFlags	=D3D11_CPU_ACCESS_WRITE;
				desc.SampleDesc.Count=1;
				desc.SampleDesc.Quality=0;
				ID3D11Texture2D *tex;
				V_CHECK(renderPlatform->AsD3D11Device()->CreateTexture2D(&desc, data ? initialSubresourceData.data():nullptr,&tex));
				stagingBuffer=tex;
				if(renderPlatform->GetMemoryInterface())
					renderPlatform->GetMemoryInterface()->TrackVideoMemory(stagingBuffer,GetMemorySize(),name.c_str());
				if(!stagingBuffer)
				{
					SIMUL_BREAK_ONCE("Failed to create staging texture");
				}
				else
				{
					textureUploadComplete=false;
				}

			}
			HRESULT hr=pd3dDevice->CreateTexture2D(&textureDesc, nullptr,(ID3D11Texture2D**)(&texture));
			if (hr != S_OK)
			{
				textureDesc.Format= srvFormat = (DXGI_FORMAT)dx11::RenderPlatform::ToDxgiFormat(platform::crossplatform::PixelFormat::RGBA_8_UNORM, crossplatform::CompressionFormat::UNCOMPRESSED);
				V_CHECK(hr=pd3dDevice->CreateTexture2D(&textureDesc,  nullptr, (ID3D11Texture2D * *)(&texture)));
				if (hr != S_OK)
					return false;
			}
			external_texture=false;
			if(renderPlatform->GetMemoryInterface())
				renderPlatform->GetMemoryInterface()->TrackVideoMemory(texture,GetMemorySize(),name.c_str());

			SetDebugObjectName(texture,"dx11::Texture::ensureTexture2DSizeAndFormat");
		}
		this->computable = computable;
		this->renderTarget = rendertarget;
		this->depthStencil = depthstencil;
	}
	mips=m;
	arraySize=1;
	return !ok;
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int num,int m,crossplatform::PixelFormat f
	, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
	,bool computable,bool rendertarget,bool depthstencil,bool cubemap,crossplatform::CompressionFormat compressionFormat)
{
	renderPlatform=r;
	if(m<=0||m>16)
		m=1;
	int total_num			=cubemap?6*num:num;
	dxgi_format=(DXGI_FORMAT)dx11::RenderPlatform::ToDxgiFormat(f,compressionFormat);
	genericDxgiFormat=dxgi_format;
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
			if(textureDesc.ArraySize!=(UINT)total_num||textureDesc.MipLevels!=(UINT)m||textureDesc.Width!=(UINT)w||textureDesc.Height!=(UINT)l||textureDesc.Format!=dxgi_format)
				ok=false;
			if(computable!=((textureDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)==D3D11_BIND_UNORDERED_ACCESS))
				ok=false;
			if(rendertarget!=((textureDesc.BindFlags&D3D11_BIND_RENDER_TARGET)==D3D11_BIND_RENDER_TARGET))
				ok=false;
		}
		SAFE_RELEASE_SILENT(ppd);
	}
	else
		ok=false; 
	if(ok)
		return false;


	pixelFormat=f;
	dxgi_format=(DXGI_FORMAT)dx11::RenderPlatform::ToDxgiFormat(pixelFormat, compressionFormat);
	genericDxgiFormat=dxgi_format;
	srvFormat=dxgi_format;
	DepthFormatToResourceAndSrvFormats(genericDxgiFormat,srvFormat);
	InvalidateDeviceObjects();
	//dxgi_format=(DXGI_FORMAT)dx11::RenderPlatform::ToDxgiFormat(pixelFormat,compressionFormat);
	D3D11_TEXTURE2D_DESC desc;

	mips=m;
	arraySize=num;

	width					=w;
	length					=l;
	depth					=1;
	dim						=2;
	desc.Width				=w;
	desc.Height				=l;
	desc.Format				=genericDxgiFormat;
	desc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0)|(rendertarget?D3D11_BIND_RENDER_TARGET:0);
	desc.Usage				=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags		=0;
	desc.ArraySize			=total_num;
	desc.MiscFlags			=(rendertarget?D3D11_RESOURCE_MISC_GENERATE_MIPS:0)|(cubemap?D3D11_RESOURCE_MISC_TEXTURECUBE:0);
	desc.MipLevels			=m;
	desc.SampleDesc.Count	=1;
	desc.SampleDesc.Quality	=0;
	if(w*l*num<=0)
		return false;
	ID3D11Texture2D *pArrayTexture;

	
	std::vector<D3D11_SUBRESOURCE_DATA> initialSubresourceData(m*total_num);
	if(data)
	{
		size_t bytesPerTexel	=crossplatform::GetByteSize(f);
		size_t n=0;
		for(size_t j=0;j<total_num;j++)
		{
			int mip_width=width;
			int mip_length=length;
			for(size_t i=0;i<m;i++)
			{
				D3D11_SUBRESOURCE_DATA &subresource_data =initialSubresourceData[n];
				subresource_data.pSysMem						=(*data)[n].data();
				static int uu = 4;
				switch (compressionFormat)
				{
				case crossplatform::CompressionFormat::BC1:
				case crossplatform::CompressionFormat::BC3:
				case crossplatform::CompressionFormat::BC5:
					{
						size_t block_width		=std::max(1,(mip_width+3)/4);
						size_t block_height		=std::max(1,(mip_length+3)/4);
						size_t block_size_bytes	=(bytesPerTexel==4)?16:8;
						subresource_data.SysMemPitch				= (UINT)(block_size_bytes*block_width);//ByteSizeOfFormatElement(dxgi_format)*std::max(1,mip_width / uu);
						subresource_data.SysMemSlicePitch			= (UINT)std::max(block_size_bytes, subresource_data.SysMemPitch*block_height);//data.SysMemPitch*std::max(1,mip_length/4);
					}
					break;
				default:
					subresource_data.SysMemPitch		=mip_width*ByteSizeOfFormatElement(dxgi_format);
					subresource_data.SysMemSlicePitch = subresource_data.SysMemPitch*mip_length;
					break;
				};
				mip_width=(mip_width+1)/2;
				mip_length=(mip_length+1)/2;
				n++;
			}
		}
		SAFE_RELEASE(stagingBuffer);
		//Create a "Staging" Resource to actually copy data to-from the GPU buffer. 
		D3D11_TEXTURE2D_DESC desc;
		memset(&desc,0,sizeof(desc));
		desc.Width			=width;
		desc.Height			=length;
		desc.MipLevels		=m;
		desc.ArraySize		=total_num;
		desc.Format			=dxgi_format;
		desc.Usage			=D3D11_USAGE_STAGING;
		desc.CPUAccessFlags	=D3D11_CPU_ACCESS_WRITE;
		desc.SampleDesc.Count=1;
		desc.SampleDesc.Quality=0;
		ID3D11Texture2D *tex;
		V_CHECK(renderPlatform->AsD3D11Device()->CreateTexture2D(&desc,data? initialSubresourceData.data():nullptr,&tex));
		stagingBuffer=tex;
		if(renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(stagingBuffer,GetMemorySize(),name.c_str());
		textureUploadComplete=false;
	}

	V_CHECK(renderPlatform->AsD3D11Device()->CreateTexture2D(&desc,nullptr,&pArrayTexture));
	if(renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->TrackVideoMemory(pArrayTexture,GetMemorySize(),name.c_str());
	if(!external_texture&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(texture);
	SAFE_RELEASE(texture);
	SAFE_RELEASE(external_copy_source);
	external_texture=false;
	texture=pArrayTexture;
	if(!texture)
		return false;
	
	this->computable = computable;
	this->renderTarget = rendertarget;
	this->depthStencil = depthstencil;

	mips=m;
	arraySize=num;
	this->cubemap=cubemap;
	return true;
}

void Texture::ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,crossplatform::PixelFormat pf,bool computable)
{
	pixelFormat=pf;
	DXGI_FORMAT f=dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	dim=1;
	int m=1;
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
			if(textureDesc.Width!= (UINT)w||textureDesc.Format!=f)
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
		textureDesc.Format			=dxgi_format=f;
		textureDesc.MipLevels		=m;
		textureDesc.ArraySize		=1;
		D3D11_USAGE usage					=D3D11_USAGE_DYNAMIC;
		//if(((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
		//	usage							=D3D11_USAGE_DEFAULT;
		textureDesc.Usage			=computable?D3D11_USAGE_DEFAULT:usage;
		textureDesc.BindFlags		=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0);
		textureDesc.CPUAccessFlags	=computable?0:D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags		=0;
		
		V_CHECK(pd3dDevice->CreateTexture1D(&textureDesc,0,(ID3D11Texture1D**)(&texture)));
		external_texture=false;
		if(renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(texture,GetMemorySize(),name.c_str());
	}
	this->computable = computable;

	mips=m;
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear)
{
	if (!depthStencil)
		SIMUL_CERR << "Attempting to clear a texture that is not a Depth Stencil" << std::endl;

	bool layered = NumFaces() > 1;
	bool ms = GetSampleCount() > 1;
	crossplatform::TextureView tv;
	tv.type = GetShaderResourceTypeForRTVAndDSV();
	tv.subresourceRange = { crossplatform::TextureAspectFlags::DEPTH, 0, 1, 0, 1 };

	ID3D11DepthStencilView* dsv = AsD3D11DepthStencilView(tv);
	deviceContext.asD3D11DeviceContext()->ClearDepthStencilView(dsv, (D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL), depthClear, stencilClear);
}

void Texture::GenerateMips(crossplatform::GraphicsDeviceContext &deviceContext)
{
	// We can't detect if this has worked or not.
	if (!renderTarget)
		SIMUL_CERR<<"Can't use GenerateMips on texture "<<this<<" not created as rendertarget.\n";

	bool layered = NumFaces() > 1;
	bool ms = GetSampleCount() > 1;
	crossplatform::TextureView tv;
	tv.type = crossplatform::ShaderResourceType::TEXTURE_2D
		| (layered ? crossplatform::ShaderResourceType::ARRAY : crossplatform::ShaderResourceType(0))
		| (ms ? crossplatform::ShaderResourceType::MS : crossplatform::ShaderResourceType(0));

	deviceContext.asD3D11DeviceContext()->GenerateMips(AsD3D11ShaderResourceView(tv));
}

void Texture::map(ID3D11DeviceContext *context)
{
	if(mapped.pData!=NULL)
		return;
	last_context=context;
	D3D11_MAP map_type=D3D11_MAP_WRITE_DISCARD;
	//if(((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
	///	map_type=D3D11_MAP_WRITE;
	last_context->Map(texture,0,map_type,((dx11::RenderPlatform*)renderPlatform)->GetMapFlags(),&mapped);
}

bool Texture::isMapped() const
{
	return (mapped.pData!=NULL);
}

void Texture::unmap()
{
	if(mapped.pData)
		last_context->Unmap(texture,0);
	mapped.pData=NULL;
	last_context=NULL;
}

vec4 Texture::GetTexel(crossplatform::DeviceContext &deviceContext,vec2 texCoords,bool wrap)
{
	if(!stagingBuffer)
	{
		//Create a "Staging" Resource to actually copy data to-from the GPU buffer. 
		D3D11_TEXTURE2D_DESC desc;
		memset(&desc,0,sizeof(desc));
		desc.Width			=1;
		desc.Height		=1;
		desc.ArraySize = 1;
		desc.Format		=dxgi_format;
		desc.Usage			=D3D11_USAGE_STAGING;
		desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
		desc.SampleDesc.Count=1;
		desc.SampleDesc.Quality=0;
		ID3D11Texture2D *tex;
		ID3D11Device *dev=deviceContext.renderPlatform->AsD3D11Device();
		deviceContext.renderPlatform->AsD3D11Device()->CreateTexture2D(&desc,NULL,&tex);
		stagingBuffer=tex;
		if(renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(stagingBuffer,GetMemorySize(),name.c_str());
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
	void *pixel=nullptr;
	if(!texture)
		return vec4(0,0,0,0);
	try
	{
		deviceContext.asD3D11DeviceContext()->CopySubresourceRegion(stagingBuffer, 0, 0, 0, 0, texture, 0, &srcBox);

		D3D11_MAPPED_SUBRESOURCE msr;
		deviceContext.asD3D11DeviceContext()->Map(stagingBuffer, 0, D3D11_MAP_READ,SIMUL_D3D11_MAP_FLAGS, &msr);
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

void Texture::activateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::TextureView textureView)
{
	if(!deviceContext.asD3D11DeviceContext())
		return;
	last_context=deviceContext.asD3D11DeviceContext();

	const int& mip = textureView.subresourceRange.baseMipLevel;
	if (textureView.type == crossplatform::ShaderResourceType::UNKNOWN)
		textureView.type = GetShaderResourceTypeForRTVAndDSV();


	D3D11_VIEWPORT viewport;
	if (renderTarget)
	{
		ID3D11RenderTargetView* rtv = AsD3D11RenderTargetView(textureView);
		last_context->OMSetRenderTargets(1, &rtv, NULL);
		{
			viewport.Width		=(float)(std::max(1,(width>>mip)));
			viewport.Height		=(float)(std::max(1,(length>>mip)));
			viewport.TopLeftX	=0;
			viewport.TopLeftY	=0;
			viewport.MinDepth	=0.0f;
			viewport.MaxDepth	=1.0f;
			last_context->RSSetViewports(1, &viewport);
		}
	}

	targetsAndViewport.m_rt[0] = AsD3D11RenderTargetView(textureView);
	targetsAndViewport.m_dt=nullptr;
	targetsAndViewport.viewport.x=targetsAndViewport.viewport.y=0;
	targetsAndViewport.viewport.w=(int)viewport.Width;
	targetsAndViewport.viewport.h=(int)viewport.Height;
	targetsAndViewport.num=1;
	
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);

}

void Texture::deactivateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext)
{
	if(renderPlatform)
		renderPlatform->DeactivateRenderTargets(deviceContext);
	else
		deviceContext.GetFrameBufferStack().pop();
}

void Texture::FinishLoading(crossplatform::DeviceContext &deviceContext)
{
	if(textureUploadComplete)
		return;
	if(stagingBuffer)
		deviceContext.asD3D11DeviceContext()->CopyResource(texture,stagingBuffer);
	textureUploadComplete=true;
}

int Texture::GetSampleCount() const
{
	if (!texture)
		return 0;
	D3D11_TEXTURE2D_DESC t2d_desc;
	((ID3D11Texture2D*)texture)->GetDesc(&t2d_desc);
	int mNumSamples = t2d_desc.SampleDesc.Count;
	return mNumSamples == 1 ? 0 : mNumSamples;
}