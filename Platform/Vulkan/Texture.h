#pragma once

#include "Simul/Platform/Vulkan/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
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
            unsigned char* data;
			vk::Buffer buffer;
			vk::MemoryAllocateInfo mem_alloc;
			vk::DeviceMemory mem;
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
			crossplatform::RenderPlatform *renderPlatform;
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
			virtual void    InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_rt=false, bool setDepthStencil=false,bool need_srv=true) override;
			bool            ensureTexture2DSizeAndFormat(   crossplatform::RenderPlatform *renderPlatform, int w, int l,
                                                            crossplatform::PixelFormat f, bool computable = false, bool rendertarget = false, bool depthstencil = false, int num_samples = 1, int aa_quality = 0, bool wrap = false,
															vec4 clear = vec4(0.5f, 0.5f, 0.2f, 1.0f), float clearDepth = 1.0f, uint clearStencil = 0) override;
			bool            ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int nmips,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool ascubemap=false) override;
			bool            ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat frmt,bool computable=false,int nmips=1,bool rendertargets=false) override;
			void            ClearDepthStencil(crossplatform::DeviceContext& deviceContext, float depthClear, int stencilClear) override;
			void            GenerateMips(crossplatform::DeviceContext& deviceContext) override;
			void            setTexels(crossplatform::DeviceContext& deviceContext,const void* src,int texel_index,int num_texels) override;
			void            activateRenderTarget(crossplatform::DeviceContext& deviceContext,int array_index=-1,int mip_index=0) override;
			void            deactivateRenderTarget(crossplatform::DeviceContext& deviceContext) override;
			int             GetLength() const override;
			int             GetWidth() const override;
			int             GetDimension() const override;
			int             GetSampleCount() const override;
			bool            IsComputable() const override;
			void            copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel,int num_texels) override;

            vk::ImageView	*AsVulkanImageView(crossplatform::ShaderResourceType type=crossplatform::ShaderResourceType::UNKNOWN, int layer = -1, int mip = -1, bool rw = false) override;
			
			// Need an active command list to finish loading a texture!
			void			FinishLoading(crossplatform::DeviceContext &deviceContext) override;
        private:
			bool			IsSame(int w, int h, int d, int arr, int , crossplatform::PixelFormat f, bool msaa,bool computable,bool rt,bool ds,bool need_srv);
            
			void			LoadTextureData(LoadedTexture &lt,const char* path);
            //! Initializes the texture views
            void            InitViews(int mipCount, int layers, bool isRenderTarget);
            //! Creates the Framebuffers for this texture
            void            CreateFBOs(int sampleCount);
            //! Applies default sampling parameters to the texId texture
            void            SetDefaultSampling(GLuint texId);
            //! Sets a debug label name to the provided texture _id_
            void            SetVkName(const char* n );

			void			InitViewTables(int dim,crossplatform::PixelFormat f,int mipCount, int layers, bool isRenderTarget,bool cubemap);


			vk::Image									mImage;
			vk::Buffer									mBuffer;
			vk::ImageLayout								imageLayout{ vk::ImageLayout::eUndefined };
			vk::ImageView								mMainView;
			vk::ImageView								mCubeArrayView;

			std::vector<vk::ImageView>					mLayerViews;
			std::vector<vk::ImageView>					mMainMipViews;
			std::vector<std::vector<vk::ImageView>>		mLayerMipViews;
		
			vk::MemoryAllocateInfo mem_alloc;
			vk::DeviceMemory mMem;
			std::vector<LoadedTexture>					loadedTextures;

          std::vector<std::vector<uint>>    mTextureFBOs;
        };
	}

}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
