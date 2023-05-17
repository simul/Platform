#include "Platform/Core/FileLoader.h"
#include "Texture.h"
#include "RenderPlatform.h"
#include "DeviceManager.h"
#include "Effect.h"
#include <algorithm>

using namespace simul;
using namespace platform;
using namespace vulkan;

SamplerState::SamplerState()
{
}

SamplerState::~SamplerState()
{
	InvalidateDeviceObjects();
}

void SamplerState::Init(crossplatform::RenderPlatform*r,crossplatform::SamplerStateDesc* desc)
{
	InvalidateDeviceObjects();
	
	renderPlatform=r;
	vk::Device *vulkanDevice=r->AsVulkanDevice();
	vk::SamplerCreateInfo samplerCreateInfo=vk::SamplerCreateInfo();
	
	samplerCreateInfo
			.setMagFilter(RenderPlatform::toVulkanMaxFiltering(desc->filtering))
			.setMinFilter(RenderPlatform::toVulkanMinFiltering(desc->filtering))
			.setMipmapMode(RenderPlatform::toVulkanMipmapMode(desc->filtering))
			.setAddressModeU(RenderPlatform::toVulkanWrapping(desc->x))
			.setAddressModeV(RenderPlatform::toVulkanWrapping(desc->y))
			.setAddressModeW(RenderPlatform::toVulkanWrapping(desc->z))
			.setMipLodBias(0.0f)
			.setMaxLod(32.0f)
			.setAnisotropyEnable(VK_TRUE)
			.setMaxAnisotropy(16)
			.setCompareEnable(VK_FALSE)
			.setCompareOp(vk::CompareOp::eNever)
			.setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
			.setUnnormalizedCoordinates(VK_FALSE);
	SIMUL_VK_CHECK(vulkanDevice->createSampler(&samplerCreateInfo,nullptr,&mSampler));
	SetVulkanName(renderPlatform,mSampler,"Sampler ");
}

  vk::Sampler *SamplerState::AsVulkanSampler() 
  {
	  return &mSampler;
  }

void SamplerState::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	if(!vulkanDevice)
		return;
	vulkan::RenderPlatform* r = static_cast<vulkan::RenderPlatform*>(renderPlatform);
	r->PushToReleaseManager(mSampler);
	renderPlatform=nullptr;
}

Texture::Texture()
{
}

Texture::~Texture()
{
	InvalidateDeviceObjects();
}

void Texture::SetName(const char* n)
{
	if (!n)
	{
		name = "texture";
	}
	else
	{
		name = n;
	}
	
}

void Texture::LoadFromFile(crossplatform::RenderPlatform* r, const char* pFilePathUtf8, bool gen_mips)
{
	InvalidateDeviceObjects();
	std::vector<std::string> texture_files;
	texture_files.push_back(pFilePathUtf8);
	LoadTextureArray(r,texture_files,gen_mips?0:1);
}

void Texture::LoadTextureArray(crossplatform::RenderPlatform* r, const std::vector<std::string>& texture_files, bool gen_mips)
{
	InvalidateDeviceObjects();
	renderPlatform= r;
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vulkan::RenderPlatform* vr = static_cast<vulkan::RenderPlatform*>(renderPlatform);
	for(auto i:loadedTextures)
	{
		vr->PushToReleaseManager(i.buffer);
		vr->PushToReleaseManager(i.mem);
	}
	loadedTextures.resize(texture_files.size());
	for (unsigned int i = 0; i < texture_files.size(); i++)
	{
		LoadTextureData(loadedTextures[i],texture_files[i].c_str());
	}
	int w= loadedTextures[0].x;
	int l= loadedTextures[0].y;
	if(w*l==0)
	{
	// don't try to complete the load with empty data.
		textureLoadComplete=true;
		return;
	}
	size_t num= loadedTextures.size();
	int m=1;
	if(gen_mips)
		m=100;
	m = std::min(m, int(floor(log2(std::max(w, l)))) +1);
	m=std::min(16,std::max(1,m));
	name="Texture array ";
	for(auto m:texture_files)
	{
		name+=m+",";
	}
	if(num<=1)
		ensureTexture2DSizeAndFormat(r,w,l, m,crossplatform::PixelFormat::RGBA_8_UNORM,false,false,false,1,0,false,vec4(0.f,0.f,0.f,0.f),1.F,0);
	else
		ensureTextureArraySizeAndFormat(r,(int)w,(int)l,(int)num,(int)m,crossplatform::PixelFormat::RGBA_8_UNORM,false,false,false);
	textureLoadComplete=false;
}

bool Texture::IsValid() const
{
	return (width!=0);
}

void Texture::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	vulkan::RenderPlatform* r = static_cast<vulkan::RenderPlatform*>(renderPlatform);
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	if(vulkanDevice)
	{
		InvalidateDeviceObjectsExceptLoaded();
		for(auto i:loadedTextures)
		{
			r->PushToReleaseManager(i.buffer);
			r->PushToReleaseManager(i.mem);
		}
		loadedTextures.clear();
	}
	crossplatform::Texture::InvalidateDeviceObjects();
}

void Texture::InvalidateDeviceObjectsExceptLoaded()
{
	if(!renderPlatform)
		return;
	vulkan::RenderPlatform *r= static_cast<vulkan::RenderPlatform * >(renderPlatform);

	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	if(vulkanDevice)
	{
		if(!external_texture)
		{
			r->PushToReleaseManager(mImage);
			r->PushToReleaseManager(mMem);
		}
		r->PushToReleaseManager(mBuffer);

		for (auto colourImageView : colourImageViews)
		{
			r->PushToReleaseManager(*colourImageView.second);
			delete colourImageView.second;
		}
		colourImageViews.clear();

		for (auto depthImageView : depthImageViews)
		{
			r->PushToReleaseManager(*depthImageView.second);
			delete depthImageView.second;
		}
		depthImageViews.clear();
	}
}

void Texture::FinishLoading(crossplatform::DeviceContext& deviceContext)
{
	if (textureLoadComplete)
		return;
	SIMUL_ASSERT(loadedTextures.size() != 0)

		vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;

	if (GetSampleCount() > 1)
	{
		AssumeLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePreinitialized,
			mCurrentImageLayout, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader);
	}
	else
	{
		SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, mCurrentImageLayout,
			vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer);
		for (int i = 0; i < loadedTextures.size(); i++)
		{
			LoadedTexture& lt = loadedTextures[i];
			auto const subresource = vk::ImageSubresourceLayers()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(0)
				.setBaseArrayLayer(i)
				.setLayerCount(1);

			auto const copy_region =
				vk::BufferImageCopy()
				.setBufferOffset(0)
				.setBufferRowLength(lt.x)
				.setBufferImageHeight(lt.y)
				.setImageSubresource(subresource)
				.setImageOffset({ 0, 0, 0 })
				.setImageExtent({ (uint32_t)lt.x, (uint32_t)lt.y, 1 });

			commandBuffer->copyBufferToImage(lt.buffer, mImage, vk::ImageLayout::eTransferDstOptimal, 1, &copy_region);
		}
		if (mips > 1)
		{
			int srcWidth = width, srcLength = length;
			vk::ImageBlit blit = vk::ImageBlit();
			blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
			blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = arraySize;
			blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
			blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = arraySize;
			for (int i = 0; i < mips; i++)
			{
				blit.srcSubresource.mipLevel = i;
				blit.dstSubresource.mipLevel = i + 1;
				int dstWidth = srcWidth > 1 ? srcWidth / 2 : 1;
				int dstLength = srcLength > 1 ? srcLength / 2 : 1;
				blit.srcOffsets[1] = vk::Offset3D(srcWidth, srcLength, 1);
				blit.dstOffsets[1] = vk::Offset3D(dstWidth, dstLength, 1);
				SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
					vk::PipelineStageFlagBits::eTransfer, { i, 1, 0, -1 });
				if (i < mips - 1)
					commandBuffer->blitImage(mImage, vk::ImageLayout::eTransferSrcOptimal,
						mImage, vk::ImageLayout::eTransferDstOptimal,
						1, &blit,
						vk::Filter::eLinear);
				srcWidth = dstWidth;
				srcLength = dstLength;
			}
			AssumeLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferSrcOptimal,
				mCurrentImageLayout, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader);
		}
		else
		{
			AssumeLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal,
				mCurrentImageLayout, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader);
		}
	}
	for (unsigned int i = 0; i < loadedTextures.size(); i++)
	{
		FreeTranslatedTextureData(loadedTextures[i].data);
	}

	textureLoadComplete = true;
}

vk::ImageView* Texture::AsVulkanImageView(const crossplatform::TextureView& textureView)
{
	if(!textureLoadComplete)
	{
		return nullptr;
	}

	if (!ValidateTextureView(textureView))
		return nullptr;

	uint64_t hash = textureView.GetHash();
	if (depthStencil)
	{
		if (depthImageViews.find(hash) != depthImageViews.end())
			return depthImageViews[hash];
	}
	else
	{
		if (colourImageViews.find(hash) != colourImageViews.end())
			return colourImageViews[hash];
	}

	vk::ImageAspectFlagBits aspect = depthStencil ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
	uint32_t baseMipLevel = textureView.subresourceRange.baseMipLevel;
	uint32_t mipLevelCount = textureView.subresourceRange.mipLevelCount == -1 ? mips : textureView.subresourceRange.mipLevelCount;
	uint32_t baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	uint32_t arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() : textureView.subresourceRange.arrayLayerCount;

	const crossplatform::ShaderResourceType& type = textureView.type;
	vk::ImageViewType viewType;
	if (type == crossplatform::ShaderResourceType::TEXTURE_1D || type == crossplatform::ShaderResourceType::RW_TEXTURE_1D)
		viewType = vk::ImageViewType::e1D;
	else if (type == crossplatform::ShaderResourceType::TEXTURE_1D_ARRAY || type == crossplatform::ShaderResourceType::RW_TEXTURE_1D_ARRAY)
		viewType = vk::ImageViewType::e1DArray;	
	else if (type == crossplatform::ShaderResourceType::TEXTURE_2D || type == crossplatform::ShaderResourceType::RW_TEXTURE_2D || type == crossplatform::ShaderResourceType::TEXTURE_2DMS)
		viewType = vk::ImageViewType::e2D;	
	else if (type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY || type == crossplatform::ShaderResourceType::RW_TEXTURE_2D_ARRAY || type == crossplatform::ShaderResourceType::TEXTURE_2DMS_ARRAY)
		viewType = vk::ImageViewType::e2DArray;	
	else if (type == crossplatform::ShaderResourceType::TEXTURE_3D || type == crossplatform::ShaderResourceType::RW_TEXTURE_3D)
		viewType = vk::ImageViewType::e3D;	
	else if (type == crossplatform::ShaderResourceType::TEXTURE_CUBE)
		viewType = vk::ImageViewType::eCube;	
	else if (type == crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY)
		viewType = vk::ImageViewType::eCubeArray;	
	else
		SIMUL_BREAK_ONCE("Unsupported crossplatform::ShaderResourceType.");


	vk::ImageViewCreateInfo imageViewCI;
	imageViewCI.pNext = nullptr;
	imageViewCI.flags = vk::ImageViewCreateFlagBits(0);
	imageViewCI.image = mImage;
	imageViewCI.viewType = viewType;
	imageViewCI.format = vulkan::RenderPlatform::ToVulkanFormat(pixelFormat);
	imageViewCI.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
	imageViewCI.subresourceRange = { aspect, baseMipLevel, mipLevelCount, baseArrayLayer, arrayLayerCount };

	vk::ImageView* imageView = new vk::ImageView();
	SIMUL_VK_CHECK(renderPlatform->AsVulkanDevice()->createImageView(&imageViewCI, nullptr, imageView));
	SetVulkanName(renderPlatform, *imageView, (name + " imageView").c_str());

	if (depthStencil)
		depthImageViews[hash] = imageView;
	else
		colourImageViews[hash] = imageView;
	return imageView;
}

bool Texture::IsSame(int w, int h, int d, int arr, int m, crossplatform::PixelFormat f,int numSamples,bool comp,bool rt,bool ds, bool cubemap)
{
	// If we are not created yet...

	if (w != width || h != length || d != depth || m != mips||pixelFormat!=f||numSamples!=mNumSamples||this->cubemap!= cubemap)
	{
		return false;
	}

	return true;
}

#include "Platform/Core/StringFunctions.h"
bool Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform* r, void* t, int w, int l, crossplatform::PixelFormat f, bool rendertarget, bool depthstencil, int numOfSamples)
{
	mExternalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	if (rendertarget)
		mExternalLayout = vk::ImageLayout::eColorAttachmentOptimal;
	if (crossplatform::RenderPlatform::IsDepthFormat(f))
		mExternalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	crossplatform::TextureCreate textureCreate;
	textureCreate.arraysize = 1;
	textureCreate.external_texture = t;
	textureCreate.w = w;
	textureCreate.l = l;
	textureCreate.d = 1;
	textureCreate.arraysize = 1;
	textureCreate.cubemap = false;
	textureCreate.f = f;
	textureCreate.make_rt = rendertarget;
	textureCreate.setDepthStencil = depthstencil;
	textureCreate.numOfSamples = numOfSamples;
	return InitFromExternalTexture(r,&textureCreate);
}

bool Texture::InitFromExternalTexture(crossplatform::RenderPlatform *r, const crossplatform::TextureCreate *textureCreate)
{
	//AssumeLayout(vk::ImageLayout::ePresentSrcKHR);
	if (IsSame(textureCreate->w, textureCreate->l, textureCreate->d, textureCreate->arraysize, textureCreate->mips, textureCreate->f
		, textureCreate->numOfSamples, textureCreate->computable, textureCreate->make_rt, textureCreate->setDepthStencil
		, textureCreate->cubemap))
	{
		if (textureCreate->external_texture == (void*)mImage)
			return true;
	}
	renderPlatform = r;
	depth =1;
	if (textureCreate->w == 0)
		return false;
	if (textureCreate->external_texture == 0)
		return false;
	if (textureCreate->f == crossplatform::UNKNOWN)
		return false;
	InvalidateDeviceObjectsExceptLoaded();
	renderPlatform = r;
	void **image = (void **)&mImage;
	*image = textureCreate->external_texture;
	bool depthstencil = textureCreate->setDepthStencil;
	depthstencil &= (crossplatform::RenderPlatform::IsDepthFormat(textureCreate->f));
	
	int dimen = (textureCreate->d <= 1) ? 2 : 3;

	pixelFormat = textureCreate->f;
	width = textureCreate->w;
	length = textureCreate->l;
	depth = textureCreate->d;
	arraySize = textureCreate->arraysize;
	mips = textureCreate->mips;
	dim = dimen;
	mNumSamples = textureCreate->numOfSamples;
	cubemap = textureCreate->cubemap;
	depthStencil = depthstencil;
	this->computable = computable;
	this->renderTarget = textureCreate->make_rt;
	if (textureCreate->make_rt)
	{
	//	InitFramebuffers(deviceContext);
	}
	external_texture = true;
	InitViewTable(NumFaces(), mips);
	SetVulkanName(renderPlatform, mImage, name.c_str());
	return true;
}

bool Texture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int m,
	crossplatform::PixelFormat f, bool computable /*= false*/, bool rendertarget /*= false*/, bool depthstencil /*= false*/, int num_samples /*= 1*/, int aa_quality /*= 0*/, bool wrap /*= false*/,
	vec4 clear /*= vec4(0.5f, 0.5f, 0.2f, 1.0f)*/, float clearDepth /*= 1.0f*/, uint clearStencil /*= 0*/)
{
	if (IsSame(w, l, 1, 1, m,f, num_samples, computable, rendertarget, depthstencil, false))
	{
		return false;
	}
	if(w*l==0)
		return false;
	InvalidateDeviceObjectsExceptLoaded();
	renderPlatform=r;
	// include eTransferDst IN CASE this is for a texture file loaded.
	vk::ImageUsageFlags usageFlags=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst;
	if(m>1)
		usageFlags|=vk::ImageUsageFlagBits::eTransferSrc;
	if(rendertarget)
		usageFlags|=vk::ImageUsageFlagBits::eColorAttachment;
	if(depthstencil)
		usageFlags|=vk::ImageUsageFlagBits::eDepthStencilAttachment;
	if(computable)
		usageFlags|=vk::ImageUsageFlagBits::eStorage;
	
	vk::ImageCreateFlags imageCreateFlags;
	if(depthstencil)
		imageCreateFlags|=vk::ImageCreateFlagBits::eMutableFormat;
	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);
	vk::FormatProperties props;
	vk::PhysicalDevice *gpu=((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();
	gpu->getFormatProperties(tex_format, &props);
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	
	vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(tex_format)
		.setExtent({ (uint32_t)w, (uint32_t)l, (int32_t)1 })
		.setMipLevels(m)
		.setArrayLayers(1)
		.setSamples((vk::SampleCountFlagBits)num_samples) //AJR
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(usageFlags)
		.setFlags(imageCreateFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::ePreinitialized);
	RETURN_FALSE_IF_FAILED( vulkanDevice->createImage(&imageCreateInfo, nullptr, &mImage));
	SetVulkanName(renderPlatform,mImage,name+" texture mImage");
	vk::MemoryRequirements mem_reqs;
	vulkanDevice->getImageMemoryRequirements(mImage, &mem_reqs);
	
	mem_alloc.setAllocationSize(mem_reqs.size);
	mem_alloc.setMemoryTypeIndex(0);

	if(!((vulkan::RenderPlatform*)renderPlatform)->memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( vulkanDevice->allocateMemory(&mem_alloc, nullptr, &mMem));
	SetVulkanName(renderPlatform,mMem,name+" texture mMem");

	vulkanDevice->bindImageMemory(mImage, mMem, 0);

	InitViewTable(1, m);
	AssumeLayout(vk::ImageLayout::ePreinitialized);

	pixelFormat=f;
	width=w;
	length=l;
	depth=1;
	arraySize=1;
	mips=m;
	dim=2;
	cubemap=false;
	mNumSamples=num_samples;
	depthStencil=depthstencil;
	this->computable=computable;
	this->renderTarget=rendertarget;
	SetVulkanName(renderPlatform,mImage,name.c_str());
	return true;
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int num, int m, crossplatform::PixelFormat f, bool computable , bool rendertarget , bool ascubemap )
{
	if (IsSame(w, l, 1, num, m,f,1,computable,rendertarget,depthStencil,ascubemap))
	{
		return false;
	}
	InvalidateDeviceObjectsExceptLoaded();
	renderPlatform=r;
	int totalNum	= ascubemap ? 6 * num : num;

	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);
	vk::FormatProperties props;
	vk::PhysicalDevice *gpu=((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();
	gpu->getFormatProperties(tex_format, &props);
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	// Include vk::ImageUsageFlagBits::eTransferDst IN CASE we're loading from a file...
	vk::ImageUsageFlags usageFlags=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst;
	if(rendertarget)
		usageFlags|=vk::ImageUsageFlagBits::eColorAttachment;
	if(computable)
		usageFlags|=vk::ImageUsageFlagBits::eStorage;
	if(m>1)
		usageFlags|=vk::ImageUsageFlagBits::eTransferSrc;
	vk::ImageCreateFlags imageCreateFlags;
	if(ascubemap)
		imageCreateFlags|=vk::ImageCreateFlagBits::eCubeCompatible;
	vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(tex_format)
		.setExtent({ (uint32_t)w, (uint32_t)l, (uint32_t)1 })
		.setMipLevels(m)
		.setArrayLayers(totalNum)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(usageFlags)
		.setFlags(imageCreateFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::ePreinitialized);
	RETURN_FALSE_IF_FAILED( vulkanDevice->createImage(&imageCreateInfo, nullptr, &mImage));
	SetVulkanName(renderPlatform,mImage,name+" texture mImage");
	vk::MemoryRequirements mem_reqs;
	vulkanDevice->getImageMemoryRequirements(mImage, &mem_reqs);
	
	mem_alloc.setAllocationSize(mem_reqs.size);
	mem_alloc.setMemoryTypeIndex(0);

	if(!((vulkan::RenderPlatform*)renderPlatform)->memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( vulkanDevice->allocateMemory(&mem_alloc, nullptr, &mMem));
		SetVulkanName(renderPlatform,mMem,name+" texture mMem");

	vulkanDevice->bindImageMemory(mImage, mMem, 0);
	
	InitViewTable(totalNum, m);
	AssumeLayout(vk::ImageLayout::ePreinitialized);

	pixelFormat=f;
	width=w;
	length=l;
	depth=1;
	dim=2;
	arraySize=num;
	mips=m;
    cubemap   = ascubemap;
	depthStencil=false;
	this->computable=computable;
	this->renderTarget=rendertarget;
	if(ascubemap)
		SetName(platform::core::QuickFormat("%s Cubemap %d of %d x %d",name.c_str(),num,w,l));
	else
		SetName(platform::core::QuickFormat("%s TextureArray %d of %d x %d",name.c_str(),num,w,l));
	return true;
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int d, crossplatform::PixelFormat f, bool computable /*= false*/, int m /*= 1*/, bool rendertargets /*= false*/)
{
	if (IsSame(w, l, d, 1, m,f,1,computable,rendertargets,false, false))
	{
		return false;
	}
	InvalidateDeviceObjectsExceptLoaded();
	renderPlatform=r;

	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);
	vk::FormatProperties props;
	vk::PhysicalDevice *gpu=((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();
	gpu->getFormatProperties(tex_format, &props);
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	// eTransferDst in case we want to call setTexels.
	vk::ImageUsageFlags usageFlags=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst| vk::ImageUsageFlagBits::eTransferSrc;
	if(computable)
		usageFlags|=vk::ImageUsageFlagBits::eStorage;

	vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e3D)
		.setFormat(tex_format)
		.setExtent({ (uint32_t)w, (uint32_t)l, (uint32_t)d })
		.setMipLevels(m)
		.setArrayLayers(1)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(usageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::ePreinitialized);
	
	RETURN_FALSE_IF_FAILED( vulkanDevice->createImage(&imageCreateInfo, nullptr, &mImage));
	SetVulkanName(renderPlatform,mImage,name+" texture mImage");
	vk::MemoryRequirements mem_reqs;
	vulkanDevice->getImageMemoryRequirements(mImage, &mem_reqs);
	
	mem_alloc.setAllocationSize(mem_reqs.size);
	mem_alloc.setMemoryTypeIndex(0);

	if(!((vulkan::RenderPlatform*)renderPlatform)->memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( vulkanDevice->allocateMemory(&mem_alloc, nullptr, &mMem));
	SetVulkanName(renderPlatform,mMem,name+" texture mMem");

	vulkanDevice->bindImageMemory(mImage, mMem, 0);
	
	InitViewTable(1, m);
	AssumeLayout(vk::ImageLayout::ePreinitialized);

	pixelFormat=f;
	width=w;
	length=l;
	depth=d;
	arraySize=1;
	mips=m;
	dim=3;
	this->computable=computable;
	this->renderTarget=false;
	this->depthStencil=false;
	this->cubemap=false;
	return true;
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear)
{
	/*activateRenderTarget(deviceContext,0,0);
	renderPlatform->GetDebugEffect()->Apply(deviceContext,"clear_depth",0);
	renderPlatform->DrawQuad(deviceContext);
	renderPlatform->GetDebugEffect()->Unapply(deviceContext);
	deactivateRenderTarget(deviceContext);
	*/
	vk::ImageLayout prev_image_layout=mCurrentImageLayout;
	SetLayout(deviceContext, vk::ImageLayout::eTransferDstOptimal, { 0, 1, 0, 1 });

	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;
	vk::ImageLayout image_layout=mCurrentImageLayout;
	std::vector<vk::ImageSubresourceRange> image_subresource_ranges;
	// if stencil, may need |vk::ImageAspectFlagBits::eStencil
	vk::ImageSubresourceRange imageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);
	image_subresource_ranges.push_back(imageSubresourceRange);

	vk::ClearDepthStencilValue clear_value;
	clear_value.depth=depthClear;
	clear_value.stencil=stencilClear;

	commandBuffer->clearDepthStencilImage(mImage, image_layout, &clear_value, (uint32_t)image_subresource_ranges.size(), image_subresource_ranges.data() );
	SetLayout(deviceContext, prev_image_layout, { 0, 1, 0, 1 });
}

void Texture::GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext)
{
	if (IsValid())
	{
	}
}

void Texture::setTexels(crossplatform::DeviceContext& deviceContext, const void* src, int texel_index, int num_texels)
{
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vulkan::RenderPlatform *r=static_cast<vulkan::RenderPlatform*>(renderPlatform);
	for(auto i:loadedTextures)
	{
		r->PushToReleaseManager(i.buffer);
		r->PushToReleaseManager(i.mem);
	}
	loadedTextures.clear();
	loadedTextures.resize(1);
	SetTextureData(loadedTextures[0],src,width,length,depth,vulkan::RenderPlatform::FormatCount(pixelFormat),pixelFormat);
	int w= loadedTextures[0].x;
	int l= loadedTextures[0].y;
}

int Texture::GetLength() const
{
	return cubemap ? arraySize * 6 : arraySize;
}

int Texture::GetWidth() const
{
	return width;
}

int Texture::GetDimension() const
{
	return dim;
}

int Texture::GetSampleCount() const
{
	return mNumSamples == 1 ? 0 : mNumSamples; //Updated for MSAA texture -AJR
}

bool Texture::IsComputable() const
{
	return computable;
}

bool Texture::HasRenderTargets() const 
{
	return renderTarget;
}

void Texture::copyToMemory(crossplatform::DeviceContext& deviceContext, void* target, int start_texel, int num_texels)
{

}

void Texture::LoadTextureData(LoadedTexture &lt,const char* path)
{
	const auto & pathsUtf8= renderPlatform->GetTexturePathsUtf8();
	int index= platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(path,pathsUtf8);
	std::string filenameInUseUtf8=path;
	if(index==-2||index>=(int)pathsUtf8.size())
	{
		errno=0;
		std::string file;
		std::vector<std::string> split_path = core::SplitPath(path);
		if (split_path.size() > 1)
		{
			file = split_path[1];
			index = core::FileLoader::GetFileLoader()->FindIndexInPathStack(file.c_str(), pathsUtf8);
		}
		if (index < -1 || index >= (int)pathsUtf8.size())
		{
			SIMUL_CERR<<"Failed to find texture file "<<filenameInUseUtf8<<std::endl;
			return;
		}
		filenameInUseUtf8=file;
	}
	if(index<renderPlatform->GetTexturePathsUtf8().size())
		filenameInUseUtf8=(renderPlatform->GetTexturePathsUtf8()[index]+"/")+filenameInUseUtf8;

	int x,y,n;
	void* buffer=nullptr;
	unsigned size=0;
	core::FileLoader::GetFileLoader()->AcquireFileContents(buffer,size,filenameInUseUtf8.c_str(),false);

	if (!buffer)
	{
		SIMUL_CERR << "Failed to load the texture: " << filenameInUseUtf8.c_str() << std::endl;
		return;
	}
	void* data = nullptr;
	TranslateLoadedTextureData(data,buffer,size,x,y,n,4);
	core::FileLoader::GetFileLoader()->ReleaseFileContents(buffer);
	SetTextureData(lt,data,x,y,1,n,crossplatform::PixelFormat::RGBA_8_UNORM);
}

void Texture::SetTextureData(LoadedTexture &lt,const void *data,int x,int y,int z,int n,crossplatform::PixelFormat f)
{
	lt.data=( unsigned char*)data;
	lt.x=x;
	lt.y=y;
	lt.z=z;
	lt.n=n;
	lt.pixelFormat=f;
	int texelBytes=vulkan::RenderPlatform::FormatTexelBytes(f);
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vulkan::RenderPlatform *vkRenderPlatform=(vulkan::RenderPlatform *)renderPlatform;
	auto const buffer_create_info = vk::BufferCreateInfo()
		.setSize(lt.x * lt.y *lt.z * 4 *texelBytes)
		.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr);

	auto result = vulkanDevice->createBuffer(&buffer_create_info, nullptr, &lt.buffer);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	vk::MemoryRequirements mem_reqs;
	vulkanDevice->getBufferMemoryRequirements(lt.buffer, &mem_reqs);

	lt.mem_alloc.setAllocationSize(mem_reqs.size);
	lt.mem_alloc.setMemoryTypeIndex(0);

	vk::MemoryPropertyFlags requirements = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	auto pass = vkRenderPlatform->memory_type_from_properties(mem_reqs.memoryTypeBits, requirements, &lt.mem_alloc.memoryTypeIndex);
	SIMUL_ASSERT(pass == true);

	result = vulkanDevice->allocateMemory(&lt.mem_alloc, nullptr, &(lt.mem));
	SIMUL_ASSERT(result == vk::Result::eSuccess);
	SetVulkanName(renderPlatform,lt.mem,name+" texture lt.mem");

	vulkanDevice->bindBufferMemory(lt.buffer, lt.mem, 0);

	vk::SubresourceLayout layout;
	memset(&layout, 0, sizeof(layout));
	layout.rowPitch = lt.x * texelBytes;
	auto mapped_data = vulkanDevice->mapMemory(lt.mem, 0, lt.mem_alloc.allocationSize);
	SIMUL_ASSERT(mapped_data !=nullptr);
	
	//memcpy(data, lt.data, lt.x * lt.y*4);
	uint8_t *rgba_data	=(uint8_t*)mapped_data;
	uint8_t *cPtr		=(uint8_t*)lt.data;
	for (int i = 0; i < lt.y;i++)
	{
		memcpy(rgba_data, cPtr, texelBytes*lt.x);
		cPtr += texelBytes*lt.x;
		rgba_data += layout.rowPitch;
	}

	vulkanDevice->unmapMemory(lt.mem);
	textureLoadComplete=false;
}

void Texture::CreateFBOs(int sampleCount)
{
	for (int i = 0; i < arraySize; i++)
	{
		for (int mip = 0; mip < mips; mip++)
		{
		}
	}
}

void Texture::SetDefaultSampling(GLuint texId)
{
}

void Texture::StoreExternalState(crossplatform::ResourceState resourceState)
{
	mExternalLayout=vulkan::RenderPlatform::ToVulkanImageLayout(resourceState);
	if(resourceState==crossplatform::ResourceState::UNKNOWN)
		return;
	AssumeLayout(mExternalLayout);
}

void Texture::RestoreExternalTextureState(crossplatform::DeviceContext &deviceContext)
{
	SetLayout(deviceContext, mExternalLayout, {});
}


void Texture::InitViewTable(int l, int m)
{
	// Create state table, at this point, we expect that the state
	// has already being set !!!
	mSubResourcesLayouts.clear();
	mSubResourcesLayouts.resize(l);
	for (int layer = 0; layer < l; layer++)
	{
		mSubResourcesLayouts[layer].resize(m);
		for (int mip = 0; mip < m; mip++)
		{
			mSubResourcesLayouts[layer][mip] = mCurrentImageLayout;
		}
	}
}

bool Texture::AreSubresourcesInSameState(const crossplatform::SubresourceRange& subresourceRange) const
{
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == -1 ? NumFaces() : subresourceRange.arrayLayerCount;
	const uint32_t& numMips = subresourceRange.mipLevelCount == -1 ? mips : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& startMip = subresourceRange.baseMipLevel;

	std::vector<vk::ImageLayout> stateCheck;
	for (uint32_t layer = startLayer; layer < startLayer + numLayers; layer++)
		for (uint32_t mip = startMip; mip < startMip + numMips; mip++)
			stateCheck.push_back(mSubResourcesLayouts[layer][mip]);

	return std::equal(stateCheck.begin() + 1, stateCheck.end(), stateCheck.begin());
}

void Texture::SetLayout(crossplatform::DeviceContext& deviceContext, vk::ImageLayout newLayout, const crossplatform::SubresourceRange& subresourceRange)
{
	if (newLayout == vk::ImageLayout::eUndefined)
		return;

	int totalNum = cubemap ? 6 * arraySize : arraySize;
	const bool split_layouts = !AreSubresourcesInSameState({});

	const uint32_t& startMip = subresourceRange.baseMipLevel;
	const uint32_t& numMips = subresourceRange.mipLevelCount == -1 ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == -1 ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;

	bool allSubresources = ((startMip == 0) && (startLayer == 0)) && ((numMips == mips) && (numLayers == totalNum));

	auto* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	auto DstAccessMask = [](vk::ImageLayout const& layout)
	{
		vk::AccessFlags flags;

		switch (layout)
		{
		case vk::ImageLayout::eTransferDstOptimal:
			// Make sure anything that was copying from this image has completed
			flags = vk::AccessFlagBits::eTransferWrite;
			break;
		case vk::ImageLayout::eColorAttachmentOptimal:
			flags = vk::AccessFlagBits::eColorAttachmentWrite;
			break;
		case vk::ImageLayout::eDepthStencilAttachmentOptimal:
			flags = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			break;
		case vk::ImageLayout::eShaderReadOnlyOptimal:
			// Make sure any Copy or CPU writes to image are flushed
			flags = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
			break;
		case vk::ImageLayout::eTransferSrcOptimal:
			flags = vk::AccessFlagBits::eTransferRead;
			break;
		case vk::ImageLayout::ePresentSrcKHR:
			flags = vk::AccessFlagBits::eMemoryRead;
			break;
		default:
			break;
		}

		return flags;
	};
	vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
	if (crossplatform::RenderPlatform::IsDepthFormat(pixelFormat))
		aspectMask = vk::ImageAspectFlagBits::eDepth;
	if (crossplatform::RenderPlatform::IsStencilFormat(pixelFormat))
		aspectMask |= vk::ImageAspectFlagBits::eStencil;
	vk::AccessFlags srcAccessMask = vk::AccessFlagBits();
	vk::AccessFlags dstAccessMask = DstAccessMask(newLayout);
	vk::PipelineStageFlags src_stages = vk::PipelineStageFlagBits::eBottomOfPipe;
	vk::PipelineStageFlags dest_stages = vk::PipelineStageFlagBits::eAllCommands;	// very general..

	auto barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(mImage);

	// Set the whole resource state
	if (allSubresources)
	{
		if (split_layouts)
		{
			// This is the case going from split to unsplit layout. God.
			for (int l = 0; l < totalNum; l++)
			{
				for (int m = 0; m < mips; m++)
				{
					vk::ImageLayout& imageLayout = mSubResourcesLayouts[l][m];
					if (imageLayout == newLayout)
						continue;
					barrier.setOldLayout(imageLayout);
					barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, m, 1, l, 1));
					commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
					imageLayout = newLayout;
				}
			}
			AssumeLayout(newLayout);
		}
		else
		{
			if (mCurrentImageLayout == newLayout)
				return;
			barrier.setOldLayout(mCurrentImageLayout);
			barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, mips, 0, totalNum));
			commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
			AssumeLayout(newLayout);
		}
		mCurrentImageLayout = newLayout;
	}
	// Set a subresource range states
	else 
	{
		for (uint32_t l = startLayer; l < startLayer + numLayers; l++)
		{
			for (uint32_t m = startMip; m < startMip + numMips; m++)
			{
				vk::ImageLayout& imageLayout = mSubResourcesLayouts[l][m];
				barrier.setOldLayout(imageLayout);
				barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, m, 1, l, 1));
				commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
				imageLayout = newLayout;
			}
		}
	}

	if (AreSubresourcesInSameState({}))
		mCurrentImageLayout = newLayout;
}

void Texture::AssumeLayout(vk::ImageLayout layout)
{
	for (auto& i : mSubResourcesLayouts)
	{
		for (auto& j : i)
		{
			j = layout;
		}
	}
	mCurrentImageLayout = layout;
}

vk::ImageLayout Texture::GetLayout(crossplatform::DeviceContext& deviceContext, const crossplatform::SubresourceRange& subresourceRange)
{
	if (mSubResourcesLayouts.empty())
		return mCurrentImageLayout;

	if (AreSubresourcesInSameState({}))
		return mCurrentImageLayout;

	if (AreSubresourcesInSameState(subresourceRange))
		return mSubResourcesLayouts[subresourceRange.baseArrayLayer][subresourceRange.baseMipLevel];

	const uint32_t& startMip = subresourceRange.baseMipLevel;
	const uint32_t& numMips = subresourceRange.mipLevelCount == -1 ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == -1 ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;

	// Return the resource state of a mip or array layer, or the whole resource
	if (numMips > 1 || numLayers > 1)
	{
		// If we request the state of the ranges of subresource, we have to make sure
		// that all of the subresources are in the correct state. The correct state
		// will be the main resource state.
		for (uint32_t l = startLayer; l < startLayer + numLayers; l++)
		{
			for (uint32_t m = startMip; m < startMip + numMips; m++)
			{
				auto curState = mSubResourcesLayouts[l][m];
				if (curState != mCurrentImageLayout)
				{
					vk::CommandBuffer* commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
					vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
					if (crossplatform::RenderPlatform::IsDepthFormat(pixelFormat))
						aspectMask = vk::ImageAspectFlagBits::eDepth;
					if (crossplatform::RenderPlatform::IsStencilFormat(pixelFormat))
						aspectMask |= vk::ImageAspectFlagBits::eStencil;
					vk::AccessFlags srcAccessMask = vk::AccessFlagBits();
					// vk::AccessFlags dstAccessMask = DstAccessMask(newLayout);
					vk::PipelineStageFlags src_stages = vk::PipelineStageFlagBits::eBottomOfPipe;
					vk::PipelineStageFlags dest_stages = vk::PipelineStageFlagBits::eAllCommands;	// very general..
					SetImageLayout(commandBuffer, mImage, aspectMask, curState, mCurrentImageLayout, srcAccessMask, src_stages, dest_stages, { m, (uint32_t)1, l, (uint32_t)1 });
					mSubResourcesLayouts[l][m] = mCurrentImageLayout;
				}
			}
		}
		return mCurrentImageLayout;
	}

	// Return a subresource state
	return mSubResourcesLayouts[startLayer][startMip];
}

void Texture::SetImageLayout(vk::CommandBuffer* commandBuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
	vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages, const crossplatform::SubresourceRange& subresourceRange)
{
	assert(commandBuffer);
	auto DstAccessMask = [](vk::ImageLayout const& layout)
	{
		vk::AccessFlags flags;

		switch (layout)
		{
		case vk::ImageLayout::eTransferDstOptimal:
			// Make sure anything that was copying from this image has completed
			flags = vk::AccessFlagBits::eTransferWrite;
			break;
		case vk::ImageLayout::eColorAttachmentOptimal:
			flags = vk::AccessFlagBits::eColorAttachmentWrite;
			break;
		case vk::ImageLayout::eDepthStencilAttachmentOptimal:
			flags = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			break;
		case vk::ImageLayout::eShaderReadOnlyOptimal:
			// Make sure any Copy or CPU writes to image are flushed
			flags = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
			break;
		case vk::ImageLayout::eTransferSrcOptimal:
			flags = vk::AccessFlagBits::eTransferRead;
			break;
		case vk::ImageLayout::ePresentSrcKHR:
			flags = vk::AccessFlagBits::eMemoryRead;
			break;
		default:
			break;
		}

		return flags;
	};
	
	const uint32_t& startMip = subresourceRange.baseMipLevel;
	const uint32_t& numMips = subresourceRange.mipLevelCount == -1 ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == -1 ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;
	
	auto const barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(DstAccessMask(newLayout))
		.setOldLayout(oldLayout)
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, startMip, numMips, startLayer, numLayers));

	commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits(), 0, nullptr, 0, nullptr, 1, &barrier);
}