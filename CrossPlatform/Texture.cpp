#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/RuntimeError.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <algorithm>

using namespace simul;
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
				,textureLoadComplete(true)
				,cubemap(false)
				,computable(false)
				,renderTarget(false)
				,depthStencil(false)
				,external_texture(false)
				,unfenceable(false)		
{
	if(n)
		name=n;
}

Texture::~Texture()
{
// TODO: Does this cause crashes on exit?
	InvalidateDeviceObjects();
}

bool Texture::InitFromExternalTexture(crossplatform::RenderPlatform *renderPlatform, const TextureCreate *textureCreate)
{
	return InitFromExternalTexture2D(renderPlatform, textureCreate->external_texture, textureCreate->w, textureCreate->l, textureCreate->f, textureCreate->make_rt, textureCreate->setDepthStencil, textureCreate->numOfSamples);
}

void Texture::activateRenderTarget(GraphicsDeviceContext &deviceContext, TextureView textureView)
{
	const int& array_index = textureView.subresourceRange.baseArrayLayer;
	const int& mip_index = textureView.subresourceRange.baseMipLevel;
	if (textureView.type == ShaderResourceType::UNKNOWN)
		textureView.type = GetShaderResourceTypeForRTVAndDSV();
	
	targetsAndViewport.num							=1;
	targetsAndViewport.m_rt[0]						=nullptr;
	targetsAndViewport.textureTargets[0].texture	=this;
	targetsAndViewport.textureTargets[0].mip		=mip_index;
	targetsAndViewport.textureTargets[0].layer		=array_index;
	targetsAndViewport.m_dt							=nullptr;
	targetsAndViewport.viewport.x					=0;
	targetsAndViewport.viewport.y					=0;
	targetsAndViewport.viewport.w					=std::max(1, (width >> mip_index));
	targetsAndViewport.viewport.h					=std::max(1, (length >> mip_index));
	// Cache it:
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
	deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);
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
	textureLoadComplete=true;
}

bool Texture::EnsureTexture(crossplatform::RenderPlatform* r, crossplatform::TextureCreate* tc)
{
	bool res=false;

	if (tc->vidTexType != VideoTextureType::NONE)
		res = ensureVideoTexture(r, tc->w, tc->l, tc->f, tc->vidTexType);
	else if (tc->d == 1&&tc->arraysize==1)
		res= ensureTexture2DSizeAndFormat(r, tc->w, tc->l, tc->mips, tc->f, tc->computable , tc->make_rt , tc->setDepthStencil , tc->numOfSamples , tc->aa_quality , false ,tc->clear, tc->clearDepth , tc->clearStencil );
	else if(tc->d==1)
		res=ensureTextureArraySizeAndFormat( r, tc->w, tc->l, tc->arraysize, tc->mips, tc->f, tc->computable , tc->make_rt, tc->cubemap ) ;
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

bool Texture::ValidateTextureView(const TextureView& textureView)
{
	const ShaderResourceType& type = textureView.type;
	const SubresourceRange& subres = textureView.subresourceRange;
	const uint32_t& layers = (uint32_t)NumFaces();
	const uint32_t& mips = (uint32_t)this->mips;
	const int& samples = GetSampleCount();
	bool ok = true;
	
	ok &= (subres.baseMipLevel < mips);
	if (subres.mipLevelCount != -1)
		ok &= (subres.baseMipLevel + subres.mipLevelCount <= mips);

	SIMUL_ASSERT_WARN_ONCE(ok, "SubresourceRange specifices a range outside the texture's mip levels.");

	ok &= (subres.baseArrayLayer < layers);
	if (subres.arrayLayerCount != -1)
		ok &= (subres.baseArrayLayer + subres.arrayLayerCount <= layers);

	SIMUL_ASSERT_WARN_ONCE(ok, "SubresourceRange specifices a range outside the texture's array layers.");

	if (type == ShaderResourceType::TEXTURE_1D || type == ShaderResourceType::RW_TEXTURE_1D)
		ok &= dim == 1 && width > 0;
	if (type == ShaderResourceType::TEXTURE_1D_ARRAY || type == ShaderResourceType::RW_TEXTURE_1D_ARRAY)
		ok &= dim == 1 && width > 0 && layers > 1;
	if (type == ShaderResourceType::TEXTURE_2D || type == ShaderResourceType::RW_TEXTURE_2D)
		ok &= dim == 2 && width > 0 && length > 0;
	if (type == ShaderResourceType::TEXTURE_2D_ARRAY || type == ShaderResourceType::RW_TEXTURE_2D_ARRAY)
		ok &= dim == 2 && width > 0 && length > 0 && layers > 1;
	if (type == ShaderResourceType::TEXTURE_2DMS)
		ok &= dim == 2 && width > 0 && length > 0 && samples > 1;
	if (type == ShaderResourceType::TEXTURE_2DMS_ARRAY)
		ok &= dim == 2 && width > 0 && length > 0 && samples > 1 && layers > 1;
	if (type == ShaderResourceType::TEXTURE_3D || type == ShaderResourceType::RW_TEXTURE_3D)
		ok &= dim == 3 && width > 0 && length > 0 && depth > 0;
	if (type == ShaderResourceType::TEXTURE_CUBE || type == ShaderResourceType::TEXTURE_CUBE_ARRAY)
		ok &= dim == 2 && width > 0 && length > 0 && layers >= 6 && cubemap;
	
	SIMUL_ASSERT_WARN_ONCE(ok, "ShaderResourceType is incompatible with the texture.");

	return ok;
}

ShaderResourceType Texture::GetShaderResourceTypeForRTVAndDSV()
{
	const uint32_t& layers = (uint32_t)NumFaces();
	const int& samples = GetSampleCount();

	ShaderResourceType type = ShaderResourceType::UNKNOWN;

	if (dim == 1)
		type |= ShaderResourceType::TEXTURE_1D;
	else if (dim == 2)
		type |= ShaderResourceType::TEXTURE_2D;
	else if (dim == 3)
		type |= ShaderResourceType::TEXTURE_3D;

	if (layers > 1)
		type |= ShaderResourceType::ARRAY;
	if (samples > 1)
		type |= ShaderResourceType::MS;
		
	return type;
}