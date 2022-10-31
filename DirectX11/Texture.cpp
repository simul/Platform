
#include "Texture.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"
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


Texture::Texture()
	:stagingBuffer(NULL)
	,last_context(NULL)
	,texture(NULL)
	,external_copy_source(nullptr)
	,mainShaderResourceView(NULL)
	,arrayShaderResourceView(nullptr)
	,layerShaderResourceViews(NULL)
	,mainMipShaderResourceViews(NULL)
	,layerMipShaderResourceViews(NULL)
	,mipUnorderedAccessViews(NULL)
	,layerMipUnorderedAccessViews(NULL)
	,depthStencilView(NULL)
	,renderTargetViews(NULL)
{
	memset(&mapped,0,sizeof(mapped));
}


Texture::~Texture()
{
	InvalidateDeviceObjects();
}

void Texture::FreeRTVTables()
{
	if(renderTargetViews)
	{
		int total_num=cubemap?arraySize*6:arraySize;
		for(int i=0;i<total_num;i++)
		{
			if(renderTargetViews[i])
			for(int j=0;j<mips;j++)
			{
				SAFE_RELEASE(renderTargetViews[i][j]);
			}
			delete [] renderTargetViews[i];
		}
		delete [] renderTargetViews;
		renderTargetViews=NULL;
	}
}

void Texture::InitUAVTables(int l,int m)
{
	mipUnorderedAccessViews			=nullptr;
	if(m)
	{
		mipUnorderedAccessViews		=new ID3D11UnorderedAccessView*[m+1];		// UAV's for whole texture at different mips.
		mipUnorderedAccessViews[m]=0;
	}
	layerMipUnorderedAccessViews	=nullptr;
	if(l&&m)
	{
		layerMipUnorderedAccessViews	=new ID3D11UnorderedAccessView**[l];			
		for(int i=0;i<l;i++)
		{
			layerMipUnorderedAccessViews[i]=new ID3D11UnorderedAccessView*[m];	// UAV's for each layer at different mips.
		}
	}
}

void Texture::FreeUAVTables()
{
	if(mipUnorderedAccessViews)
	{
		for(int j=0;j<mips+1;j++)
		{
			SAFE_RELEASE(mipUnorderedAccessViews[j]);
		}
		delete [] mipUnorderedAccessViews;
	}
	mipUnorderedAccessViews=nullptr;
	if(layerMipUnorderedAccessViews)
	{
		int total_num=cubemap?arraySize*6:arraySize;
		for(int i=0;i<total_num;i++)
		{
			for(int j=0;j<mips;j++)
			{
				SAFE_RELEASE(layerMipUnorderedAccessViews[i][j]);
			}
			delete [] layerMipUnorderedAccessViews[i];
		}
		delete [] layerMipUnorderedAccessViews;
		layerMipUnorderedAccessViews=nullptr;
	}
}

void Texture::InitRTVTables(int l,int m)
{
	renderTargetViews=nullptr;
	
	renderTargetViews=new ID3D11RenderTargetView**[l];			// SRV's for each layer at different mips.
	for(int i=0;i<l;i++)
	{
		renderTargetViews[i]=new ID3D11RenderTargetView*[m];	// SRV's for each layer at different mips.
	}
}

void Texture::InvalidateDeviceObjects()
{
	FreeRTVTables();
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
	SAFE_RELEASE(depthStencilView);
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
	SAFE_RELEASE(mainShaderResourceView);
	SAFE_RELEASE(arrayShaderResourceView);
	ID3D11Texture2D *t	=platform::dx11::LoadTexture(renderPlatform->AsD3D11Device(),pFilePathUtf8,pathsUtf8);

	InitFromExternalTexture2D(renderPlatform, t, nullptr, 0, 0, crossplatform::PixelFormat::UNKNOWN);
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

		ensureTextureArraySizeAndFormat(r,desc.Width,desc.Height,(int)textures.size(),m,format,false,true);
	
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
	return (mainShaderResourceView!=NULL||texture!=NULL);
}

ID3D11ShaderResourceView *Texture::AsD3D11ShaderResourceView(crossplatform::ShaderResourceType t,int index,int mip)
{
	if(mip>=mips)
		mip=mips-1;
#ifdef _DEBUG
	if(index>=arraySize)
	{
		SIMUL_BREAK_ONCE("AsD3D11ShaderResourceView: mip or index out of range");
		return NULL;
	}
#endif
	bool no_array=!cubemap&&(arraySize<=1);
	if(mips<=1&&no_array||(index<0&&mip<0))
	{
		if(IsCubemap()&&t==crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
			return arrayShaderResourceView;
		return mainShaderResourceView;
	}
	if(layerShaderResourceViews&&(mip<0||mips<=1))
	{
		if(index<0||no_array)
			return mainShaderResourceView;
		return layerShaderResourceViews[index];
	}
	if(mainMipShaderResourceViews&&(no_array||index<0))
		return mainMipShaderResourceViews[mip];
	if(layerMipShaderResourceViews)
		return layerMipShaderResourceViews[index][mip];
	
	return nullptr;
}

ID3D11UnorderedAccessView *Texture::AsD3D11UnorderedAccessView(int index,int mip)
{
	if(mip<0)
	{
		mip=0;
	}
	if(index<0)
	{
		if(mipUnorderedAccessViews)
			return mipUnorderedAccessViews[mip];		// UAV for the whole texture at various mips
		else index=0;
	}
	if(!layerMipUnorderedAccessViews)
		return NULL;
	return layerMipUnorderedAccessViews[index][mip];
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

bool Texture::IsComputable() const
{
	return (mipUnorderedAccessViews!=nullptr||layerMipUnorderedAccessViews!=nullptr);
}

bool Texture::HasRenderTargets() const
{
	return (renderTargetViews!=nullptr);
}

bool Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform *r,void *T,void *SRV,int w,int l,crossplatform::PixelFormat f,bool make_rt, bool setDepthStencil,bool need_srv,int numOfSamples)
{
	ID3D11Texture2D* t = reinterpret_cast<ID3D11Texture2D*>(T);
	ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(SRV);
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
	if (srv)
	{
		ID3D11ShaderResourceView* _srv;
		if (srv->QueryInterface(__uuidof(ID3D11ShaderResourceView), (void**)&_srv) != S_OK)
		{
			SIMUL_CERR << "Can't use external shader resource view: 0x" << std::hex << srv << std::dec << std::endl;
		}
		SAFE_RELEASE(_srv);
	}
	make_rt&=(!setDepthStencil);
	// If it's the same as before, return.
	if ((texture == t && srv==mainShaderResourceView) && mainShaderResourceView != NULL && (make_rt ==( renderTargetViews != NULL)))
		return true;
	if(texture==t&&mainShaderResourceView!=nullptr)
		return true;
	// If it's the same texture, and we created our own srv or don't need one, that's fine, return.
	if (texture!=NULL&&texture == t&&(!need_srv||(mainShaderResourceView != NULL&&srv == NULL))&&!setDepthStencil)
		return true;
	renderPlatform=r;
	if(external_copy_source==t&& external_copy_source)
	{
		r->GetImmediateContext().asD3D11DeviceContext()->CopyResource(texture,external_copy_source);
		return true;
	}
	external_texture=true;
	FreeSRVTables();
	SAFE_RELEASE(mainShaderResourceView);
	SAFE_RELEASE(arrayShaderResourceView);
	if(!external_texture&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(texture);
	SAFE_RELEASE(texture);
	SAFE_RELEASE(external_copy_source);
	mainShaderResourceView=srv;
	if(mainShaderResourceView)
		mainShaderResourceView->AddRef();
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
			if(((textureDesc.BindFlags&D3D11_BIND_SHADER_RESOURCE)==D3D11_BIND_SHADER_RESOURCE)||!need_srv)
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
			dxgi_format=textureDesc.Format;
			pixelFormat=RenderPlatform::FromDxgiFormat(textureDesc.Format);
			width=textureDesc.Width;
			length=textureDesc.Height;
			int total_num=textureDesc.ArraySize*(cubemap?6:1);
			if(!srv&&need_srv&&(textureDesc.BindFlags&D3D11_BIND_SHADER_RESOURCE)!=0)
			{
				InitSRVTables(total_num,textureDesc.MipLevels);
				CreateSRVTables(textureDesc.ArraySize,textureDesc.MipLevels,cubemap,false,textureDesc.SampleDesc.Count>1);
				arraySize=textureDesc.ArraySize;
				mips=textureDesc.MipLevels;
			}
			depth=textureDesc.ArraySize;
			if(make_rt&&(textureDesc.BindFlags&D3D11_BIND_RENDER_TARGET))
			{
				renderTarget = true;
				// Setup the description of the render target view.
				D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
				renderTargetViewDesc.Format = TypelessToSrvFormat(textureDesc.Format);
				InitRTVTables(total_num,textureDesc.MipLevels);

				arraySize=textureDesc.ArraySize;
				mips=textureDesc.MipLevels;
				bool msaa=(textureDesc.SampleDesc.Count)>1;
				if(renderTargetViews)
				{
					renderTargetViewDesc.ViewDimension		=msaa?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
					if(msaa)
					{
						renderTargetViewDesc.Texture2DMSArray.FirstArraySlice		=0;
						renderTargetViewDesc.Texture2DMSArray.ArraySize				=1;
					}
					else
					{
						renderTargetViewDesc.Texture2DArray.FirstArraySlice		=0;
						renderTargetViewDesc.Texture2DArray.ArraySize			=1;
					}
					for(int i=0;i<(int)total_num;i++)
					{
						if(msaa)
							renderTargetViewDesc.Texture2DMSArray.FirstArraySlice		=0;
						else
							renderTargetViewDesc.Texture2DArray.FirstArraySlice = i;
						for(int j=0;j<(int)textureDesc.MipLevels;j++)
						{
							if(!msaa)
								renderTargetViewDesc.Texture2DArray.MipSlice			=j;
							V_CHECK(renderPlatform->AsD3D11Device()->CreateRenderTargetView(texture,&renderTargetViewDesc,&(renderTargetViews[i][j])));
						}
					}
				}
			}
			if (setDepthStencil && (textureDesc.BindFlags&D3D11_BIND_DEPTH_STENCIL))
			{
				D3D11_TEX2D_DSV dsv;
				dsv.MipSlice = 0;
				D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc;
				depthDesc.ViewDimension =(textureDesc.SampleDesc.Count)>1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D ;
				depthDesc.Format = ResourceToDsvFormat(textureDesc.Format);
				depthDesc.Flags = 0;
				depthDesc.Texture2D = dsv;
				SAFE_RELEASE(depthStencilView);
				V_CHECK(renderPlatform->AsD3D11Device()->CreateDepthStencilView(texture, &depthDesc, &depthStencilView));
			}
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

bool Texture::InitFromExternalTexture3D(crossplatform::RenderPlatform *r,void *ta,void *srv,bool make_uav)
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
	if (srv)
	{
		ID3D11ShaderResourceView* _srv;
		if (reinterpret_cast<ID3D11ShaderResourceView*>(srv)->QueryInterface(__uuidof(ID3D11ShaderResourceView), (void**)&_srv) != S_OK)
		{
			SIMUL_CERR << "Can't use external shader resource view: 0x" << std::hex << srv << std::dec << std::endl;
		}
		SAFE_RELEASE(_srv);
	}
	// If it's the same as before, return.
	if ((texture == ta && srv==mainShaderResourceView) && mainShaderResourceView != NULL && (make_uav ==( mipUnorderedAccessViews != NULL)))
		return true;
	// If it's the same texture, and we created our own srv, that's fine, return.
	if (texture!=NULL&&texture == ta&&mainShaderResourceView != NULL&&srv == NULL)
		return true;
	FreeSRVTables();
	renderPlatform=r;
	SAFE_RELEASE(mainShaderResourceView);
	SAFE_RELEASE(arrayShaderResourceView);
	if(!external_texture&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(texture);
	external_texture=true;
	SAFE_RELEASE(texture);
	SAFE_RELEASE(external_copy_source);
	texture=(ID3D11Resource*)ta;
	mainShaderResourceView=(ID3D11ShaderResourceView*)srv;
	if(mainShaderResourceView)
		mainShaderResourceView->AddRef();
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
			if(!srv)
			{
				InitSRVTables(1,textureDesc3.MipLevels);
				CreateSRVTables(1,textureDesc3.MipLevels,false,true);
				arraySize=1;
				mips=textureDesc3.MipLevels;
			}
			depth=textureDesc3.Depth;
			FreeUAVTables();
			InitUAVTables(1,textureDesc3.MipLevels);// 1 layer, m mips.
	
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
			ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			uav_desc.Format					=dxgi_format;
			uav_desc.ViewDimension			=D3D11_UAV_DIMENSION_TEXTURE3D;
			uav_desc.Texture3D.MipSlice		=0;
			uav_desc.Texture3D.WSize		=textureDesc3.Depth;
			uav_desc.Texture3D.FirstWSlice	=0;
		
			if(mipUnorderedAccessViews)
			for(uint i=0;i<textureDesc3.MipLevels;i++)
			{
				uav_desc.Texture3D.MipSlice=i;
				V_CHECK(r->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &mipUnorderedAccessViews[i]));
				uav_desc.Texture3D.WSize/=2;
			}
		
			uav_desc.Texture3D.WSize	= textureDesc3.Depth;
			if(layerMipUnorderedAccessViews)
			for(uint i=0;i<textureDesc3.MipLevels;i++)
			{
				uav_desc.Texture3D.MipSlice=i;
				V_CHECK(r->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &layerMipUnorderedAccessViews[0][i]));
				uav_desc.Texture3D.WSize/=2;
			}
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
		FreeSRVTables();
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

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						= srvFormat;
		srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Texture3D.MipLevels		= m;
		srv_desc.Texture3D.MostDetailedMip	= 0;
		InitSRVTables(1,m);
		V_CHECK(r->AsD3D11Device()->CreateShaderResourceView(texture,&srv_desc,&mainShaderResourceView));
		if(mainMipShaderResourceViews)
		for(int j=0;j<m;j++)
		{
			srv_desc.Texture3D.MipLevels=1;
			srv_desc.Texture3D.MostDetailedMip=j;
			V_CHECK(r->AsD3D11Device()->CreateShaderResourceView(texture, &srv_desc, &mainMipShaderResourceViews[j]));
		}
	}
	if(computable&&(!layerMipUnorderedAccessViews||!ok))
	{
		FreeUAVTables();
		InitUAVTables(1,m);// 1 layer, m mips.
		changed=true;
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format					=uavFormat;
		uav_desc.ViewDimension			=D3D11_UAV_DIMENSION_TEXTURE3D;
		uav_desc.Texture3D.MipSlice		=0;
		uav_desc.Texture3D.WSize		=d;
		uav_desc.Texture3D.FirstWSlice	=0;
		
		if(mipUnorderedAccessViews)
		{
			for(int i=0;i<m;i++)
			{
				uav_desc.Texture3D.MipSlice=i;
				V_CHECK(r->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &mipUnorderedAccessViews[i]));
				uav_desc.Texture3D.WSize/=2;
			}
		}
		uav_desc.Texture3D.WSize	= d;
		if(layerMipUnorderedAccessViews)
		for(int i=0;i<m;i++)
		{
			uav_desc.Texture3D.MipSlice=i;
			V_CHECK(r->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &layerMipUnorderedAccessViews[0][i]));
			uav_desc.Texture3D.WSize/=2;
		}
		if(mipUnorderedAccessViews)
		{
			//if(uavFormat!=altUavFormat)
			{
				uav_desc.Format=altUavFormat;
				uav_desc.Texture3D.MipSlice=0;
				uav_desc.Texture3D.WSize=d;
				V_CHECK(r->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &mipUnorderedAccessViews[m]));
			}
		}
	}
	if(d<=8&&rendertargets&&(!renderTargetViews||!renderTargetViews[0]||!ok))
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
	, bool computable, bool rendertarget, bool depthstencil
	, int num_samples, int aa_quality, bool
	, vec4 clear, float clearDepth, uint clearStencil, bool shared, crossplatform::CompressionFormat compressionFormat,const uint8_t **initData)
{
	return EnsureTexture2DSizeAndFormat(r, w, l,m, f, computable, rendertarget, depthstencil, num_samples, aa_quality, false, clear, clearDepth, clearStencil,compressionFormat,initData);
}

bool Texture::EnsureTexture2DSizeAndFormat(crossplatform::RenderPlatform *r
	, int w, int l,int m
	, crossplatform::PixelFormat f
	, bool computable, bool rendertarget, bool depthstencil
	, int num_samples, int aa_quality, bool
	, vec4 clear, float clearDepth, uint clearStencil
	, crossplatform::CompressionFormat compressionFormat
	, const uint8_t** initData)
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
		FreeUAVTables();
		FreeRTVTables();

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
			if(initData)
			{
				for(size_t i=0;i<m;i++)
				{
					for(size_t j=0;j<arraySize;j++)
					{
						size_t n=i*arraySize+j;
						D3D11_SUBRESOURCE_DATA &data=initialSubresourceData[n];
						data.pSysMem						=initData[n];
						data.SysMemPitch					=mip_width*ByteSizeOfFormatElement(dxgi_format);
						static int uu = 4;
						data.SysMemSlicePitch = data.SysMemPitch*mip_length;
						switch (compressionFormat)
						{
						case crossplatform::CompressionFormat::BC1:
						case crossplatform::CompressionFormat::BC3:
						case crossplatform::CompressionFormat::BC5:
							data.SysMemPitch				= ByteSizeOfFormatElement(dxgi_format)*std::max(1,mip_width / uu);
							data.SysMemSlicePitch			= data.SysMemPitch*std::max(1,mip_length/4);
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
				V_CHECK(renderPlatform->AsD3D11Device()->CreateTexture2D(&desc,initData? initialSubresourceData.data():nullptr,&tex));
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
			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
			ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			srv_desc.Format						=srvFormat;
			srv_desc.ViewDimension				=num_samples>1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels		=m;
			srv_desc.Texture2D.MostDetailedMip	=0;
			SAFE_RELEASE(mainShaderResourceView);
			SAFE_RELEASE(arrayShaderResourceView);
			V_CHECK(pd3dDevice->CreateShaderResourceView(texture,&srv_desc,&mainShaderResourceView));
			SetDebugObjectName(mainShaderResourceView,"dx11::Texture::ensureTexture2DSizeAndFormat mainShaderResourceView");
			
			{
				InitSRVTables(1,textureDesc.MipLevels);
				CreateSRVTables(textureDesc.ArraySize,textureDesc.MipLevels,false,false,textureDesc.SampleDesc.Count>1);
			}
		}
		if(computable&&(!layerMipUnorderedAccessViews||!ok))
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
			ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			uav_desc.Format						=genericDxgiFormat;
			uav_desc.ViewDimension				=D3D11_UAV_DIMENSION_TEXTURE2D;
			uav_desc.Texture2D.MipSlice			=0;
		
			if(m<1)
				m=1;
			InitUAVTables(1,m);
			if(mipUnorderedAccessViews)
			for(int i=0;i<m;i++)
			{
				uav_desc.Texture2D.MipSlice=i;
				V_CHECK(pd3dDevice->CreateUnorderedAccessView(texture, &uav_desc, &mipUnorderedAccessViews[i]));
				SetDebugObjectName(mipUnorderedAccessViews[i],"dx11::Texture::ensureTexture2DSizeAndFormat unorderedAccessView");
			}
			if(layerMipUnorderedAccessViews)
			for(int i=0;i<m;i++)
			{
				uav_desc.Texture2D.MipSlice=i;
				V_CHECK(pd3dDevice->CreateUnorderedAccessView(texture, &uav_desc, &layerMipUnorderedAccessViews[0][i]));
				SetDebugObjectName(layerMipUnorderedAccessViews[0][i],"dx11::Texture::ensureTexture2DSizeAndFormat unorderedAccessView");
			}
		}
		if(texture&&rendertarget&&(!renderTargetViews||!ok))
		{
			InitRTVTables(1,m);
			// Setup the description of the render target view.
			D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
			renderTargetViewDesc.Format				=genericDxgiFormat;
			renderTargetViewDesc.ViewDimension		=num_samples>1?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
			// Create the render target in DX11:
			if(renderTargetViews)
			for(int j=0;j<m;j++)
			{
				renderTargetViewDesc.Texture2D.MipSlice	=j;
				V_CHECK(pd3dDevice->CreateRenderTargetView(texture,&renderTargetViewDesc,&(renderTargetViews[0][j])));
				SetDebugObjectName((renderTargetViews[0][j]),"dx11::Texture::ensureTexture2DSizeAndFormat renderTargetView");
			}
		}
		if(depthstencil&&(!depthStencilView||!ok))
		{
			D3D11_TEX2D_DSV dsv;
			dsv.MipSlice=0;
			D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc;
			depthDesc.ViewDimension		=num_samples>1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
			depthDesc.Format			=dxgi_format;
			depthDesc.Flags				=0;
			depthDesc.Texture2D			=dsv;
			SAFE_RELEASE(depthStencilView);
			V_CHECK(pd3dDevice->CreateDepthStencilView(texture,&depthDesc,&depthStencilView));
		}
		SetDebugObjectName(texture,"ensureTexture2DSizeAndFormat");
	}
	this->depthStencil = depthstencil;
	mips=m;
	arraySize=1;
	return !ok;
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int num,int m,crossplatform::PixelFormat f,bool computable,bool rendertarget,bool depthstencil,bool cubemap,crossplatform::CompressionFormat compressionFormat,const uint8_t **initData)
{
	renderPlatform=r;
	if(m<0||m>16)
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
	InvalidateDeviceObjects();
	//dxgi_format=(DXGI_FORMAT)dx11::RenderPlatform::ToDxgiFormat(pixelFormat,compressionFormat);
	D3D11_TEXTURE2D_DESC desc;

	FreeSRVTables();
	FreeRTVTables();
	FreeUAVTables();
	mips=m;
	arraySize=num;

	width					=w;
	length					=l;
	depth					=1;
	dim						=2;
	desc.Width				=w;
	desc.Height				=l;
	desc.Format				=dxgi_format;
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
	//std::vector<std::vector<uint32_t>> tempData(m*total_num);
	if(initData)
	{
		size_t bytesPerTexel	=crossplatform::GetByteSize(f);
		size_t n=0;
		for(size_t j=0;j<total_num;j++)
		{
			int mip_width=width;
			int mip_length=length;
			for(size_t i=0;i<m;i++)
			{
				/*tempData[n].resize(mip_width*mip_width);
				for(int k=0;k<mip_width*mip_width;k++)
				{
					tempData[n][k]=0xFF00FF00;
				}*/
				D3D11_SUBRESOURCE_DATA &data=initialSubresourceData[n];
				data.pSysMem						=initData[n];
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
						data.SysMemPitch				= (UINT)block_size_bytes*block_width;;//ByteSizeOfFormatElement(dxgi_format)*std::max(1,mip_width / uu);
						data.SysMemSlicePitch			= (UINT)std::max(block_size_bytes,data.SysMemPitch*block_height);//data.SysMemPitch*std::max(1,mip_length/4);
					}
					break;
				default:
					data.SysMemPitch					=mip_width*ByteSizeOfFormatElement(dxgi_format);
					data.SysMemSlicePitch = data.SysMemPitch*mip_length;
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
		V_CHECK(renderPlatform->AsD3D11Device()->CreateTexture2D(&desc,initData? initialSubresourceData.data():nullptr,&tex));
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
	InitSRVTables(total_num,m);
	CreateSRVTables(num,m,cubemap);
	
	if(computable)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format							=dxgi_format;
		uav_desc.ViewDimension					=D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uav_desc.Texture2DArray.ArraySize		=total_num;
		uav_desc.Texture2DArray.FirstArraySlice	=0;
		InitUAVTables(total_num,m);
		if(mipUnorderedAccessViews)
		for(int i=0;i<m;i++)
		{
			uav_desc.Texture2DArray.MipSlice=i;
			V_CHECK(renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &mipUnorderedAccessViews[i]));
			SetDebugObjectName(mipUnorderedAccessViews[i],"dx11::Texture::ensureTexture2DSizeAndFormat unorderedAccessView");
		}
		if (layerMipUnorderedAccessViews)
			for (int i = 0; i < total_num; i++)
				for (int j = 0; j < m; j++)
				{
					uav_desc.Texture2DArray.FirstArraySlice = i;
					uav_desc.Texture2DArray.ArraySize=1;
					uav_desc.Texture2DArray.MipSlice = j;
					V_CHECK(renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &layerMipUnorderedAccessViews[i][j]));
					SetDebugObjectName(layerMipUnorderedAccessViews[i][j],"dx11::Texture::ensureTexture2DSizeAndFormat unorderedAccessView");
				}
	}

	if(rendertarget)
	{
		InitRTVTables(total_num, m);
		// Create the multi-face render target view
		D3D11_RENDER_TARGET_VIEW_DESC DescRT;
		DescRT.Format							=dxgi_format;
		DescRT.ViewDimension					=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		DescRT.Texture2DArray.FirstArraySlice	=0;
		DescRT.Texture2DArray.ArraySize			=total_num;
		if(renderTargetViews)
		for(int i=0;i<total_num;i++)
		{
			DescRT.Texture2DArray.FirstArraySlice = i;
			DescRT.Texture2DArray.ArraySize = 1;
			for(int j=0;j<m;j++)
			{
				DescRT.Texture2DArray.MipSlice			=j;
				V_CHECK(renderPlatform->AsD3D11Device()->CreateRenderTargetView(pArrayTexture, &DescRT, &(renderTargetViews[i][j])));
			}
		}
	}
	SetDebugObjectName(texture,"ensureTextureArraySizeAndFormat");

	mips=m;
	arraySize=num;
	this->cubemap=cubemap;
	return true;
}

void Texture::CreateSRVTables(int num,int m,bool cubemap,bool volume,bool msaa)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );

	if(!volume)
	{
		SRVDesc.Format						=TypelessToSrvFormat(genericDxgiFormat);
		SRVDesc.ViewDimension				=D3D11_SRV_DIMENSION_TEXTURE2D;
		int total_num						=cubemap?6*num:num;
		if(cubemap)
		{
			if(num<=1)
			{
				SRVDesc.ViewDimension				=D3D11_SRV_DIMENSION_TEXTURECUBE;
				SRVDesc.TextureCube.MipLevels		=m;
				SRVDesc.TextureCube.MostDetailedMip =0;
			}
			else
			{
				SRVDesc.ViewDimension						=D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
				SRVDesc.TextureCubeArray.MipLevels			=m;
				SRVDesc.TextureCubeArray.MostDetailedMip	=0;
				SRVDesc.TextureCubeArray.First2DArrayFace	=0;
				SRVDesc.TextureCubeArray.NumCubes			=num;
			}
		}
		else if(num>1)
		{
			SRVDesc.ViewDimension					=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			SRVDesc.Texture2DArray.ArraySize		=num;
			SRVDesc.Texture2DArray.FirstArraySlice	=0;		
			SRVDesc.Texture2DArray.MipLevels		=m;
			SRVDesc.Texture2DArray.MostDetailedMip	=0;
		}
		else if(msaa)
		{
			SRVDesc.ViewDimension					=D3D11_SRV_DIMENSION_TEXTURE2DMS;
		}
		else
		{ 
			SRVDesc.Texture2D.MipLevels				=m;
			SRVDesc.Texture2D.MostDetailedMip		 =0;
		}
		SAFE_RELEASE(mainShaderResourceView);
		V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture,&SRVDesc,&mainShaderResourceView));
	
		SAFE_RELEASE(arrayShaderResourceView);
		if(cubemap)
		{
			SRVDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			SRVDesc.Texture2DArray.ArraySize=total_num;
			SRVDesc.Texture2DArray.FirstArraySlice=0;
			SRVDesc.Texture2DArray.MipLevels=m;
			SRVDesc.Texture2DArray.MostDetailedMip=0;
			V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture,&SRVDesc, &arrayShaderResourceView));
		}
		if(mainMipShaderResourceViews)
		{
			if (cubemap)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				SRVDesc.TextureCube.MipLevels = 1;
				SRVDesc.TextureCube.MostDetailedMip;
				for(int j=0;j<m;j++)
				{
					SRVDesc.Texture3D.MostDetailedMip = j;
					V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture, &SRVDesc, &mainMipShaderResourceViews[j]));
				}
			}
			else
			{
				if(SRVDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D)
					SRVDesc.Texture2D.MipLevels=1;
				if(SRVDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
					SRVDesc.Texture2DArray.MipLevels=1;
				for (int j = 0; j < m; j++)
				{
					if(SRVDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D)
						SRVDesc.Texture2D.MostDetailedMip=j;
					if(SRVDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
						SRVDesc.Texture2DArray.MostDetailedMip=j;
					V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture, &SRVDesc, &mainMipShaderResourceViews[j]));
				}
			}
		}
		if(layerShaderResourceViews||layerMipShaderResourceViews)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC face_srv_desc;
			ZeroMemory(&face_srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			face_srv_desc.Format					= SRVDesc.Format;
			face_srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			if (cubemap)
				face_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
			for(int i=0;i<total_num;i++)
			{
				face_srv_desc.Texture2DArray.ArraySize=1;
				face_srv_desc.Texture2DArray.FirstArraySlice=i;
				face_srv_desc.Texture2DArray.MipLevels=m;
				face_srv_desc.Texture2DArray.MostDetailedMip=0;
				if(layerShaderResourceViews)
					V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture, &face_srv_desc, &layerShaderResourceViews[i]));
				if(layerMipShaderResourceViews)
					for(int j=0;j<m;j++)
					{
						face_srv_desc.Texture2DArray.MipLevels=1;
						face_srv_desc.Texture2DArray.MostDetailedMip=j;
						V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture, &face_srv_desc, &layerMipShaderResourceViews[i][j]));
					}
			}
		}
	}
	else
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						= TypelessToSrvFormat(dxgi_format);
		srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Texture3D.MipLevels		= m;
		srv_desc.Texture3D.MostDetailedMip	= 0;
		V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture,&srv_desc,&mainShaderResourceView));
		if(mainMipShaderResourceViews)
		for(int j=0;j<m;j++)
		{
			srv_desc.Texture3D.MipLevels=1;
			srv_desc.Texture3D.MostDetailedMip=j;
			V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture, &srv_desc, &mainMipShaderResourceViews[j]));
		}
	}
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

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						= f;
		srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE1D;
		srv_desc.Texture1D.MipLevels		= m;
		srv_desc.Texture1D.MostDetailedMip	= 0;
		V_CHECK(pd3dDevice->CreateShaderResourceView(texture,&srv_desc,&mainShaderResourceView));
		if(mainMipShaderResourceViews)
		for(int j=0;j<mips;j++)
		{
			srv_desc.Texture1D.MipLevels=1;
			srv_desc.Texture1D.MostDetailedMip=j;
			V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(texture, &srv_desc, &mainMipShaderResourceViews[j]));
		}
	}
	if(computable&&(!layerMipUnorderedAccessViews||!ok))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format				= f;
		uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE1D;
		uav_desc.Texture1D.MipSlice	= 0;
		
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format							=dxgi_format;
		uav_desc.ViewDimension					=D3D11_UAV_DIMENSION_TEXTURE1D;
		InitUAVTables(1,m);
		if(layerMipUnorderedAccessViews)
		for(int j=0;j<m;j++)
		{
			uav_desc.Texture1D.MipSlice=j;
			V_CHECK(renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(texture, &uav_desc, &layerMipUnorderedAccessViews[0][j]));
			SetDebugObjectName(layerMipUnorderedAccessViews[0][j],"dx11::Texture::ensureTexture1DSizeAndFormat unorderedAccessView");
		}
	}
	mips=m;
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext &deviceContext, float depthClear, int stencilClear)
{
	if (!depthStencil || !depthStencilView)
		SIMUL_CERR << "Attempting to clear a texture that is not a Depth Stencil" << std::endl;

	deviceContext.asD3D11DeviceContext()->ClearDepthStencilView(depthStencilView, (D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL), depthClear, stencilClear);

}

void Texture::GenerateMips(crossplatform::GraphicsDeviceContext &deviceContext)
{
	// We can't detect if this has worked or not.
	if(renderTargetViews&&*renderTargetViews)
		deviceContext.asD3D11DeviceContext()->GenerateMips(AsD3D11ShaderResourceView());
	else
		SIMUL_CERR<<"Can't use GenerateMips on texture "<<this<<" not created as rendertarget.\n";
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

void Texture::activateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext,int array_index,int mip)
{
	if(!deviceContext.asD3D11DeviceContext())
		return;
	last_context=deviceContext.asD3D11DeviceContext();
	if(array_index<0)
		array_index=0;
	if(mip<0)
		mip=0;
	if(mip>=mips)
		return;
	D3D11_VIEWPORT viewport;
	if(renderTargetViews)
	{
		last_context->OMSetRenderTargets(1,&(renderTargetViews[array_index][mip]),NULL);
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

	targetsAndViewport.m_rt[0]=(renderTargetViews[array_index][mip]);
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
	if(!mainShaderResourceView)
		return 0;
	bool msaa=false;
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	mainShaderResourceView->GetDesc(&desc);
	msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	if(!msaa)
		return 0;
	D3D11_TEXTURE2D_DESC t2d_desc;
	((ID3D11Texture2D*)texture)->GetDesc(&t2d_desc);
	return t2d_desc.SampleDesc.Count;
}

void Texture::InitSRVTables(int l,int m)
{
	if(l>1)
		layerShaderResourceViews=new ID3D11ShaderResourceView*[l];				// SRV's for each layer, including all mips
	else
		layerShaderResourceViews=nullptr;
	if(m>1)
		mainMipShaderResourceViews=new ID3D11ShaderResourceView*[m];			// SRV's for the whole texture at different mips.
	else
		mainMipShaderResourceViews=nullptr;
	if(l>1&&m>1)
	{
		layerMipShaderResourceViews=new ID3D11ShaderResourceView**[l];			// SRV's for each layer at different mips.
		for(int i=0;i<l;i++)
		{
			layerMipShaderResourceViews[i]=new ID3D11ShaderResourceView*[m];	// SRV's for each layer at different mips.
		}
	}
	else
		layerMipShaderResourceViews=nullptr;
}

void Texture::FreeSRVTables()
{
	SAFE_RELEASE(arrayShaderResourceView);
	SAFE_RELEASE(mainShaderResourceView);
	int total_num=cubemap?arraySize*6:arraySize;
	for(int i=0;i<total_num;i++)
	{
		if(layerShaderResourceViews)
			SAFE_RELEASE(layerShaderResourceViews[i]);				// SRV's for each layer, including all mips
		if(layerMipShaderResourceViews)
		{
			for(int j=0;j<mips;j++)
			{
				SAFE_RELEASE(layerMipShaderResourceViews[i][j]);	
			}
			delete [] layerMipShaderResourceViews[i];
		}
	}
	delete [] layerShaderResourceViews;
	layerShaderResourceViews=nullptr;
	delete [] layerMipShaderResourceViews;
	layerMipShaderResourceViews=nullptr;
	if(mainMipShaderResourceViews)
	{
		for(int j=0;j<mips;j++)
		{
			SAFE_RELEASE(mainMipShaderResourceViews[j]);	
		}
	}
	delete [] mainMipShaderResourceViews;
	mainMipShaderResourceViews=nullptr;
}
