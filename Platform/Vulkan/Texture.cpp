#define NOMINMAX

#include "Texture.h"
#include "RenderPlatform.h"
#include "DeviceManager.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>

using namespace simul;
using namespace vulkan;

void DeleteTextures(size_t num,GLuint *t)
{
	if(!t||!num)
		return;
	for(int i=0;i<num;i++)
	{
//		if(t[i]!=0&&!glIsTexture(t[i]))
		{
	//		SIMUL_BREAK_ONCE("Not a texture");
		}
	}
	//glDeleteTextures(num,t);
}

// TODO: This is ridiculous. But GL, at least in the current NVidia implementation, seems unable to write to a re-used texture id that it has generated after that id 
// was previously freed. Bad bug.
// Therefore we force the issue by making the tex id's go up sequentially, not standard GL behaviour:
void glGenTextures_DONT_REUSE(int count,GLuint *tex)
{
	//glGenTextures(count,tex);
}

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
	vk::Device *device=r->AsVulkanDevice();
	vk::SamplerCreateInfo samplerCreateInfo=vk::SamplerCreateInfo();
	device->createSampler(&samplerCreateInfo,nullptr,&mSampler);
}

  vk::Sampler *SamplerState::AsVulkanSampler() 
  {
	  return &mSampler;
  }

void SamplerState::InvalidateDeviceObjects()
{
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

void Texture::LoadFromFile(crossplatform::RenderPlatform* r, const char* pFilePathUtf8)
{
	InvalidateDeviceObjects();
	std::vector<std::string> texture_files;
	texture_files.push_back(pFilePathUtf8);
	LoadTextureArray(r,texture_files,1);
}

void Texture::LoadTextureArray(crossplatform::RenderPlatform* r, const std::vector<std::string>& texture_files,int m)
{
	InvalidateDeviceObjects();
	renderPlatform=r;
	loadedTextures.resize(texture_files.size());
	for (unsigned int i = 0; i < texture_files.size(); i++)
	{
		//std::string mainPath	= r->GetTexturePathsUtf8()[0] + "/" + texture_files[i];
		LoadTextureData(loadedTextures[i],texture_files[i].c_str());
	}
	int w= loadedTextures[0].x;
	int l= loadedTextures[0].y;
	int num= loadedTextures.size();
	 m= std::min(m,1 + int(floor(log2(width >= length ? width : length))));
	if(num<=1)
		ensureTexture2DSizeAndFormat(r,w,l,crossplatform::PixelFormat::RGBA_32_FLOAT,false,false,false);
	else
		ensureTextureArraySizeAndFormat(r,w,l,num,m,crossplatform::PixelFormat::RGBA_32_FLOAT,false,false,false);
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
	vk::Device *device=renderPlatform->AsVulkanDevice();
	for(auto i:mLayerViews)
	{
		device->destroyImageView(i);
	}
	for(auto i:mMainMipViews)
	{
		device->destroyImageView(i);
	}
	for(auto i:mLayerMipViews)
	{
		for(auto j:i)
		{
			device->destroyImageView(j);
		}
	}
	for(auto i:loadedTextures)
	{
		device->destroyBuffer(i.buffer);
		device->freeMemory(i.mem, nullptr);
	}
	loadedTextures.clear();
	device->destroyImageView(mCubeArrayView);
	device->destroyImageView(mMainView);
	mLayerViews.clear();
	mMainMipViews.clear();
	mLayerMipViews.clear();
	device->destroyImage(mImage, nullptr);
	device->freeMemory(mMem, nullptr);
	device->destroyBuffer(mBuffer, nullptr);
}
/*
vk::ImageView *Texture::AsVulkanImageView(crossplatform::ShaderResourceType type, int index , int mip , bool rw)
{
    if (mip >= mips)
    {
        mip = 0;
    }
	if(!textureLoadComplete)
	{
		FinishLoading(renderPlatform->GetImmediateContext());
		textureLoadComplete=true;
	}

	bool no_array = !cubemap && (arraySize <= 1);

	// Return array SRV / return main SRV
	if (mips <= 1 && no_array || (index < 0 && mip < 0))
	{
        if (IsCubemap() && type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
        {
			return &mCubeArrayView;
        }
		return &mMainView;
	}

	// Return main SRV / return element of array
	if (mLayerViews.size() && (mip < 0 || mips <= 1))
	{
        if (index < 0 || no_array)
        {
			return &mMainView;
        }
		return &mLayerViews[index];
	}

	// Return main SRV to a MIP
    if (mMainMipViews.size() && (no_array || index < 0))
    {
		return &mMainMipViews[mip];
    }

	// Return main SRV to a MIP of a layer
    if (mLayerMipViews.size())
    {
		return &mLayerMipViews[index][mip];
    }

	return nullptr;
}
*/

void set_image_layout(vk::CommandBuffer *commandBuffer,vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
	vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages)
{
	assert(commandBuffer);

	auto DstAccessMask = [](vk::ImageLayout const &layout)
	{
		vk::AccessFlags flags;

		switch (layout)
		{
		case vk::ImageLayout::eTransferDstOptimal:
			// Make sure anything that was copying from this image has
			// completed
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

	auto const barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(DstAccessMask(newLayout))
		.setOldLayout(oldLayout)
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1));

	commandBuffer->pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits(), 0, nullptr, 0, nullptr, 1, &barrier);
}

void Texture::FinishLoading(crossplatform::DeviceContext &deviceContext)
{
	if(textureLoadComplete)
		return;
	vk::Device *device=renderPlatform->AsVulkanDevice();
	vk::CommandBuffer *commandBuffer=(vk::CommandBuffer *)deviceContext.platform_context;

	set_image_layout(commandBuffer,mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePreinitialized,
			vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer);
	for(int i=0;i<loadedTextures.size();i++)
	{
		LoadedTexture &lt=loadedTextures[i];
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
	imageLayout=vk::ImageLayout::eShaderReadOnlyOptimal;
	set_image_layout(commandBuffer,mImage, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal,
			imageLayout, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader);
	loadedTextures.clear();
	textureLoadComplete=true;
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

	// Base view:
	if ((mips <= 1 && no_array) || (layer < 0 && mip < 0))
	{
		if (cubemap && ((type & crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY) == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY))
		{
			return &mCubeArrayView;
		}
		return &mMainView;
	}
	// Layer view:
	if (mip < 0 || mips <= 1)
	{
		if (layer < 0 || no_array)
		{
			return &mMainView;
		}
		return &mLayerViews[layer];
	}
	// Mip view:
	if (layer < 0)
	{
		return &mMainMipViews[mip];
	}
	// Layer mip view:
	return &mLayerMipViews[layer][mip];
}

bool Texture::IsSame(int w, int h, int d, int arr, int m, crossplatform::PixelFormat f,bool msaa,bool comp,bool rt,bool ds,bool need_srv)
{
	// If we are not created yet...

	if (w != width || h != length || d != depth || m != mips||pixelFormat!=f)
	{
		return false;
	}

	return true;
}


void Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform* renderPlatform, void* t, void* srv, bool make_rt /*= false*/, bool setDepthStencil /*= false*/,bool need_srv /*= true*/)
{
	float qw, qh;
}

bool Texture::ensureTexture2DSizeAndFormat( crossplatform::RenderPlatform* r, int w, int l,
											crossplatform::PixelFormat f, bool computable /*= false*/, bool rendertarget /*= false*/, bool depthstencil /*= false*/, int num_samples /*= 1*/, int aa_quality /*= 0*/, bool wrap /*= false*/,
											vec4 clear /*= vec4(0.5f, 0.5f, 0.2f, 1.0f)*/, float clearDepth /*= 1.0f*/, uint clearStencil /*= 0*/)
{
	if (IsSame(w, l, 1, 1, 1,f,false,computable,rendertarget,depthstencil, true))
	{
		return true;
	}
	renderPlatform=r;
	// include eTransferDst IN CASE this is for a texture file loaded.
	vk::ImageUsageFlags usage=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst ;
	if(rendertarget)
		usage|=vk::ImageUsageFlagBits::eColorAttachment;
	if(depthstencil)
		usage|=vk::ImageUsageFlagBits::eDepthStencilAttachment;
	
	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);
	vk::FormatProperties props;
	vk::PhysicalDevice *gpu=((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();
	gpu->getFormatProperties(tex_format, &props);
	vk::Device *device=renderPlatform->AsVulkanDevice();
	
	vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(tex_format)
		.setExtent({ (uint32_t)w, (uint32_t)l, (int32_t)1 })
		.setMipLevels(1)
		.setArrayLayers(1)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::eUndefined);
	
	RETURN_FALSE_IF_FAILED( device->createImage(&imageCreateInfo, nullptr, &mImage));
	vk::MemoryRequirements mem_reqs;
	device->getImageMemoryRequirements(mImage, &mem_reqs);
	
	mem_alloc.setAllocationSize(mem_reqs.size);
	mem_alloc.setMemoryTypeIndex(0);

	if(!((vulkan::RenderPlatform*)renderPlatform)->memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( device->allocateMemory(&mem_alloc, nullptr, &mMem));

	 device->bindImageMemory(mImage, mMem, 0);

	InitViewTables(2,f,1, 1, rendertarget,false);

	pixelFormat=f;
	width=w;
	length=l;
	depth=1;
	mips=1;
	cubemap=false;
	return true;
}

void Texture::InitViewTables(int dim,crossplatform::PixelFormat f,int mipCount, int layers, bool isRenderTarget,bool cubemap)
{
	if(!renderPlatform)
		return;
	vk::Device *device=renderPlatform->AsVulkanDevice();
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
	else if(layers>1)
		viewType=vk::ImageViewType::e2DArray;
	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);
	int totalNum = cubemap ? 6 * layers : layers;

	vk::ImageViewCreateInfo viewCreateInfo = vk::ImageViewCreateInfo()
		.setImage(mImage)
		.setViewType(viewType)
		.setFormat(tex_format)
		.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipCount, 0, layers));
	SIMUL_VK_CHECK(device->createImageView(&viewCreateInfo, nullptr, &mMainView));
	
	// the mips of the main view.
	if(mipCount>1)
	{
		mMainMipViews.resize(mipCount);
		for(int i=0;i<mipCount;i++)
		{
			viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,i,1,0,layers));
			SIMUL_VK_CHECK(device->createImageView(&viewCreateInfo, nullptr, &mMainMipViews[i]));
		}
	}
	// cube array as 2d texture array.
	if(layers>1&&cubemap)
	{
		viewType=vk::ImageViewType::e2DArray;
		viewCreateInfo	.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipCount, 0, totalNum))
						.setViewType(viewType);
		SIMUL_VK_CHECK(device->createImageView(&viewCreateInfo, nullptr, &mCubeArrayView));
	}
	// layer views: individual layers of an array.
	if(dim==2&&totalNum>1)
	{
		if(cubemap)
			viewType=vk::ImageViewType::eCube;
		else
			viewType=vk::ImageViewType::e2D;
		viewCreateInfo.setViewType(viewType);
		mLayerViews.resize(totalNum);
		for(int i=0;i<totalNum;i++)
		{
			viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,0,mipCount,i,1));
			SIMUL_VK_CHECK(device->createImageView(&viewCreateInfo, nullptr, &mLayerViews[i]));
		}
	}
	mLayerMipViews.resize(totalNum);
	for (int i = 0; i < totalNum; i++)
	{
		mLayerMipViews[i].resize(mipCount);
		for (int j = 0; j < mipCount; j++)
		{
			viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,j,1,i,1));
			SIMUL_VK_CHECK(device->createImageView(&viewCreateInfo, nullptr, &mLayerMipViews[i][j]));
		}
	}
/*	if (isRenderTarget)
	{
		mTextureFBOs.resize(layers);
		for (int i = 0; i < layers; i++)
		{
			mTextureFBOs[i].resize(mipCount);
		}
	}*/
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* renderPlatform, int w, int l, int num, int m, crossplatform::PixelFormat f, bool computable , bool rendertarget , bool ascubemap )
{
	if (IsSame(w, l, 1, num, m,f,false,computable,rendertarget,depthStencil, true))
	{
		return true;
	}
	int totalNum	= ascubemap ? 6 * num : num;

	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);
	vk::FormatProperties props;
	vk::PhysicalDevice *gpu=((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();
	gpu->getFormatProperties(tex_format, &props);
	vk::Device *device=renderPlatform->AsVulkanDevice();
	// Include vk::ImageUsageFlagBits::eTransferDst IN CASE we're loading from a file...
	vk::ImageUsageFlags usage=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst;
	if(rendertarget)
		usage|=vk::ImageUsageFlagBits::eColorAttachment;
	vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(tex_format)
		.setExtent({ (uint32_t)w, (uint32_t)l, (uint32_t)1 })
		.setMipLevels(m)
		.setArrayLayers(totalNum)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::eUndefined);
	
	RETURN_FALSE_IF_FAILED( device->createImage(&imageCreateInfo, nullptr, &mImage));
	vk::MemoryRequirements mem_reqs;
	device->getImageMemoryRequirements(mImage, &mem_reqs);
	
	mem_alloc.setAllocationSize(mem_reqs.size);
	mem_alloc.setMemoryTypeIndex(0);

	if(!((vulkan::RenderPlatform*)renderPlatform)->memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( device->allocateMemory(&mem_alloc, nullptr, &mMem));

	 device->bindImageMemory(mImage, mMem, 0);
	
	InitViewTables(2,f,m, 1, rendertarget,ascubemap);

	pixelFormat=f;
	width=w;
	length=l;
	depth=1;
	arraySize=num;
	mips=m;
    cubemap   = ascubemap;
	return true;
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int d, crossplatform::PixelFormat f, bool computable /*= false*/, int m /*= 1*/, bool rendertargets /*= false*/)
{
	if (IsSame(w, l, d, 1, m,f,false,computable,rendertargets,false, true))
	{
		return true;
	}
	renderPlatform=r;

	pixelFormat=f;
	width=w;
	length=l;
	depth=d;
	mips=m;

	vk::Format tex_format = vulkan::RenderPlatform::ToVulkanFormat(f);
	vk::FormatProperties props;
	vk::PhysicalDevice *gpu=((vulkan::RenderPlatform*)renderPlatform)->GetVulkanGPU();
	gpu->getFormatProperties(tex_format, &props);
	vk::Device *device=renderPlatform->AsVulkanDevice();

	vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e3D)
		.setFormat(tex_format)
		.setExtent({ (uint32_t)w, (uint32_t)l, (uint32_t)d })
		.setMipLevels(m)
		.setArrayLayers(1)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(vk::ImageUsageFlagBits::eSampled)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::eUndefined);

	
	RETURN_FALSE_IF_FAILED( device->createImage(&imageCreateInfo, nullptr, &mImage));
	vk::MemoryRequirements mem_reqs;
	device->getImageMemoryRequirements(mImage, &mem_reqs);
	
	mem_alloc.setAllocationSize(mem_reqs.size);
	mem_alloc.setMemoryTypeIndex(0);

	if(!((vulkan::RenderPlatform*)renderPlatform)->memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc.memoryTypeIndex))
		return false;

	RETURN_FALSE_IF_FAILED( device->allocateMemory(&mem_alloc, nullptr, &mMem));

	 device->bindImageMemory(mImage, mMem, 0);
	
	InitViewTables(3,f,m, 1, false,false);
	return true;
}

void Texture::ClearDepthStencil(crossplatform::DeviceContext& deviceContext, float depthClear, int stencilClear)
{

}

void Texture::GenerateMips(crossplatform::DeviceContext& deviceContext)
{
	if (IsValid())
	{
	}
}

void Texture::setTexels(crossplatform::DeviceContext& deviceContext, const void* src, int texel_index, int num_texels)
{
	if (dim == 2)
	{
	}
	else if (dim == 3)
	{
	}
}

void Texture::activateRenderTarget(crossplatform::DeviceContext& deviceContext, int array_index /*= -1*/, int mip_index /*= 0*/)
{
	if (array_index == -1)
	{
		array_index = 0;
	}
	if (mip_index == -1)
	{
		mip_index = 0;
	}

	targetsAndViewport.num				= 1;
//	targetsAndViewport.m_rt[0]			= (void*)mTextureFBOs[array_index][mip_index];
	targetsAndViewport.m_dt			 = nullptr;
	targetsAndViewport.viewport.x		= 0;
	targetsAndViewport.viewport.y		= 0;
	targetsAndViewport.viewport.w		= std::max(1, (width >> mip_index));
	targetsAndViewport.viewport.h		= std::max(1, (length >> mip_index));

	// Activate the render target and set the viewport:
	GLuint id = GLuint(uintptr_t(targetsAndViewport.m_rt[0]));
	//glBindFramebuffer(GL_FRAMEBUFFER, id);
	deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

	// Cache it:
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
}

void Texture::deactivateRenderTarget(crossplatform::DeviceContext& deviceContext)
{
	deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
}

int Texture::GetLength()const
{
	return cubemap ? arraySize * 6 : arraySize;
}

int Texture::GetWidth()const
{
	return width;
}

int Texture::GetDimension()const
{
	return dim;
}

int Texture::GetSampleCount()const
{
	return 0;
}

bool Texture::IsComputable()const
{
	return true;
}

void Texture::copyToMemory(crossplatform::DeviceContext& deviceContext, void* target, int start_texel, int num_texels)
{

}
#include "Simul/Base/FileLoader.h"

void Texture::LoadTextureData(LoadedTexture &lt,const char* path)
{
	int index=simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(path,renderPlatform->GetTexturePathsUtf8());
	std::string filenameInUseUtf8=path;
	if(index==-2||index>=(int)renderPlatform->GetTexturePathsUtf8().size())
	{
		SIMUL_CERR<<"Failed to find texture file "<<filenameInUseUtf8<<std::endl;
		return;
	}
	else if(index<renderPlatform->GetTexturePathsUtf8().size())
		filenameInUseUtf8=(renderPlatform->GetTexturePathsUtf8()[index]+"/")+filenameInUseUtf8;

	lt.data			 = stbi_load(filenameInUseUtf8.c_str(), &lt.x, &lt.y, &lt.n, 4);
	if (!lt.data)
	{
		SIMUL_CERR << "Failed to load the texture: " << path << std::endl;
		return;
	}
	vk::Device *device=renderPlatform->AsVulkanDevice();
	vulkan::RenderPlatform *vkRenderPlatform=(vulkan::RenderPlatform *)renderPlatform;
	auto const buffer_create_info = vk::BufferCreateInfo()
		.setSize(lt.x * lt.y * 4*4)
		.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr);

	auto result = device->createBuffer(&buffer_create_info, nullptr, &lt.buffer);
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	vk::MemoryRequirements mem_reqs;
	device->getBufferMemoryRequirements(lt.buffer, &mem_reqs);

	lt.mem_alloc.setAllocationSize(mem_reqs.size);
	lt.mem_alloc.setMemoryTypeIndex(0);

	vk::MemoryPropertyFlags requirements = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	auto pass = vkRenderPlatform->memory_type_from_properties(mem_reqs.memoryTypeBits, requirements, &lt.mem_alloc.memoryTypeIndex);
	SIMUL_ASSERT(pass == true);

	result = device->allocateMemory(&lt.mem_alloc, nullptr, &(lt.mem));
	SIMUL_ASSERT(result == vk::Result::eSuccess);

	 device->bindBufferMemory(lt.buffer, lt.mem, 0);

	vk::SubresourceLayout layout;
	memset(&layout, 0, sizeof(layout));
	layout.rowPitch = lt.x * 4;
	auto data = device->mapMemory(lt.mem, 0, lt.mem_alloc.allocationSize);
	SIMUL_ASSERT(data !=nullptr);
	
	//memcpy(data, lt.data, lt.x * lt.y*4);
	uint8_t *rgba_data	=(uint8_t*)data;
	uint8_t *cPtr		=(uint8_t*)lt.data;
	for (int y = 0; y < lt.y; y++)
	{
		uint8_t *rowPtr = rgba_data;
		for (int x = 0; x < lt.x; x++)
		{
			memcpy(rowPtr, cPtr, 4);
			rowPtr += 4;
			cPtr += 4;
		}
		rgba_data += layout.rowPitch;
	}

	device->unmapMemory(lt.mem);
}




void Texture::InitViews(int mipCount, int layers, bool isRenderTarget)
{
	mLayerViews.resize(layers);

	mMainMipViews.resize(mipCount);

	mLayerMipViews.resize(layers);
	for (int i = 0; i < layers; i++)
	{
		mLayerMipViews[i].resize(mipCount);
	}

	if (isRenderTarget)
	{
		mTextureFBOs.resize(layers);
		for (int i = 0; i < layers; i++)
		{
			mTextureFBOs[i].resize(mipCount);
		}
	}
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

void Texture::SetVkName(const char* n)
{
}
