#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/RuntimeError.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <algorithm>

using namespace platform;
using namespace crossplatform;
SamplerState::SamplerState():default_slot(-1),renderPlatform(nullptr)
{
}

SamplerState::~SamplerState()
{
}

Texture::Texture(const char *n)
				:width(0)
				,length(0)
				,depth(0)
				,arraySize(0)
				,dim(0)
				,mips(1)
				,pixelFormat(crossplatform::UNKNOWN)
				,renderPlatform(nullptr)
				,cubemap(false)
				,computable(false)
				,renderTarget(false)
				,external_texture(false)
				,depthStencil(false)
				,unfenceable(false)
				,yuvLayerIndex(-1)
{
	if(n)
		name=n;
}

Texture::~Texture()
{
	InvalidateDeviceObjects();
}

bool Texture::InitFromExternalTexture(crossplatform::RenderPlatform *renderPlatform, const TextureCreate *textureCreate)
{
	return InitFromExternalTexture2D(renderPlatform, textureCreate->external_texture, textureCreate->srv, textureCreate->w, textureCreate->l, textureCreate->f, textureCreate->make_rt, textureCreate->setDepthStencil, textureCreate->need_srv, textureCreate->numOfSamples);
}

void Texture::activateRenderTarget(GraphicsDeviceContext &deviceContext,int array_index,int mip_index )
{
	if (array_index == -1)
	{
		array_index = 0;
	}
	if (mip_index == -1)
	{
		mip_index = 0;
	}
	targetsAndViewport.num							=1;
	targetsAndViewport.m_rt[0]						=nullptr;
	targetsAndViewport.textureTargets[0].texture	=this;
	targetsAndViewport.textureTargets[0].mip		=mip_index;
	targetsAndViewport.textureTargets[0].layer		=array_index;
	targetsAndViewport.textureTargets[0].layerCount =NumFaces();
	targetsAndViewport.m_dt							=nullptr;
	targetsAndViewport.viewport.x					=0;
	targetsAndViewport.viewport.y					=0;
	targetsAndViewport.viewport.w					=std::max(1, (width >> mip_index));
	targetsAndViewport.viewport.h					=std::max(1, (length >> mip_index));
	/*deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

	// Cache it:
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);*/
	deviceContext.renderPlatform->ActivateRenderTargets(deviceContext,&targetsAndViewport);
}

void Texture::deactivateRenderTarget(GraphicsDeviceContext &deviceContext)
{
	deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
}

bool Texture::IsCubemap() const
{
	return cubemap;
}

void Texture::SetName(const char *n)
{
	name=n;
}

const char* Texture::GetName() const
{
	return name.c_str();
}

unsigned long long Texture::GetFence(DeviceContext &deviceContext) const
{
	const auto &i=deviceContext.contextState.fenceMap.find((const Texture*)this);
	if(i!=deviceContext.contextState.fenceMap.end())
		return i->second.label;
	return 0;
}

void Texture::SetFence(DeviceContext &deviceContext,unsigned long long f)
{
	auto i=deviceContext.contextState.fenceMap.find(this);
	if(i!=deviceContext.contextState.fenceMap.end())
	if(i->second.label!=0)
	{
		// This is probably ok. We might be adding to a different part of a texture.
	//	SIMUL_CERR<<"Setting fence when the last one is not cleared yet\n"<<std::endl;
	}
	auto &fence=deviceContext.contextState.fenceMap[this];
	fence.label=f;
	fence.texture=this;
}


void Texture::ClearFence(DeviceContext &deviceContext)
{
	auto i=deviceContext.contextState.fenceMap.find(this);
	if(i!=deviceContext.contextState.fenceMap.end())
		deviceContext.contextState.fenceMap.erase(i);
}

void Texture::InvalidateDeviceObjects()
{
	if(renderPlatform)
		renderPlatform->InvalidatingTexture(this);
	shouldGenerateMips=false;
	width=length=depth=arraySize=dim=mips=0;
	pixelFormat=PixelFormat::UNKNOWN;
	renderPlatform=nullptr;
	textureUploadComplete=true;
}

bool Texture::EnsureTexture(crossplatform::RenderPlatform* r, crossplatform::TextureCreate* tc)
{
	bool res=false;
	if(tc->name)
		name=tc->name;
	if (tc->vidTexType != VideoTextureType::NONE)
		res = ensureVideoTexture(r, tc->w, tc->l, tc->f, tc->vidTexType);
	else if (tc->d < 2&&tc->arraysize==1&&!tc->cubemap)
		res=ensureTexture2DSizeAndFormat(r, tc->w, tc->l, tc->mips, tc->f,tc->initialData, tc->computable , tc->make_rt , tc->setDepthStencil , tc->numOfSamples , tc->aa_quality , false ,tc->clear, tc->clearDepth , tc->clearStencil,false
		,tc->compressionFormat);
	else if(tc->d<2)
		res=ensureTextureArraySizeAndFormat( r, tc->w, tc->l, tc->arraysize, tc->mips, tc->f, tc->initialData, tc->computable , tc->make_rt, tc->setDepthStencil, tc->cubemap, tc->compressionFormat);
	else
		res=ensureTexture3DSizeAndFormat(r, tc->w, tc->l, tc->d, tc->f, tc->computable , tc->mips , tc->make_rt) ;

	return res;
}

bool Texture::TranslateLoadedTextureData(void*& target, const void* src, size_t size, int& x, int& y, int& num_channels, int req_num_channels)
{
	target = stbi_load_from_memory((const unsigned char*)src, (int)size, &x, &y, &num_channels, 4);
	stbi_loaded = true;
	return(target!=nullptr);
}

void Texture::FreeTranslatedTextureData(void* data)
{
	if (stbi_loaded)
	{
		stbi_image_free(data);
	}
}

uint32_t Texture::CalculateSubresourceIndex(uint32_t MipSlice, uint32_t ArraySlice, uint32_t PlaneSlice, uint32_t MipLevels, uint32_t ArraySize)
{
	return MipSlice + (ArraySlice * MipLevels) + (PlaneSlice * MipLevels * ArraySize);
}

uint3 Texture::CalculateSubresourceSlices(uint32_t Index, uint32_t MipSlice, uint32_t ArraySlice)
{
	uint z = Index / (MipSlice * ArraySlice);
	Index -= (z * MipSlice * ArraySlice);
	uint y = Index / MipSlice;
	uint x = Index % MipSlice;
	return uint3(x, y, z);
}

bool Texture::ensureTexture2DSizeAndFormat(RenderPlatform* renderPlatform, int w, int l, int m
	, PixelFormat f, bool computable, bool rendertarget, bool depthstencil, int num_samples, int aa_quality, bool wrap,
	vec4 clear, float clearDepth, uint clearStencil, bool shared
	, crossplatform::CompressionFormat compressionFormat, const uint8_t** initData)
{
	std::shared_ptr< std::vector<std::vector<uint8_t>>> data;
	// initData is ordered by slice (outer) and mip (inner)
	if (initData)
	{
		data = std::make_shared<std::vector<std::vector<uint8_t>>>();
		size_t bytesPerTexel = crossplatform::GetByteSize(f);
		size_t n = 0;
		int total_num = cubemap ? 6 :1;
		size_t SysMemPitch = 0;
		size_t SysMemSlicePitch=0;

		for (size_t j = 0; j < total_num; j++)
		{
			int mip_width = width;
			int mip_length = length;
			for (size_t i = 0; i < m; i++)
			{
				const uint8_t *src = initData[n];
				static int uu = 4;
				switch (compressionFormat)
				{
				case crossplatform::CompressionFormat::BC1:
				case crossplatform::CompressionFormat::BC3:
				case crossplatform::CompressionFormat::BC5:
				{
					size_t block_width = std::max(1, (mip_width + 3) / 4);
					size_t block_height = std::max(1, (mip_length + 3) / 4);
					size_t block_size_bytes = (bytesPerTexel == 4) ? 16 : 8;
					SysMemPitch = (size_t)(block_size_bytes * block_width);
					SysMemSlicePitch = (size_t)std::max(block_size_bytes, SysMemPitch * block_height);
				}
				break;
				default:
					SysMemPitch = mip_width * crossplatform::GetByteSize(f);;
					SysMemSlicePitch = SysMemPitch * mip_length;
					break;
				};
				(*data)[n].resize(SysMemSlicePitch);
				uint8_t* target = (*data)[n].data();
				memcpy(target,src, SysMemSlicePitch);
				mip_width = (mip_width + 1) / 2;
				mip_length = (mip_length + 1) / 2;
				n++;
			}
		}
	}
	return ensureTexture2DSizeAndFormat(renderPlatform, w, l, m
		, f, data, computable, rendertarget, depthstencil, num_samples, aa_quality, wrap,
		clear, clearDepth, clearStencil, shared
		, compressionFormat);
}