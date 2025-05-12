#pragma once

#include <vulkan/vulkan.hpp>
#include "Platform/Vulkan/Export.h"
#include "Platform/Vulkan/Allocation.h"
#include "Platform/CrossPlatform/Texture.h"
#include <parallel_hashmap/phmap.h>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
	#pragma warning(disable:4275)
#endif

namespace platform
{
	namespace vulkan
	{
		// a temporary structure holding a loaded texture until it can be transferred to the main texture object.
		struct LoadedTexture
		{
			LoadedTexture():x(0),y(0),z(0),n(0),data(nullptr){}
			int x;
			int y;
			int z;
			int n;
			unsigned char* data;
			vk::Buffer buffer;
			AllocationInfo allocationInfo;
			crossplatform::PixelFormat pixelFormat;
		};

		class SIMUL_VULKAN_EXPORT SamplerState:public crossplatform::SamplerState
		{
		public:
					SamplerState();
			virtual ~SamplerState() override;
			void	Init(crossplatform::RenderPlatform*r,crossplatform::SamplerStateDesc* desc);
			void	InvalidateDeviceObjects() override;
			vk::Sampler *AsVulkanSampler() override;

		private:
			vk::Sampler		mSampler;
		};

		class SIMUL_VULKAN_EXPORT Texture:public platform::crossplatform::Texture
		{
		public:
			Texture();
			~Texture() override;

			void			SetName(const char* n)override;

			bool			LoadFromFile(crossplatform::RenderPlatform* r, const char* pFilePathUtf8, bool gen_mips) override;
			bool			LoadTextureArray(crossplatform::RenderPlatform *r, const std::vector<std::string> &texture_files, bool gen_mips) override;
			bool			IsValid() const override;
			void			InvalidateDeviceObjects() override;
			bool			InitFromExternalTexture2D(crossplatform::RenderPlatform* renderPlatform, void* t, int w, int l, crossplatform::PixelFormat f, bool make_rt = false, bool setDepthStencil = false, int numOfSamples = 1) override;
			bool			InitFromExternalTexture(crossplatform::RenderPlatform* renderPlatform, const crossplatform::TextureCreate* textureCreate) override;

			bool			ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform* renderPlatform, int w, int l, int m, crossplatform::PixelFormat f
													,std::shared_ptr<std::vector<std::vector<uint8_t>>> data
													,bool computable = false, bool rendertarget = false, bool depthstencil = false
													,int num_samples = 1, int aa_quality = 0, bool wrap = false
													,vec4 clear = vec4(0.0f, 0.0f, 0.0f, 0.0f)
													,float clearDepth = 0.0f
													,uint clearStencil = 0
													,bool shared = false
													,crossplatform::CompressionFormat compressionFormat = crossplatform::CompressionFormat::UNCOMPRESSED) override;
			bool			ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* renderPlatform, int w, int l, int num, int mips, crossplatform::PixelFormat f
													, std::shared_ptr<std::vector<std::vector<uint8_t>>> data,bool computable = false, bool rendertarget = false, bool depthstencil = false, bool cubemap = false,
													crossplatform::CompressionFormat compressionFormat = crossplatform::CompressionFormat::UNCOMPRESSED ) override;
			bool			ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform* renderPlatform, int w, int l, int d, crossplatform::PixelFormat frmt, bool computable = false, int nmips = 1, bool rendertargets = false) override;

			bool			ensureVideoTexture(crossplatform::RenderPlatform* renderPlatform, int w, int l, crossplatform::PixelFormat f, crossplatform::VideoTextureType texType) override;
			void			ClearColour(crossplatform::GraphicsDeviceContext &deviceContext, vec4 colourClear) override;
			void			ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear) override;
			void			GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext) override;
			void			setTexels(crossplatform::DeviceContext& deviceContext, const void* src, int texel_index, int num_texels) override;
			void			setTexels(const void* src, int texel_index, int num_texels) override;
			int				GetLength() const override;
			int				GetWidth() const override;
			int				GetDimension() const override;
			int				GetSampleCount() const override;
			bool			IsComputable() const override;
			bool			HasRenderTargets() const override;
			void			copyToMemory(crossplatform::DeviceContext& deviceContext, void* target, int start_texel, int num_texels) override;

			vk::Image*		AsVulkanImage() override { return &mImage; }
			vk::ImageView*	AsVulkanImageView( crossplatform::TextureView textureView) override;
			vk::ImageView*	CreateVulkanImageView(crossplatform::TextureView textureView);

			/// We need an active command list to finish loading a texture!
			void			FinishLoading(crossplatform::DeviceContext& deviceContext) override;

			void			StoreExternalState(crossplatform::ResourceState) override;
			void			RestoreExternalTextureState(crossplatform::DeviceContext& deviceContext) override;

			void			InitViewTable(int l, int m);

			bool			AreSubresourcesInSameState(crossplatform::SubresourceRange subresourceRange = crossplatform::DefaultSubresourceRange) const;
			/// Transition a Subresource Range to the specified layout.
			void			SetLayout(crossplatform::DeviceContext& deviceContext, vk::ImageLayout imageLayout, const crossplatform::SubresourceRange& subresourceRange);
			/// Assume the texture will be in this layout.
			void			AssumeLayout(vk::ImageLayout imageLayout);
			/// Get the tracked current layout.
			vk::ImageLayout GetLayout(crossplatform::DeviceContext& deviceContext, const crossplatform::SubresourceRange& subresourceRange);
			
			void			Reallocate(crossplatform::GraphicsDeviceContext& deviceContext,const void *src,const void *dest) override;

		private:
			void			SetImageLayout(vk::CommandBuffer* commandBuffer, vk::Image image, vk::ImageAspectFlags aspectMask
											, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages, crossplatform::SubresourceRange subresourceRange = crossplatform::DefaultSubresourceRange);
			void			InvalidateDeviceObjectsExceptLoaded();
			bool			IsSame(int w, int h, int d, int arr, int, crossplatform::PixelFormat f, int msaa_samples, bool computable, bool rt, bool ds, bool cb = false);

			void			LoadTextureData(LoadedTexture& lt, const char* path);
			void			SetTextureData(LoadedTexture& lt, const void* data, int x, int y, int z, int n, crossplatform::PixelFormat f,crossplatform::CompressionFormat c=crossplatform::CompressionFormat::UNCOMPRESSED);
			
			void			ClearLoadedTextures();
			void			ResizeLoadedTextures(size_t mips, size_t layers);
			void			PushLoadedTexturesToReleaseManager();
			void			UpdateSplitLayoutsFlag(); //This must be called after modifying mCurrentImageLayout!

			vk::Image										mImage;
			AllocationInfo									mAllocationInfo;

			//! Full resource state
			vk::ImageLayout									mCurrentImageLayout{ vk::ImageLayout::eUndefined };
			//! States of the subresources mSubResourcesLayouts[index][mip]
			std::vector<std::vector<vk::ImageLayout>>		mSubResourcesLayouts;
			bool											mSplitLayouts = true;
			
			phmap::flat_hash_map<uint64_t, vk::ImageView *> mImageViews;
			vk::ImageView*									mDefaultImageView=nullptr;
			std::vector<std::vector<LoadedTexture>>			mLoadedTextures; //By mip, then by layer.
			int												mNumSamples = 1;
			vk::ImageLayout									mExternalLayout;
			vk::ImageCreateInfo imageCreateInfo; //!< Stored for defragmentation purposes.
		};
	}

}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
