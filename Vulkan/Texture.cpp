#include "Platform/Core/FileLoader.h"
#include "Texture.h"
#include "RenderPlatform.h"
#include "DeviceManager.h"
#include "Effect.h"
#include <algorithm>

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
		ensureTexture2DSizeAndFormat(r,w,l, m,crossplatform::PixelFormat::RGBA_8_UNORM,false,false,false,1,0,false,vec4(0.f,0.f,0.f,0.f),1.F,0,false);
	else
		ensureTextureArraySizeAndFormat(r,(int)w,(int)l,(int)num,(int)m,crossplatform::PixelFormat::RGBA_8_UNORM,false,false,false,false);
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
		for(auto i:mLayerViews)
		{
			r->PushToReleaseManager(i);
		}
		for(auto i:mFramebuffers)
		{
			for(auto j:i)
			{
				r->PushToReleaseManager(j);
			}
		}
		mFramebuffers.clear();
		for(auto i:mMainMipViews)
		{
			r->PushToReleaseManager(i);
		}
		for(auto i:faceArrayMipViews)
		{
			r->PushToReleaseManager(i);
		}
		for(auto i:mCubemapLayerMipViews)
		{
			for(auto j:i)
			{
				r->PushToReleaseManager(j);
			}
		}
		mCubemapLayerMipViews.clear();
		for(auto i:mLayerMipViews)
		{
			for(auto j:i)
			{
				r->PushToReleaseManager(j);
			}
		}
		r->PushToReleaseManager(mFaceArrayView);
		r->PushToReleaseManager(mCubeArrayView);
		r->PushToReleaseManager(mMainView);
		r->PushToReleaseManager(mRenderPass);
		mRenderPass = nullptr;
		mLayerViews.clear();
		mMainMipViews.clear();
		faceArrayMipViews.clear();
		mLayerMipViews.clear();
		if(!external_texture)
		{
			r->PushToReleaseManager(mImage);
			r->PushToReleaseManager(mMem);
		}
		r->PushToReleaseManager(mBuffer);
	}
}

void Texture::SetImageLayout(vk::CommandBuffer *commandBuffer,vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
	vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages,int startm,int num_mips)
{
	assert(commandBuffer);
	auto DstAccessMask = [](vk::ImageLayout const &layout)
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
	if(num_mips<=0)
		num_mips=mips-startm;
	int totalNum = cubemap ? 6 * arraySize : arraySize;
	auto const barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(DstAccessMask(newLayout))
		.setOldLayout(oldLayout)
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED) 
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, startm, num_mips, 0,totalNum));

	commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits(), 0, nullptr, 0, nullptr, 1, &barrier);
}

void Texture::FinishLoading(crossplatform::DeviceContext &deviceContext)
{
	if(textureLoadComplete)
		return;
	SIMUL_ASSERT(loadedTextures.size()!=0)
	
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;

	if (GetSampleCount() > 1)
	{
		AssumeLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePreinitialized,
			currentImageLayout, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader);
	}
	else
	{
		SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, currentImageLayout,
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
					vk::PipelineStageFlagBits::eTransfer, i, 1);
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
				currentImageLayout, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader);
		}
		else
		{
			AssumeLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			SetImageLayout(commandBuffer, mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal,
				currentImageLayout, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader);
		}
	}
	for (unsigned int i = 0; i < loadedTextures.size(); i++)
	{
		FreeTranslatedTextureData(loadedTextures[i].data);
	}
	
	textureLoadComplete=true;
}

void Texture::SplitLayouts()
{
	split_layouts=true;
}

void Texture::AssumeLayout(vk::ImageLayout layout)
{
//	int totalNum = cubemap ? 6 * arraySize : arraySize;
	for(auto &i:mLayerMipLayouts)
	{
		for(auto &j:i)
		{
			j=layout;
		}
	}
	currentImageLayout=layout;
	split_layouts=false;
#ifdef _DEBUG
//	SIMUL_COUT<<"Assume layout for "<<name.c_str()<<": "<<to_string( layout )<<std::endl;
#endif
 }
 
vk::ImageView *Texture::AsVulkanDepthView( int layer, int mip)
{
	if(!textureLoadComplete)
	{
		return nullptr;
	}
	if (mip == mips)
	{
		mip = 0;
	}
	int realArray	= GetArraySize();
	bool no_array	= !cubemap && (arraySize <= 1);

	// Base view:
	if ((mips <= 1 && no_array) || (layer < 0 && mip < 0))
	{
		return &layerDepthViews[0];
	}
	if(mip>0)
	{
		SIMUL_BREAK("Not implemented.");
	}
	return &layerDepthViews[layer];
}

vk::ImageView *Texture::AsVulkanImageView(crossplatform::ShaderResourceType type, int layer, int mip, bool rw)
{
	if(!textureLoadComplete)
	{
		return nullptr;
	}
	if (mip == mips)
	{
		mip = 0;
	}

	int realArray	= GetArraySize();
	bool no_array	= !cubemap && (arraySize <= 1);
	bool isUAV		= (crossplatform::ShaderResourceType::RW & type) == crossplatform::ShaderResourceType::RW;
	
	bool cubemap_as_array=cubemap && ((type & crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY) == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY);

	// Base view:
	if ((mips <= 1 && no_array) || (layer < 0 && mip < 0))
	{
		if (cubemap_as_array)
		{
			return &mFaceArrayView;
		}
		if(type==crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY&&arraySize==1)
			return &mCubeArrayView;
		return &mMainView;
	}
	// Layer view:
	if (mip < 0 || mips <= 1)
	{
		if (layer < 0 || no_array)
		{
			if(cubemap_as_array)
				return &mFaceArrayView;
			return &mMainView;
		}
		return &mLayerViews[layer];
	}
	// Mip view:
	if (layer < 0)
	{
		if (cubemap_as_array)
		{
			return &faceArrayMipViews[mip];
		}
		return &mMainMipViews[mip];
	}
	if((type&crossplatform::ShaderResourceType::TEXTURE_CUBE)==crossplatform::ShaderResourceType::TEXTURE_CUBE)
	{
		return &mCubemapLayerMipViews[layer][mip];
	}
	// Layer mip view:
	return &mLayerMipViews[layer][mip];
}

vk::Framebuffer *Texture::GetVulkanFramebuffer(int layer , int mip)
{
	if(layer<0&&mip<0)
	{
		AssumeLayout(vk::ImageLayout::ePresentSrcKHR);
	}
	else
		split_layouts=true;
	if(layer<0)
		layer=0;
	if(mip<0)
		mip=0;
	return &(mFramebuffers[layer][mip]);
}

bool Texture::IsSame(int w, int h, int d, int arr, int m, crossplatform::PixelFormat f,int numSamples,bool comp,bool rt,bool ds,bool need_srv
	, bool cubemap)
{
	// If we are not created yet...

	if (w != width || h != length || d != depth || m != mips||pixelFormat!=f||numSamples!=mNumSamples||this->cubemap!= cubemap)
	{
		return false;
	}

	return true;
}

#include "Platform/Core/StringFunctions.h"
bool Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform* r, void* t, void* srv, int w, int l, crossplatform::PixelFormat f, bool rendertarget, bool depthstencil, bool need_srv, int numOfSamples)
{
	mExternalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	if (rendertarget)
		mExternalLayout = vk::ImageLayout::eColorAttachmentOptimal;
	if (crossplatform::RenderPlatform::IsDepthFormat(f))
		mExternalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	crossplatform::TextureCreate textureCreate;
	textureCreate.arraysize = 1;
	textureCreate.external_texture = t;
	textureCreate.srv = srv;
	textureCreate.w = w;
	textureCreate.l = l;
	textureCreate.d = 1;
	textureCreate.arraysize = 1;
	textureCreate.cubemap = false;
	textureCreate.f = f;
	textureCreate.make_rt = rendertarget;
	textureCreate.setDepthStencil = depthstencil;
	textureCreate.need_srv = need_srv;
	textureCreate.numOfSamples = numOfSamples;
	return InitFromExternalTexture(r,&textureCreate);
}

bool Texture::InitFromExternalTexture(crossplatform::RenderPlatform *r, const crossplatform::TextureCreate *textureCreate)
{
	//AssumeLayout(vk::ImageLayout::ePresentSrcKHR);
	if (!textureCreate->forceInit&&IsSame(textureCreate->w, textureCreate->l, textureCreate->d, textureCreate->arraysize, textureCreate->mips, textureCreate->f
		, textureCreate->numOfSamples, textureCreate->computable, textureCreate->make_rt, textureCreate->setDepthStencil
		, textureCreate->need_srv
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
	InitViewTables(dimen, textureCreate->f, textureCreate->w, textureCreate->l, textureCreate->mips, textureCreate->arraysize, textureCreate->make_rt
		,textureCreate->cubemap
		, depthstencil);
	SplitLayouts();

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
	SetVulkanName(renderPlatform, mImage, name.c_str());
	return true;
}

bool Texture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int m, crossplatform::PixelFormat f,
	bool computable, bool rendertarget, bool depthstencil,
	int num_samples, int aa_quality, bool wrap, vec4 clear, float clearDepth, uint clearStencil,
	bool shared, crossplatform::CompressionFormat compressionFormat, const uint8_t** initData)
{
	if (IsSame(w, l, 1, 1, m,f, num_samples, computable, rendertarget, depthstencil, true))
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

	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);

	vk::Device* vulkanDevice = renderPlatform->AsVulkanDevice();
	vk::PhysicalDevice* gpu = ((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();

	vk::FormatProperties props = gpu->getFormatProperties(tex_format);
	vk::ImageFormatProperties image_props = gpu->getImageFormatProperties(tex_format, vk::ImageType::e2D, vk::ImageTiling::eOptimal, usageFlags, imageCreateFlags);
	
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

	if(!((vulkan::RenderPlatform*)renderPlatform)->MemoryTypeFromProperties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( vulkanDevice->allocateMemory(&mem_alloc, nullptr, &mMem));
	SetVulkanName(renderPlatform,mMem,name+" texture mMem");

	vulkanDevice->bindImageMemory(mImage, mMem, 0);

	InitViewTables(2,f,w,l,m, 1, rendertarget,false,depthstencil);
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

void Texture::InitViewTables(int dim,crossplatform::PixelFormat f,int w,int h,int mipCount, int layers, bool isRenderTarget,bool cubemap,bool isDepthTarget, bool isArray)
{
	if(!renderPlatform)
		return;
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	vk::ImageViewType viewType=vk::ImageViewType::e2D;
	if(dim==3)
		viewType=vk::ImageViewType::e3D;
	else if(cubemap)
	{
		if(layers>1)
			viewType=vk::ImageViewType::eCubeArray;
		else
			viewType=vk::ImageViewType::eCube;

	}
	else if(layers>1 || isArray)
		viewType=vk::ImageViewType::e2DArray;
	int totalNum = cubemap ? 6 * layers : layers;
	vk::ImageAspectFlags imageAspectFlags=vk::ImageAspectFlagBits::eColor;
	crossplatform::PixelFormat colourFormat=f;
	crossplatform::PixelFormat depthFormat=f;
	vk::Format depth_format;
	if(crossplatform::RenderPlatform::IsDepthFormat(f))
	{
		imageAspectFlags=vk::ImageAspectFlagBits::eDepth;
		SIMUL_ASSERT(isDepthTarget);
		depth_format = vulkan::RenderPlatform::ToVulkanFormat(depthFormat);
	}
	vk::Format pixel_read_format = vulkan::RenderPlatform::ToVulkanFormat(colourFormat);
	
	vk::ImageViewCreateInfo viewCreateInfo = vk::ImageViewCreateInfo()
		.setImage(mImage)
		.setViewType(viewType)
		.setFormat(pixel_read_format)
		.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags, 0, mipCount, 0, totalNum));
	if(pixel_read_format==vk::Format::eUndefined)
		viewCreateInfo=viewCreateInfo.setPNext(vulkanRenderPlatform->GetVideoSamplerYcbcrConversionInfo());
	vk::Result res=vulkanDevice->createImageView(&viewCreateInfo, nullptr, &mMainView);
	SIMUL_VK_CHECK(res);
	SetVulkanName(renderPlatform,mMainView,(name+" imageView").c_str());
	
	// the mips of the main view.
	if(mipCount>1)
	{
		mMainMipViews.resize(mipCount);
		for(int i=0;i<mipCount;i++)
		{
			viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags,i,1,0,totalNum));
			SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &mMainMipViews[i]));
			SetVulkanName(renderPlatform,mMainMipViews[i],(name+" imageView").c_str());
		}
	}
	// cube array as 2d texture array.
	if(cubemap)
	{
		viewType=vk::ImageViewType::e2DArray;
		viewCreateInfo	.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags, 0, mipCount, 0, totalNum))
						.setViewType(viewType);
		SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &mFaceArrayView));
		SetVulkanName(renderPlatform,mFaceArrayView,(name+" mFaceArrayView").c_str());

		// View cubemap as a Cubemap array.
		viewCreateInfo	.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags, 0, mipCount, 0, totalNum))
						.setViewType(vk::ImageViewType::eCubeArray);
		SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &mCubeArrayView));
		SetVulkanName(renderPlatform,mCubeArrayView,(name+" mCubeArrayView").c_str());
		
		// View each mip as a a 2D array of faces.
		if(mipCount>1)
		{
			faceArrayMipViews.resize(mipCount);
			for(int i=0;i<mipCount;i++)
			{
				viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags,i,1,0,totalNum))
					.setViewType(vk::ImageViewType::e2DArray);
				SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &faceArrayMipViews[i]));
				SetVulkanName(renderPlatform,faceArrayMipViews[i],(name+" faceArray imageView").c_str());
			}
		}
	}
	// layer views: individual layers of an array.
	if(dim==2&&totalNum>1)
	{
		int c=1;
		viewType=vk::ImageViewType::e2D;
		viewCreateInfo.setViewType(viewType);
		mLayerViews.resize(totalNum);
		for(int i=0;i< totalNum;i++)
		{
			viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags,0,mipCount,i,1));
			SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &mLayerViews[i]));
			SetVulkanName(renderPlatform,mLayerViews[i],(name+" mFaceArrayView").c_str());
		}
	}
	if(isDepthTarget)
	{
		viewType=vk::ImageViewType::e2DArray;
		viewCreateInfo.setViewType(viewType);
		viewCreateInfo.setFormat(depth_format);
		viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags,0,1,0,totalNum));
		SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &mainDepthView));
		SetVulkanName(renderPlatform, mainDepthView,(name+" mainDepthView").c_str());

		layerDepthViews.resize(totalNum);
		viewType=vk::ImageViewType::e2D;
		viewCreateInfo.setViewType(viewType);
		for(int i=0;i< totalNum;i++)
		{
			viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags,0,1,i,1));
			SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &layerDepthViews[i]));
			SetVulkanName(renderPlatform,layerDepthViews[i],(name+" layerDepthView").c_str());
		}
		viewCreateInfo.setFormat(pixel_read_format);
	}
	if(cubemap)
	{
		viewCreateInfo.setViewType(vk::ImageViewType::eCube);
		mCubemapLayerMipViews.resize(layers);
		for (int i = 0; i < layers; i++)
		{
			mCubemapLayerMipViews[i].resize(mipCount);
			for (int j = 0; j < mipCount; j++)
			{
				viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags,j,1,6*i,6));
				SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &mCubemapLayerMipViews[i][j]));
				SetVulkanName(renderPlatform,mCubemapLayerMipViews[i][j],(name+" mCubemapLayerMipView").c_str());
			}
		}
	}
	viewCreateInfo.setViewType(dim==2?vk::ImageViewType::e2D:vk::ImageViewType::e3D);
	mLayerMipViews.resize(totalNum);
	for (int i = 0; i < totalNum; i++)
	{
		mLayerMipViews[i].resize(mipCount);
		for (int j = 0; j < mipCount; j++)
		{
			viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(imageAspectFlags,j,1,i,1));
			SIMUL_VK_CHECK(vulkanDevice->createImageView(&viewCreateInfo, nullptr, &mLayerMipViews[i][j]));
			SetVulkanName(renderPlatform,mLayerMipViews[i][j],(name+" mFaceArrayView").c_str());
		}
	}
	if (isRenderTarget)
	{
		mFramebuffers.clear();
	}
	mLayerMipLayouts.resize(totalNum);
	for(int i=0;i<totalNum;i++)
		mLayerMipLayouts[i].resize(mipCount);
}

vk::RenderPass &Texture::GetRenderPass(crossplatform::DeviceContext &deviceContext)
{
	if (!mRenderPass)
	{
		auto *r = (vulkan::RenderPlatform*)renderPlatform;
		if(currentImageLayout==vk::ImageLayout::eUndefined||currentImageLayout==vk::ImageLayout::ePreinitialized)
		{
			SetLayout(deviceContext,vk::ImageLayout::eColorAttachmentOptimal);
		}
		vk::ImageLayout layouts[]={currentImageLayout};
		vk::ImageLayout end_layouts[]={(currentImageLayout==vk::ImageLayout::eUndefined||currentImageLayout==vk::ImageLayout::ePreinitialized)?vk::ImageLayout::eColorAttachmentOptimal:currentImageLayout};
		r->CreateVulkanRenderpass(deviceContext,mRenderPass, 1, &pixelFormat, crossplatform::PixelFormat::UNKNOWN,false,false,false,GetSampleCount(),layouts,end_layouts);
		AssumeLayout(end_layouts[0]);
	}
	return mRenderPass;
}

void Texture::InitFramebuffers(crossplatform::DeviceContext &deviceContext)
{
	if(mFramebuffers.size())
		return;
	vulkan::EffectPass *effectPass=(vulkan::EffectPass*)deviceContext.contextState.currentEffectPass;
	vk::RenderPass& vkRenderPass = effectPass->multiview ? effectPass->GetVulkanRenderPass(*deviceContext.AsGraphicsDeviceContext()) : GetRenderPass(deviceContext);
	
	vk::ImageView attachments[1]={nullptr};

	vk::FramebufferCreateInfo framebufferCreateInfo = vk::FramebufferCreateInfo();
	framebufferCreateInfo.renderPass = vkRenderPass;
	framebufferCreateInfo.attachmentCount = 1;
	framebufferCreateInfo.width = width;
	framebufferCreateInfo.height = length;
	framebufferCreateInfo.layers = 1;
	
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	int totalNum	= cubemap ? 6 * arraySize : arraySize;
	mFramebuffers.resize(totalNum);
	for (int i = 0; i < totalNum; i++)
	{
		mFramebuffers[i].resize(mips);
		framebufferCreateInfo.width = width;
		framebufferCreateInfo.height = length;
		for (int j= 0; j < mips; j++)
		{
			attachments[0]=mLayerMipViews[i][j];
			framebufferCreateInfo.pAttachments = attachments;
			SIMUL_VK_CHECK(vulkanDevice->createFramebuffer(&framebufferCreateInfo, nullptr, &mFramebuffers[i][j]));
			SetVulkanName(renderPlatform,mFramebuffers[i][j],platform::core::QuickFormat("%s FB, layer %d, mip %d", name.c_str(),i,j));
	
			framebufferCreateInfo.width=(framebufferCreateInfo.width+1)/2;
			framebufferCreateInfo.height = (framebufferCreateInfo.height+1)/2;
		}
	}
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int num, int mips, crossplatform::PixelFormat f,
	bool computable, bool rendertarget, bool depthstencil, bool cubemap, crossplatform::CompressionFormat compressionFormat, const uint8_t** initData)
{
	if (IsSame(w, l, 1, num, mips, f, 1, computable, rendertarget, depthstencil, true, cubemap))
	{
		return false;
	}
	InvalidateDeviceObjectsExceptLoaded();
	renderPlatform=r;
	int totalNum	= cubemap ? 6 * num : num;

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
	if(cubemap)
		imageCreateFlags|=vk::ImageCreateFlagBits::eCubeCompatible;
	if (depthstencil)
		imageCreateFlags |= vk::ImageCreateFlagBits::eMutableFormat;
	
	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);

	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
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
	RETURN_FALSE_IF_FAILED( vulkanDevice->createImage(&imageCreateInfo, nullptr, &mImage));
	SetVulkanName(renderPlatform,mImage,name+" texture mImage");
	vk::MemoryRequirements mem_reqs;
	vulkanDevice->getImageMemoryRequirements(mImage, &mem_reqs);
	
	mem_alloc.setAllocationSize(mem_reqs.size);
	mem_alloc.setMemoryTypeIndex(0);

	if(!((vulkan::RenderPlatform*)renderPlatform)->MemoryTypeFromProperties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( vulkanDevice->allocateMemory(&mem_alloc, nullptr, &mMem));
		SetVulkanName(renderPlatform,mMem,name+" texture mMem");

	vulkanDevice->bindImageMemory(mImage, mMem, 0);
	
	InitViewTables(2, f, w, l, mips, num, rendertarget, cubemap, depthstencil, true);
	AssumeLayout(vk::ImageLayout::ePreinitialized);

	pixelFormat=f;
	width=w;
	length=l;
	depth=1;
	dim=2;
	arraySize=num;
	this->mips=mips;
	this->cubemap=cubemap;
	this->depthStencil=depthstencil;
	this->computable=computable;
	this->renderTarget=rendertarget;
	if(cubemap)
		SetName(platform::core::QuickFormat("%s Cubemap %d of %d x %d",name.c_str(),num,w,l));
	else
		SetName(platform::core::QuickFormat("%s TextureArray %d of %d x %d",name.c_str(),num,w,l));
	return true;
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int d, crossplatform::PixelFormat f, bool computable /*= false*/, int m /*= 1*/, bool rendertargets /*= false*/)
{
	if (IsSame(w, l, d, 1, m,f,1,computable,rendertargets,false, true))
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
	
	RETURN_FALSE_IF_FAILED( vulkanDevice->createImage(&imageCreateInfo, nullptr, &mImage));
	SetVulkanName(renderPlatform,mImage,name+" texture mImage");
	vk::MemoryRequirements mem_reqs;
	vulkanDevice->getImageMemoryRequirements(mImage, &mem_reqs);
	
	mem_alloc.setAllocationSize(mem_reqs.size);
	mem_alloc.setMemoryTypeIndex(0);

	if(!((vulkan::RenderPlatform*)renderPlatform)->MemoryTypeFromProperties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( vulkanDevice->allocateMemory(&mem_alloc, nullptr, &mMem));
	SetVulkanName(renderPlatform,mMem,name+" texture mMem");

	vulkanDevice->bindImageMemory(mImage, mMem, 0);
	
	InitViewTables(3,f,w,l,m, 1, false,false,false);
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
	return true;
}

bool Texture::ensureVideoTexture(crossplatform::RenderPlatform* renderPlatform, int w, int l, crossplatform::PixelFormat f, crossplatform::VideoTextureType texType)
{
	return false;
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear)
{
	vk::ImageLayout prev_image_layout=currentImageLayout;
	SetLayout(deviceContext,vk::ImageLayout::eTransferDstOptimal);

	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;
	vk::ImageLayout image_layout=currentImageLayout;
	std::vector<vk::ImageSubresourceRange> image_subresource_ranges;
	// if stencil, may need |vk::ImageAspectFlagBits::eStencil
	vk::ImageSubresourceRange imageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);
	image_subresource_ranges.push_back(imageSubresourceRange);

	vk::ClearDepthStencilValue clear_value;
	clear_value.depth=depthClear;
	clear_value.stencil=stencilClear;

	commandBuffer->clearDepthStencilImage(mImage, image_layout, &clear_value, (uint32_t)image_subresource_ranges.size(), image_subresource_ranges.data() );
	// can't go back to these layouts so just stay in eTransferDstOptimal for now:
	if(prev_image_layout!=vk::ImageLayout::ePreinitialized&&prev_image_layout!=vk::ImageLayout::eUndefined)
		SetLayout(deviceContext, prev_image_layout);
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
	auto pass = vkRenderPlatform->MemoryTypeFromProperties(mem_reqs.memoryTypeBits, requirements, &lt.mem_alloc.memoryTypeIndex);
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

vk::ImageLayout Texture::GetLayout(int layer, int mip) const
{
	if(layer>=0&&mip>=0)
		return mLayerMipLayouts[layer][mip];
	return currentImageLayout;
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
	SetLayout(deviceContext, mExternalLayout);
}

void Texture::SetLayout(crossplatform::DeviceContext &deviceContext, vk::ImageLayout newLayout, int layer, int mip)
{
	if(newLayout==vk::ImageLayout::eUndefined)
		return;
	if (mip == mips)
	{
		mip = 0;
	}
	//void SetImageLayout(vk::CommandBuffer *commandBuffer,vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
	//	vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages)
	auto *commandBuffer = (vk::CommandBuffer*)deviceContext.platform_context;
	auto DstAccessMask = [](vk::ImageLayout const &layout)
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
	vk::ImageAspectFlags aspectMask=vk::ImageAspectFlagBits::eColor;
	if(crossplatform::RenderPlatform::IsDepthFormat(pixelFormat))
		aspectMask=vk::ImageAspectFlagBits::eDepth;
	if(crossplatform::RenderPlatform::IsStencilFormat(pixelFormat))
		aspectMask|=vk::ImageAspectFlagBits::eStencil;
	vk::AccessFlags srcAccessMask = vk::AccessFlagBits();
	vk::AccessFlags dstAccessMask = DstAccessMask(newLayout);
	vk::PipelineStageFlags src_stages = vk::PipelineStageFlagBits::eBottomOfPipe;
	vk::PipelineStageFlags dest_stages = vk::PipelineStageFlagBits::eAllCommands;	// very general..

	auto  barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(mImage);
	int totalNum = cubemap ? 6 * arraySize : arraySize;
	if ((layer >= 0 || mip >= 0) && (arraySize >1||mips>1))
	{
		int num_mips = 1;
		int num_layers = 1;
		if(layer<0)
		{
			layer=0;
			num_layers = totalNum;
		}
		if (mip < 0)
		{
			mip = 0;
			num_mips = mips;
		}
		for (int l = layer; l < layer + num_layers; l++)
		{
			for (int m = mip; m < mip + num_mips; m++)
			{
				vk::ImageLayout& imageLayout = mLayerMipLayouts[l][m];
				barrier.setOldLayout(imageLayout);
				barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, m, 1, l, 1));
				commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
				imageLayout = newLayout;
			}
		}
		split_layouts = true;
	}
	else if(split_layouts)
	{
	// This is the case going from split to unsplit layout. God.
		for (int l = 0; l <  totalNum; l++)
		{
			for (int m = 0; m < mips; m++)
			{
				vk::ImageLayout& imageLayout = mLayerMipLayouts[l][m];
				if (imageLayout == newLayout)
					continue;
				barrier.setOldLayout(imageLayout);
				barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, m, 1, l, 1));
				commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
				imageLayout = newLayout;
			}
		}
		AssumeLayout(newLayout);
		split_layouts = false;
	}
	else
	{
		if (currentImageLayout == newLayout)
			return;
		int totalNum = cubemap ? 6 * arraySize : arraySize;
		barrier.setOldLayout(currentImageLayout);
		barrier.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, mips, 0, totalNum));
		commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits::eDeviceGroup, 0, nullptr, 0, nullptr, 1, &barrier);
		AssumeLayout(newLayout);
		split_layouts = false;
	}
#ifdef _DEBUG
//	SIMUL_COUT<<"Set layout for "<<name.c_str()<<": "<<to_string( newLayout )<<std::endl;
#endif
}