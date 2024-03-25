#include "Texture.h"
#include "RenderPlatform.h"
#include "DeviceManager.h"
#include "Effect.h"

#include "Platform/Core/FileLoader.h"
#include "Platform/Core/StringFunctions.h"
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
//#include <vulkan/vk_enum_string_helper.h>


using namespace platform;
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
	samplerStateDesc=*desc;
	renderPlatform=r;
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
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
			.setAnisotropyEnable((desc->filtering==crossplatform::SamplerStateDesc::POINT)?VK_FALSE:VK_TRUE)
			.setMaxAnisotropy(16)
			.setCompareEnable(VK_FALSE)
			.setCompareOp(vk::CompareOp::eNever)
			.setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
			.setUnnormalizedCoordinates(VK_FALSE);
	SIMUL_VK_CHECK(vulkanDevice->createSampler(&samplerCreateInfo,nullptr,&mSampler));
	SetVulkanName(renderPlatform,mSampler,"Sampler");
}

  vk::Sampler *SamplerState::AsVulkanSampler() 
  {
	  return &mSampler;
  }

void SamplerState::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
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

bool Texture::LoadFromFile(crossplatform::RenderPlatform *r, const char *pFilePathUtf8, bool gen_mips)
{
	InvalidateDeviceObjects();
	std::vector<std::string> texture_files;
	texture_files.push_back(pFilePathUtf8);
	return LoadTextureArray(r,texture_files,gen_mips?0:1);
}

bool Texture::LoadTextureArray(crossplatform::RenderPlatform *r, const std::vector<std::string> &texture_files, bool gen_mips)
{
	InvalidateDeviceObjects();
	renderPlatform= r;
	PushLoadedTexturesToReleaseManager();
	ResizeLoadedTextures(1, texture_files.size());
	for (unsigned int i = 0; i < texture_files.size(); i++)
	{
		LoadTextureData(mLoadedTextures[0][i],texture_files[i].c_str());
	}
	int w= mLoadedTextures[0][0].x;
	int l= mLoadedTextures[0][0].y;
	if(w*l==0)
	{
		// don't try to complete the load with empty data.
		textureUploadComplete=true;
		return false;
	}
	size_t num = mLoadedTextures[0].size();
	int m=1;
	if(gen_mips)
		m=100;
	m = std::min(m, int(floor(log2(std::max(w, l)))) +1);
	m = std::min(16, std::max(1, m));
	name = "Texture array ";
	for(auto m:texture_files)
	{
		name += m + ",";
	}
	if(num<=1)
		ensureTexture2DSizeAndFormat(r,w,l, m,crossplatform::PixelFormat::RGBA_8_UNORM,nullptr,false,false,false,1,0,false,vec4(0.f,0.f,0.f,0.f),1.F,0,false);
	else
		ensureTextureArraySizeAndFormat(r,w,l,(int)num,m,crossplatform::PixelFormat::RGBA_8_UNORM,nullptr,false,false,false,false);
	textureUploadComplete=false;
	return true;
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
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	if(vulkanDevice)
	{
		InvalidateDeviceObjectsExceptLoaded();
		PushLoadedTexturesToReleaseManager();
		ClearLoadedTextures();
	}
	crossplatform::Texture::InvalidateDeviceObjects();
}

void Texture::InvalidateDeviceObjectsExceptLoaded()
{
	if(!renderPlatform)
		return;
	vulkan::RenderPlatform *r= static_cast<vulkan::RenderPlatform * >(renderPlatform);

	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	if(vulkanDevice)
	{
		if(!external_texture)
		{
			r->PushToReleaseManager(mImage, &mAllocationInfo);
		}
		// don't free defaultImageView, it's a duplicate.
		for (auto imageView : mImageViews)
		{
			r->PushToReleaseManager(*imageView.second);
			delete imageView.second;
		}
		mImageViews.clear();
	}
}

void Texture::FinishLoading(crossplatform::DeviceContext &deviceContext)
{
	if(textureUploadComplete)
		return;
	SIMUL_ASSERT(mLoadedTextures.size() != 0)
	SIMUL_ASSERT(mLoadedTextures[0].size() != 0)

	vulkanRenderPlatform->EndRenderPass(deviceContext);
	
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;

	if (GetSampleCount() > 1)
	{
		AssumeLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePreinitialized,
			mCurrentImageLayout, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader);
	}
	else
	{
		if(compressionFormat!=crossplatform::CompressionFormat::UNCOMPRESSED)
		{
			SIMUL_CERR<<"Texture "<<name.c_str()<<" uploading with compression format "<<magic_enum::enum_name<crossplatform::CompressionFormat>(compressionFormat)<<"\n";
		}
		SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, mCurrentImageLayout,
			vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer);
		
		uint32_t totalMips = uint32_t(mips);
		uint32_t totalLayers = uint32_t(arraySize * (cubemap ? 6 : 1));
		uint32_t totalSubresources = totalLayers * totalMips;
		for (uint32_t mip = 0; mip < totalMips; mip++)
		{
			for (uint32_t layer = 0; layer < totalLayers; layer++)
			{
				if (mip >= mLoadedTextures.size())
					continue;
				if (layer >= mLoadedTextures[mip].size())
					continue;

				LoadedTexture &lt = mLoadedTextures[mip][layer];
				auto const subresource = vk::ImageSubresourceLayers()
					.setAspectMask(vk::ImageAspectFlagBits::eColor)
					.setMipLevel(mip)
					.setBaseArrayLayer(layer)
					.setLayerCount(1);

				int row_texels = lt.x;
				int rows = lt.y;
				if(compressionFormat!=crossplatform::CompressionFormat::UNCOMPRESSED)
				{
					// must be at least a block size;
					if(row_texels<4)
						row_texels=4;
					if(rows<4)
						rows=4;
				}

				vk::Offset3D offset = { 0, 0, 0 };
				vk::Extent3D extent = vk::Extent3D()
					.setWidth(lt.x)
					.setHeight(lt.y)
					.setDepth(1);

				auto const copy_region =
					vk::BufferImageCopy()
					.setBufferOffset(0)
					.setBufferRowLength(row_texels)
					.setBufferImageHeight(rows)
					.setImageSubresource(subresource)
					.setImageOffset(offset)
					.setImageExtent(extent);

				commandBuffer->copyBufferToImage(lt.buffer, mImage, vk::ImageLayout::eTransferDstOptimal, 1, &copy_region);
				if(compressionFormat!=platform::crossplatform::CompressionFormat::UNCOMPRESSED)
				{
					//SIMUL_CERR<<"Texture "<<name.c_str()<<", mip "<<i<<" Surface "<<j<<": Uploaded "<<lt.mem_alloc.allocationSize<<" bytes.\n";
				}
			}
		}
		
		if (totalMips > 1 && totalMips > mLoadedTextures.size())
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
			blit.dstSubresource.layerCount = arraySize; //Does all layers at the same time!
			for (int mip = 0; mip < mips; mip++)
			{
				blit.srcSubresource.mipLevel = mip;
				blit.dstSubresource.mipLevel = mip + 1;
				int dstWidth = srcWidth > 1 ? srcWidth / 2 : 1;
				int dstLength = srcLength > 1 ? srcLength / 2 : 1;
				blit.srcOffsets[1] = vk::Offset3D(srcWidth, srcLength, 1);
				blit.dstOffsets[1] = vk::Offset3D(dstWidth, dstLength, 1);
				SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
							   vk::PipelineStageFlagBits::eTransfer, {crossplatform::TextureAspectFlags::COLOUR, uint8_t(mip), 1, 0, uint8_t(-1)});
				if (mip < mips - 1)
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
	for (auto j : mLoadedTextures)
	{
		for(auto i:j)
		{
			FreeTranslatedTextureData(i.data);
		}
	}
	
	textureUploadComplete = true;
}
vk::ImageView *Texture::AsVulkanImageView(crossplatform::TextureView textureView)
{
#if PLATFORM_INTERNAL_CHECKS
	if (!ValidateTextureView(textureView))
		return nullptr;
#endif
	const uint8_t &startMip = textureView.elements.subresourceRange.baseMipLevel;
	const uint8_t &startLayer = textureView.elements.subresourceRange.baseArrayLayer;
	// special case for the most common situation:
	if (textureView.elements.subresourceRange.mipLevelCount == 0xff)
		textureView.elements.subresourceRange.mipLevelCount = mips - startMip;
	if (textureView.elements.subresourceRange.arrayLayerCount == 0xff)
		textureView.elements.subresourceRange.arrayLayerCount = NumFaces() - startLayer; // Not arraySize;

	// Special case for the most common situation:
	if (textureView.hash == 0 || textureView.hash == defaultTextureView.hash)
		return mDefaultImageView;

	uint64_t hash = textureView.hash;
	auto view = mImageViews.find(textureView.hash);
	if (view != mImageViews.end())
		return view->second;

	if (textureView.hash == 0)
	{
		textureView = crossplatform::DefaultTextureView;
		hash = 0; // Ok, 0x0000 0100FF00FF 00 is now aliased with 0.
	}

	vk::ImageView *imageView = CreateVulkanImageView(textureView);
	mImageViews[hash] = imageView;
	return imageView;
}
vk::ImageView *Texture::CreateVulkanImageView(crossplatform::TextureView textureView)
{
	// TODO: Should we override aspect if the texture is a depth stencil? - AJR
	vk::ImageAspectFlagBits aspect = depthStencil ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits(textureView.elements.subresourceRange.aspectMask);
	const uint8_t &startMip = textureView.elements.subresourceRange.baseMipLevel;
	const uint8_t &numMips = textureView.elements.subresourceRange.mipLevelCount == uint8_t(0xFF) ? mips - startMip : textureView.elements.subresourceRange.mipLevelCount;
	const uint8_t &startLayer = textureView.elements.subresourceRange.baseArrayLayer;
	const uint8_t &numLayers = textureView.elements.subresourceRange.arrayLayerCount == uint8_t(0xFF) ? NumFaces() - startLayer : textureView.elements.subresourceRange.arrayLayerCount;

	crossplatform::ShaderResourceType type = textureView.elements.type;
	if(type==crossplatform::ShaderResourceType::UNKNOWN)
	{
		type = GetDefaultShaderResourceType();
	}

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
	imageViewCI.format = vulkan::RenderPlatform::ToVulkanFormat(pixelFormat, compressionFormat);
	imageViewCI.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
	imageViewCI.subresourceRange = {aspect, startMip, numMips, startLayer, numLayers};

	vk::ImageView *imageView = new vk::ImageView();
	SIMUL_VK_CHECK(((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice()->createImageView(&imageViewCI, nullptr, imageView));
	SetVulkanName(renderPlatform, *imageView, (name + " imageView").c_str());
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
	if (!textureCreate->forceInit&&IsSame(textureCreate->w, textureCreate->l, textureCreate->d, textureCreate->arraysize, textureCreate->mips, textureCreate->f
		, textureCreate->numOfSamples, textureCreate->computable, textureCreate->make_rt, textureCreate->setDepthStencil, textureCreate->cubemap))
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
	this->depthStencil = depthstencil;
	this->computable = textureCreate->computable;
	this->renderTarget = textureCreate->make_rt;
	
	external_texture = true;
	InitViewTable(NumFaces(), mips);
	SetVulkanName(renderPlatform, mImage, name.c_str());
	return true;
}

bool Texture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int m, crossplatform::PixelFormat f
	, std::shared_ptr<std::vector<std::vector<uint8_t>>> data,
	bool computable, bool rendertarget, bool depthstencil,
	int num_samples, int aa_quality, bool wrap, vec4 clear, float clearDepth, uint clearStencil,
	bool shared, crossplatform::CompressionFormat cf)
{
	if (IsSame(w, l, 1, 1, m,f, num_samples, computable, rendertarget, depthstencil, false))
	{
		return false;
	}
	if(w*l==0)
		return false;
	compressionFormat=cf;
	InvalidateDeviceObjectsExceptLoaded();
	renderPlatform=r;
	//Include eTransferDst IN CASE this is for a texture file loaded.
	//Include eTransferSrc IN CASE this is for a texture readback.
	vk::ImageUsageFlags usageFlags=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst|vk::ImageUsageFlagBits::eTransferSrc;
	if(m>1)
		usageFlags|=vk::ImageUsageFlagBits::eTransferSrc;
	if(rendertarget)
		usageFlags|=vk::ImageUsageFlagBits::eColorAttachment;
	if(computable)
		usageFlags|=vk::ImageUsageFlagBits::eStorage;
	if(depthstencil)
#if defined(__ANDROID__)
		usageFlags=vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst; //Overwrite!
#else
		usageFlags|=vk::ImageUsageFlagBits::eDepthStencilAttachment;
#endif
	
	vk::ImageCreateFlags imageCreateFlags;
	if(depthstencil)
		imageCreateFlags|=vk::ImageCreateFlagBits::eMutableFormat;

	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f, compressionFormat);
	
	//SIMUL_CERR<<"Texture "<<name.c_str()<<", format "<<magic_enum::enum_name(f)<<", compr: "<<magic_enum::enum_name(cf)<<" Vk format: "<<magic_enum::enum_name(tex_format)<<" ("<<(int)tex_format<<").\n";

	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	vk::PhysicalDevice* gpu = ((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();

	vk::FormatProperties props = gpu->getFormatProperties(tex_format);
	vk::ImageTiling tiling=vk::ImageTiling::eOptimal;
	vk::ImageFormatProperties image_props = gpu->getImageFormatProperties(tex_format, vk::ImageType::e2D, tiling, usageFlags, imageCreateFlags);
	vk::ImageLayout vkLayout = vk::ImageLayout::eUndefined;
	vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(tex_format)
		.setExtent({ (uint32_t)w, (uint32_t)l, (int32_t)1 })
		.setMipLevels(m)
		.setArrayLayers(1)
		.setSamples((vk::SampleCountFlagBits)num_samples) //AJR
											  .setTiling(tiling)
		.setUsage(usageFlags)
		.setFlags(imageCreateFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr);

	std::string _name = name + " texture mImage";
	VkExternalMemoryImageCreateInfo extImageCreateInfo = {};
	if(shared)
	{
		/* Indicate that the memory backing this image will be exported in an
		 * fd. In some implementations, this may affect the call to
		 * GetImageMemoryRequirements() with this image.*/
		extImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
		extImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

		imageCreateInfo.pNext = &extImageCreateInfo;

		//VkExternalImageFormatProperties::externalMemoryProperties.compatibleHandleTypes;
	}
	else
	{
		vkLayout=vk::ImageLayout::ePreinitialized;
	}
	imageCreateInfo.setInitialLayout(vkLayout);
	vulkanRenderPlatform->CreateVulkanImage(imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, mImage, mAllocationInfo, _name.c_str());

	InitViewTable(1, m);
	AssumeLayout(vkLayout);

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
	
	if(data)
	{
		int mip_width=width;
		int mip_length=length;
		ClearLoadedTextures();
		ResizeLoadedTextures(mips, 1);
		for (uint32_t i = 0; i < uint32_t(mips); i++)
		{
			uint32_t n = CalculateSubresourceIndex(i, 0, 0, mips, 1);
			LoadedTexture& loadedTexture=mLoadedTextures[i][0];
			SetTextureData(loadedTexture, (*data)[i].data(), mip_width, mip_length, 1, 0, pixelFormat, compressionFormat);
			mip_width=(mip_width+1)/2;
			mip_length=(mip_length+1)/2;
		}
		textureUploadComplete=false;
	}
	mDefaultImageView = mImageViews[crossplatform::DefaultTextureView.hash] = CreateVulkanImageView(crossplatform::DefaultTextureView);
	SetDefaultTextureView();
	return true;
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int num, int mips, crossplatform::PixelFormat f
	, std::shared_ptr<std::vector<std::vector<uint8_t>>> data,bool computable, bool rendertarget, bool depthstencil, bool cb, crossplatform::CompressionFormat cf)
{
	if (IsSame(w, l, 1, num, mips, f, 1, computable, rendertarget, depthstencil, cb))
	{
		return false;
	}
	InvalidateDeviceObjectsExceptLoaded();
	renderPlatform=r;
	int totalNum	= cb ? 6 * num : num;

	// Include vk::ImageUsageFlagBits::eTransferDst IN CASE we're loading from a file...
	vk::ImageUsageFlags usageFlags=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst;
	if(rendertarget)
		usageFlags|=vk::ImageUsageFlagBits::eColorAttachment;
	if(computable)
		usageFlags|=vk::ImageUsageFlagBits::eStorage;
	if(mips>1)
		usageFlags|=vk::ImageUsageFlagBits::eTransferSrc;
	if(depthstencil)
#if defined(__ANDROID__)
		usageFlags=vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst; //Overwrite!
#else
		usageFlags|=vk::ImageUsageFlagBits::eDepthStencilAttachment;
#endif

	vk::ImageCreateFlags imageCreateFlags;
	if(cb)
		imageCreateFlags|=vk::ImageCreateFlagBits::eCubeCompatible;
	if (depthstencil)
		imageCreateFlags |= vk::ImageCreateFlagBits::eMutableFormat;
	
	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f, cf);
	if(cf!=crossplatform::CompressionFormat::UNCOMPRESSED)
		SIMUL_CERR<<"Texture "<<name.c_str()<<", format "<<magic_enum::enum_name(f)<<", compr: "<<magic_enum::enum_name(cf)<<" Vk format: "<<magic_enum::enum_name(tex_format)<<" ("<<(int)tex_format<<").\n";

	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	vk::PhysicalDevice *gpu=((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();
	
	vk::FormatProperties props = gpu->getFormatProperties(tex_format);
	vk::ImageFormatProperties image_props = gpu->getImageFormatProperties(tex_format, vk::ImageType::e2D, vk::ImageTiling::eOptimal, usageFlags, imageCreateFlags);
	
	vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(tex_format)
		.setExtent({ (uint32_t)w, (uint32_t)l, (uint32_t)1 })
		.setMipLevels(mips)
		.setArrayLayers(totalNum)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(usageFlags)
		.setFlags(imageCreateFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::ePreinitialized);

	std::string _name = name + " texture mImage";
	vulkanRenderPlatform->CreateVulkanImage(imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, mImage, mAllocationInfo, _name.c_str());

	pixelFormat=f;
	width=w;
	length=l;
	depth=1;
	dim=2;
	arraySize=num;
	this->mips=mips;
	this->cubemap=cb;
	this->depthStencil=depthstencil;
	this->computable=computable;
	this->renderTarget=rendertarget;
	compressionFormat=cf;
	if(cubemap)
		SetName(platform::core::QuickFormat("%s Cubemap %d of %d x %d",name.c_str(),num,w,l));
	else
		SetName(platform::core::QuickFormat("%s TextureArray %d of %d x %d",name.c_str(),num,w,l));
	
	InitViewTable(totalNum, mips);
	AssumeLayout(vk::ImageLayout::ePreinitialized);
	if(data)
	{
		ResizeLoadedTextures(mips, totalNum);
		for (uint32_t i = 0; i < uint32_t(totalNum); i++)
		{
			int mip_width=width;
			int mip_length=length;
			for (uint32_t j = 0; j < uint32_t(mips); j++)
			{
				uint32_t n = CalculateSubresourceIndex(j, i, 0, mips, totalNum);
				LoadedTexture& loadedTexture = mLoadedTextures[j][i];
				SetTextureData(loadedTexture, (*data)[n].data(), mip_width, mip_length, 1, 0, pixelFormat, compressionFormat);
				mip_width=(mip_width+1)/2;
				mip_length=(mip_length+1)/2;
			}
		}
		textureUploadComplete=false;
	}
	mDefaultImageView = mImageViews[crossplatform::DefaultTextureView.hash] = CreateVulkanImageView(crossplatform::DefaultTextureView);
	SetDefaultTextureView();
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
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	// eTransferDst in case we want to call setTexels.
	vk::ImageUsageFlags usageFlags=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst ;
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

	std::string _name = name + " texture mImage";
	vulkanRenderPlatform->CreateVulkanImage(imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, mImage, mAllocationInfo, _name.c_str());
	
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
	this->depthStencil = false;
	this->cubemap = false;
	mDefaultImageView = mImageViews[crossplatform::DefaultTextureView.hash] = CreateVulkanImageView(crossplatform::DefaultTextureView);
	SetDefaultTextureView();
	return true;
}

bool Texture::ensureVideoTexture(crossplatform::RenderPlatform* renderPlatform, int w, int l, crossplatform::PixelFormat f, crossplatform::VideoTextureType texType)
{
	return false;
}

void Texture::ClearColour(crossplatform::GraphicsDeviceContext &deviceContext, vec4 colourClear)
{
	const int &layerCount = NumFaces();
	crossplatform::SubresourceRange subresource = {crossplatform::TextureAspectFlags::COLOUR, (uint8_t)0, (uint8_t)mips, (uint8_t)0, (uint8_t)layerCount};

	vk::ImageLayout prevImageLayout = mCurrentImageLayout;
	SetLayout(deviceContext, vk::ImageLayout::eTransferDstOptimal, subresource);

	vk::ImageSubresourceRange imageSubresourceRange(vk::ImageAspectFlagBits::eColor, (uint32_t)0, (uint32_t)mips, (uint32_t)0, (uint32_t)layerCount);

	vk::ClearColorValue clearValue;
	clearValue.float32[0] = colourClear[0];
	clearValue.float32[1] = colourClear[1];
	clearValue.float32[2] = colourClear[2];
	clearValue.float32[3] = colourClear[3];

	vulkanRenderPlatform->EndRenderPass(deviceContext);
	vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
	commandBuffer->clearColorImage(mImage, mCurrentImageLayout, &clearValue, 1, &imageSubresourceRange);

	// We can't go back to these layouts below, so just stay in eTransferDstOptimal for now:
	if (prevImageLayout != vk::ImageLayout::ePreinitialized && prevImageLayout != vk::ImageLayout::eUndefined)
		SetLayout(deviceContext, prevImageLayout, subresource);
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear)
{
	const int& layerCount = NumFaces();
	crossplatform::SubresourceRange subresource = {crossplatform::TextureAspectFlags::DEPTH, (uint8_t)0, (uint8_t)mips, 0, (uint8_t)layerCount};

	vk::ImageLayout prevImageLayout = mCurrentImageLayout;
	SetLayout(deviceContext, vk::ImageLayout::eTransferDstOptimal, subresource);

	vk::ImageSubresourceRange imageSubresourceRange(vk::ImageAspectFlagBits::eDepth, (uint32_t)0, (uint32_t)1, (uint32_t)0, (uint32_t)layerCount);

	vk::ClearDepthStencilValue clearValue;
	clearValue.depth = depthClear;
	clearValue.stencil = stencilClear;

	vulkanRenderPlatform->EndRenderPass(deviceContext);
	vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
	commandBuffer->clearDepthStencilImage(mImage, mCurrentImageLayout, &clearValue, 1, &imageSubresourceRange);

	// We can't go back to these layouts below, so just stay in eTransferDstOptimal for now:
	if (prevImageLayout != vk::ImageLayout::ePreinitialized && prevImageLayout != vk::ImageLayout::eUndefined)
		SetLayout(deviceContext, prevImageLayout, subresource);
}

void Texture::GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext)
{
	if (IsValid())
	{
	}
}

void Texture::setTexels(crossplatform::DeviceContext& deviceContext, const void* src, int texel_index, int num_texels)
{
	setTexels(src,texel_index,num_texels);
}

void Texture::setTexels(const void *src,int texel_index,int num_texels)
{
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	vulkan::RenderPlatform *r=static_cast<vulkan::RenderPlatform*>(renderPlatform);
	PushLoadedTexturesToReleaseManager();
	ClearLoadedTextures();
	ResizeLoadedTextures(1, 1);
	SetTextureData(mLoadedTextures[0][0], src, width, length, depth, vulkan::RenderPlatform::FormatCount(pixelFormat), pixelFormat);
	int w= mLoadedTextures[0][0].x;
	int l= mLoadedTextures[0][0].y;
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

void Texture::SetTextureData(LoadedTexture &lt,const void *data,int x,int y,int z,int n,crossplatform::PixelFormat f,crossplatform::CompressionFormat cf)
{
	lt.data=( unsigned char*)data;
	lt.x=x;
	lt.y=y;
	lt.z=z;
	lt.n=n;
	lt.pixelFormat=f;
	static int uu = 4;
	size_t bytesPerTexel	=crossplatform::GetByteSize(f);
	size_t SysMemPitch		=x*bytesPerTexel;
	size_t bufferSize		 = SysMemPitch*y;
	size_t numRowsToCopy	 = y;
	switch (cf)
	{
	case crossplatform::CompressionFormat::BC1:
	case crossplatform::CompressionFormat::BC3:
	case crossplatform::CompressionFormat::BC5:
	case crossplatform::CompressionFormat::ETC1:
	case crossplatform::CompressionFormat::ETC2:
		{
		// The memory pitch of a line for uncompressed formats, becomes the memory pitch
		// of a row of blocks in the case of the compressed.
			size_t block_width		=std::max(1,(x+3)/4);
			size_t block_height		=std::max(1,(y+3)/4);
			size_t block_size_bytes=(bytesPerTexel==4)?16:8;
			SysMemPitch				= block_size_bytes*block_width;
			// buffer must be at least one block in size.
			bufferSize				= std::max(block_size_bytes,SysMemPitch*block_height);
			numRowsToCopy			=block_height;
		}
		break;
	default:
		break;
	};
	//int texelBytes=vulkan::RenderPlatform::FormatTexelBytes(f);
	vk::Device *vulkanDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	vulkan::RenderPlatform *vkRenderPlatform=(vulkan::RenderPlatform *)renderPlatform;
	std::string _name = name + " texture upload buffer";

	vkRenderPlatform->CreateVulkanBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		lt.buffer, lt.allocationInfo, _name.c_str());

	vk::SubresourceLayout layout;
	memset(&layout, 0, sizeof(layout));
	layout.rowPitch = SysMemPitch;//lt.x * texelBytes;
	void *mapped_data = nullptr;
	vmaMapMemory(lt.allocationInfo.allocator, lt.allocationInfo.allocation, &mapped_data);
	SIMUL_ASSERT(mapped_data !=nullptr);
	
	//memcpy(data, lt.data, lt.x * lt.y*4);
	uint8_t *target_data	=(uint8_t*)mapped_data;
	uint8_t *cPtr			=(uint8_t*)lt.data;
	size_t totalBytes=0;
	for (int i = 0; i < numRowsToCopy;i++)
	{
		memcpy(target_data, cPtr,SysMemPitch);
		cPtr		+= SysMemPitch;
		target_data	+= layout.rowPitch;
		totalBytes	+= SysMemPitch;
	}
	
	if(cf!=platform::crossplatform::CompressionFormat::UNCOMPRESSED)
	{
		//SIMUL_CERR<<"Texture "<<name.c_str()<<", surface "<<" mip "<<": Uploaded "<<totalBytes<<" bytes.\n";
	}
	vmaUnmapMemory(lt.allocationInfo.allocator, lt.allocationInfo.allocation);
	textureUploadComplete=false;
}

void Texture::StoreExternalState(crossplatform::ResourceState resourceState)
{
	mExternalLayout = vulkan::RenderPlatform::ToVulkanImageLayout(resourceState);
	if(resourceState==crossplatform::ResourceState::UNKNOWN)
		return;
	AssumeLayout(mExternalLayout);
}

void Texture::RestoreExternalTextureState(crossplatform::DeviceContext &deviceContext)
{
	SetLayout(deviceContext, mExternalLayout, crossplatform::DefaultSubresourceRange);
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
	mSplitLayouts = false; //Eqv. to UpdateSplitLayoutsFlag();
}

bool Texture::AreSubresourcesInSameState(crossplatform::SubresourceRange subresourceRange) const
{
	const uint32_t &startMip = subresourceRange.baseMipLevel;
	const uint32_t &numMips = subresourceRange.mipLevelCount == uint8_t(0xFF) ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t &startLayer = subresourceRange.baseArrayLayer;
	const uint32_t &numLayers = subresourceRange.arrayLayerCount == uint8_t(0xFF) ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;

	vk::ImageLayout checkLayout = mSubResourcesLayouts[startLayer][startMip];
	for (uint32_t layer = startLayer; layer < startLayer + numLayers; layer++)
	{
		for (uint32_t mip = startMip; mip < startMip + numMips; mip++)
		{
			if(mSubResourcesLayouts[layer][mip]!=checkLayout)
				return false;
		}
	}
	return true;
}

void Texture::SetLayout(crossplatform::DeviceContext &deviceContext, vk::ImageLayout newLayout, const crossplatform::SubresourceRange& subresourceRange)
{
	if (newLayout == vk::ImageLayout::eUndefined)
		return;

	int totalNum = cubemap ? 6 * arraySize : arraySize;

	const uint32_t& startMip = subresourceRange.baseMipLevel;
	const uint32_t& numMips = (subresourceRange.mipLevelCount == uint8_t(0xFF)) ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = (subresourceRange.arrayLayerCount == uint8_t(0xFF)) ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;

	bool allSubresources = ((startMip == 0) && (startLayer == 0)) && ((numMips == mips) && (numLayers == totalNum));

	if (!mSplitLayouts && mCurrentImageLayout == newLayout)
		return;
	if (AreSubresourcesInSameState(subresourceRange))
	{
		if (mSubResourcesLayouts[startLayer][startMip] == newLayout)
			return;
	}

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
			flags = vk::AccessFlagBits::eDepthStencilAttachmentWrite|vk::AccessFlagBits::eDepthStencilAttachmentRead;
			break;
		case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
			flags = vk::AccessFlagBits::eDepthStencilAttachmentRead;
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
		case vk::ImageLayout::eGeneral:
			break;
		default:
			SIMUL_BREAK_ONCE("Unknown layout.");
			break;
		}

		return flags;
	};
	vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
	if (crossplatform::RenderPlatform::IsDepthFormat(pixelFormat))
		aspectMask = vk::ImageAspectFlagBits::eDepth;
	if (crossplatform::RenderPlatform::IsStencilFormat(pixelFormat))
		aspectMask |= vk::ImageAspectFlagBits::eStencil;
	//std::cout << "Texture::SetLayout " << name << " from layout: " << string_VkImageLayout((VkImageLayout)mCurrentImageLayout) << " to layout: " << string_VkImageLayout((VkImageLayout)newLayout) << 
	//	" - AspectMask is " << string_VkImageAspectFlags((VkImageAspectFlags)aspectMask) << "\n";
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
		if (mSplitLayouts)
		{
			// This is the case going from split to unsplit layout.
			for (int l = 0; l < totalNum; l++)
			{
				for (int m = 0; m < mips; m++)
				{
					vk::ImageLayout& imageLayout = mSubResourcesLayouts[l][m];
					if (imageLayout == newLayout)
						continue;
					barrier.setOldLayout(imageLayout);
					barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, m, 1, l, 1));
					
					vulkanRenderPlatform->EndRenderPass(deviceContext);
					
					commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
					imageLayout = newLayout;
				}
			}
		}
		else
		{
			barrier.setOldLayout(mCurrentImageLayout);
			barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, mips, 0, totalNum));
			
			vulkanRenderPlatform->EndRenderPass(deviceContext);
			
			commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		AssumeLayout(newLayout); //Will set mSplitLayouts to false.
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
				if(imageLayout==newLayout)
					continue;
				barrier.setOldLayout(imageLayout);
				barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, m, 1, l, 1));

				vulkanRenderPlatform->EndRenderPass(deviceContext);

				commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
				imageLayout = newLayout;
			}
		}
		UpdateSplitLayoutsFlag();
	}
	
	if (!mSplitLayouts)
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
	mSplitLayouts = false; //Eqv. to UpdateSplitLayoutsFlag();
}

vk::ImageLayout Texture::GetLayout(crossplatform::DeviceContext& deviceContext, const crossplatform::SubresourceRange& subresourceRange)
{
	if (mSubResourcesLayouts.empty())
		return mCurrentImageLayout;

	if (!mSplitLayouts)
		return mCurrentImageLayout;

	if (AreSubresourcesInSameState(subresourceRange))
		return mSubResourcesLayouts[subresourceRange.baseArrayLayer][subresourceRange.baseMipLevel];

	const uint32_t& startMip = subresourceRange.baseMipLevel;
	const uint32_t& numMips = subresourceRange.mipLevelCount == uint8_t(0xFF) ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == uint8_t(0xFF) ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;

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
					SetImageLayout(commandBuffer, mImage, aspectMask, curState, mCurrentImageLayout, srcAccessMask, src_stages, dest_stages, {crossplatform::TextureAspectFlags::COLOUR, (uint8_t)m, (uint8_t)1, (uint8_t)l, (uint8_t)1});
					mSubResourcesLayouts[l][m] = mCurrentImageLayout;
				}
			}
		}
		UpdateSplitLayoutsFlag();
		return mCurrentImageLayout;
	}

	// Return a subresource state
	return mSubResourcesLayouts[startLayer][startMip];
}

void Texture::SetImageLayout(vk::CommandBuffer* commandBuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
	vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages, crossplatform::SubresourceRange subresourceRange)
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
	const uint32_t& numMips = subresourceRange.mipLevelCount == uint8_t(0xFF) ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == uint8_t(0xFF) ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;

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

void Texture::ClearLoadedTextures()
{ 
	for (auto& i : mLoadedTextures)
	{ 
		i.clear(); 
	} 
	mLoadedTextures.clear();
}

void Texture::ResizeLoadedTextures(size_t mips, size_t layers) 
{ 
	mLoadedTextures.resize(mips);
	for (auto &i : mLoadedTextures)
	{
		i.resize(layers);
	}
}

void Texture::PushLoadedTexturesToReleaseManager()
{
	if (!renderPlatform)
		return;
	vulkan::RenderPlatform* r = static_cast<vulkan::RenderPlatform*>(renderPlatform);

	for (auto j : mLoadedTextures)
	{
		for (auto i : j)
		{
			r->PushToReleaseManager(i.buffer, &(i.allocationInfo));
		}
	}
}

void Texture::UpdateSplitLayoutsFlag()
{
	mSplitLayouts = !AreSubresourcesInSameState();
}