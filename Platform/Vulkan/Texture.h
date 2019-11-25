#pragma once

#include "Simul/Platform/Vulkan/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
	#pragma warning(disable:4275)
#endif

namespace simul
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
            const unsigned char* data;
			vk::Buffer buffer;
			vk::MemoryAllocateInfo mem_alloc;
			vk::DeviceMemory mem;
			crossplatform::PixelFormat pixelFormat;
        };

		class SIMUL_VULKAN_EXPORT SamplerState:public crossplatform::SamplerState
		{
		public:
			        SamplerState();
			virtual ~SamplerState() override;
            void    Init(crossplatform::RenderPlatform*r,crossplatform::SamplerStateDesc* desc);
			void    InvalidateDeviceObjects() override;
            vk::Sampler *AsVulkanSampler() override;
        private:
			vk::Sampler									mSampler;
		};

		class SIMUL_VULKAN_EXPORT Texture:public simul::crossplatform::Texture
		{
		public:
			                Texture();
							~Texture() override;
            
            void            SetName(const char* n)override;

			void            LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8) override;
			void            LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files,int specify_mips=-1) override;
			bool            IsValid() const override;
			void            InvalidateDeviceObjects() override;
			virtual void    InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,int w,int l,crossplatform::PixelFormat f,bool make_rt=false, bool setDepthStencil=false,bool need_srv=true, int numOfSamples = 1) override;
			virtual void	InitFromExternalTexture(crossplatform::RenderPlatform *renderPlatform, const crossplatform::TextureCreate *textureCreate) override;

			bool            ensureTexture2DSizeAndFormat(   crossplatform::RenderPlatform *renderPlatform, int w, int l,
                                                            crossplatform::PixelFormat f, bool computable = false, bool rendertarget = false, bool depthstencil = false, int num_samples = 1, int aa_quality = 0, bool wrap = false,
															vec4 clear = vec4(0.5f, 0.5f, 0.2f, 1.0f), float clearDepth = 1.0f, uint clearStencil = 0) override;
			bool            ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int nmips,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool ascubemap=false) override;
			bool            ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat frmt,bool computable=false,int nmips=1,bool rendertargets=false) override;
			void            ClearDepthStencil(crossplatform::DeviceContext& deviceContext, float depthClear, int stencilClear) override;
			void            GenerateMips(crossplatform::DeviceContext& deviceContext) override;
			void            setTexels(crossplatform::DeviceContext& deviceContext,const void* src,int texel_index,int num_texels) override;
			int             GetLength() const override;
			int             GetWidth() const override;
			int             GetDimension() const override;
			int             GetSampleCount() const override;
			bool            IsComputable() const override;
			bool            HasRenderTargets() const override;
			void            copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels) override;
			vk::Image		&AsVulkanImage()
			{
				return mImage;
			}
            vk::ImageView	*AsVulkanImageView(crossplatform::ShaderResourceType type=crossplatform::ShaderResourceType::UNKNOWN, int layer = -1, int mip = -1, bool rw = false) override;
			vk::Framebuffer *GetVulkanFramebuffer(int layer = -1, int mip = -1);
		//	static vk::ImageView *GetDummyVulkanImageView(crossplatform::ShaderResourceType type);
			vk::RenderPass &GetRenderPass(crossplatform::DeviceContext &deviceContext);
			
			/// We need an active command list to finish loading a texture!
			void			FinishLoading(crossplatform::DeviceContext &deviceContext) override;

			/// We need a renderpass before we can create any framebuffers!
			void			InitFramebuffers(crossplatform::DeviceContext &deviceContext);
			void			RestoreExternalTextureState(crossplatform::DeviceContext &deviceContext);
			/// Transition EITHER the whole texture, OR a single mip/layer combination to the specified "layout" (actually more of a state than a layout.)
			void			SetLayout(crossplatform::DeviceContext &deviceContext,vk::ImageLayout imageLayout,int layer=-1,int mip=-1);
			/// Assume the texture will be in this layout due to internal Vulkan shenanigans.
			void			AssumeLayout(vk::ImageLayout imageLayout);
			/// Admit we have no idea what the layouts will end up as, so reset them when needed.
			void			SplitLayouts();
			/// Get the tracked current layout.
			vk::ImageLayout GetLayout(int layer=-1, int mip=-1) const;
        private:
			void			SetImageLayout(vk::CommandBuffer *commandBuffer,vk::Image image, vk::ImageAspectFlags aspectMask
											, vk::ImageLayout oldLayout, vk::ImageLayout newLayout
											, vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages,int m=0,int num_mips=0);
			void			InvalidateDeviceObjectsExceptLoaded();
			bool			IsSame(int w, int h, int d, int arr, int , crossplatform::PixelFormat f, int msaa_samples,bool computable,bool rt,bool ds,bool need_srv,bool cb=false);
            
			void			LoadTextureData(LoadedTexture &lt,const char* path);
			void			SetTextureData(LoadedTexture &lt,const void *data,int x,int y,int z,int n,crossplatform::PixelFormat f);
            //! Initializes the texture views
            void            InitViews(int mipCount, int layers, bool isRenderTarget);
            //! Creates the Framebuffers for this texture
            void            CreateFBOs(int sampleCount);
            //! Applies default sampling parameters to the texId texture
            void            SetDefaultSampling(GLuint texId);

			void			InitViewTables(int dim,crossplatform::PixelFormat f,int w,int h,int mipCount, int layers, bool isRenderTarget,bool cubemap,bool isDepthTarget);
			
			vk::Image									mImage;
			vk::Buffer									mBuffer;
			vk::ImageLayout								currentImageLayout{ vk::ImageLayout::eUndefined };
			std::vector<std::vector<vk::ImageLayout>>	mLayerMipLayouts;
			vk::ImageView								mMainView;
			vk::ImageView								mCubeArrayView;
			vk::ImageView								mFaceArrayView;
			vk::RenderPass								mRenderPass;

			std::vector<vk::ImageView>					mLayerViews;
			std::vector<vk::ImageView>					mMainMipViews;
			std::vector<std::vector<vk::ImageView>>		mLayerMipViews;
			
            std::vector<std::vector<vk::Framebuffer>>    mFramebuffers;
		
			vk::MemoryAllocateInfo mem_alloc;
			vk::DeviceMemory mMem;
			std::vector<LoadedTexture>					loadedTextures;
			bool split_layouts;
			int	 mNumSamples = 1;
			vk::ImageLayout mExternalLayout;
		public:
			vk::Image GetImage()const { return mImage; } //AJR
        };
	}

}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
