#pragma once

#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif

namespace vk
{
	class Instance;
}
namespace simul
{
	namespace vulkan
	{
		extern void SetVulkanName(crossplatform::RenderPlatform *renderPlatform,void *ds,const char *name);
		extern void SetVulkanName(crossplatform::RenderPlatform *renderPlatform,void *ds,const std::string &name);
		class Material;
        class Texture;

		//! Vulkan renderplatform implementation
		class SIMUL_VULKAN_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
		public:
			            RenderPlatform();
			virtual~    RenderPlatform() override;

			vk::Device *AsVulkanDevice() override;
			vk::Instance *AsVulkanInstance();
			vk::PhysicalDevice *GetVulkanGPU();
			const char* GetName() const override;
			crossplatform::RenderPlatformType GetType() const override
			{
				return crossplatform::RenderPlatformType::Vulkan;
			}
			virtual const char *GetSfxConfigFilename() const override
			{
				return "GLSL/GLSL.json";
			}
			void        RestoreDeviceObjects(void*) override;
			void        InvalidateDeviceObjects() override;
			void        BeginFrame() override;
			void        EndFrame() override;
			void		CopyTexture(crossplatform::DeviceContext& deviceContext, crossplatform::Texture *, crossplatform::Texture *) override;
			float		GetDefaultOutputGamma() const override;
            void        BeginEvent(crossplatform::DeviceContext& deviceContext, const char* name)override;
            void        EndEvent(crossplatform::DeviceContext& deviceContext)override;
			void        DispatchCompute(crossplatform::DeviceContext& deviceContext, int w, int l, int d) override;
			void        Draw(crossplatform::DeviceContext& deviceContext, int num_verts, int start_vert) override;
			void        DrawIndexed(crossplatform::DeviceContext& deviceContext, int num_indices, int start_index = 0, int base_vertex = 0) override;
			void        DrawLineLoop(crossplatform::DeviceContext& deviceContext, const double *mat, int num, const double *vertexArray, const float colr[4]) override;
			void        DrawTexture(crossplatform::DeviceContext& deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, vec4 mult, bool blend = false, float gamma = 1.0f, bool debug = false) override;
			void        DrawQuad(crossplatform::DeviceContext& deviceContext) override;
			void        DrawLines(crossplatform::DeviceContext& deviceContext, crossplatform::PosColourVertex* lines, int count, bool strip = false, bool test_depth = false, bool view_centred = false) override;
			void        Draw2dLines(crossplatform::DeviceContext& deviceContext, crossplatform::PosColourVertex* lines, int count, bool strip) override;
			void        DrawCircle(crossplatform::DeviceContext& context, const float *dir, float rads, const float* colr, bool fill = false) override;
            void        GenerateMips(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* t, bool wrap, int array_idx = -1)override;
            //! This should be called after a Draw/Dispatch command that uses
            //! textures. Here we will apply the textures.
			bool		ApplyContextState(crossplatform::DeviceContext &deviceContext,bool error_checking=true) override;

            void        InsertFences(crossplatform::DeviceContext& deviceContext);
            crossplatform::Material*                CreateMaterial();
			crossplatform::Texture*                 CreateTexture(const char *lFileNameUtf8= nullptr) override;
			crossplatform::BaseFramebuffer*         CreateFramebuffer(const char *name=nullptr) override;
			crossplatform::SamplerState*            CreateSamplerState(crossplatform::SamplerStateDesc *) override;
			crossplatform::Effect*                  CreateEffect() override;
			crossplatform::Effect*                  CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines) override;
			crossplatform::PlatformConstantBuffer*  CreatePlatformConstantBuffer() override;
			crossplatform::PlatformStructuredBuffer*CreatePlatformStructuredBuffer() override;
			crossplatform::Buffer*                  CreateBuffer() override;
			crossplatform::RenderState*             CreateRenderState(const crossplatform::RenderStateDesc &desc) override;
			crossplatform::Query*                   CreateQuery(crossplatform::QueryType type) override;
			crossplatform::Shader*                  CreateShader() override;

			crossplatform::DisplaySurface*			CreateDisplaySurface() override;
			void*                                   GetDevice();
			
			void									SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer,int start_index=0) override;
			void									ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth) override;
			void									DeactivateRenderTargets(crossplatform::DeviceContext &) override;
			void									SetViewports(crossplatform::DeviceContext &deviceContext,int num,const crossplatform::Viewport *vps) override;

			void									SetTopology(crossplatform::DeviceContext &deviceContext,crossplatform::Topology t) override;
			void									EnsureEffectIsBuilt				(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &options) override;

			void									StoreRenderState(crossplatform::DeviceContext &deviceContext) override;
			void									RestoreRenderState(crossplatform::DeviceContext &deviceContext) override;
			void									PopRenderTargets(crossplatform::DeviceContext &deviceContext) override;
			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s) override;
            void					                SetStandardRenderState(crossplatform::DeviceContext& deviceContext, crossplatform::StandardRenderState s)override;
			void									Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source) override;
			void									SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8) override;
			
            static vk::PrimitiveTopology			toVulkanTopology(crossplatform::Topology t);
			static vk::CullModeFlags				toVulkanCullFace(crossplatform::CullFaceMode c);
			static vk::PolygonMode					toVulkanPolygonMode(crossplatform::PolygonMode p);
			static vk::CompareOp					toVulkanComparison(crossplatform::DepthComparison d);
			static vk::BlendFactor					toVulkanBlendFactor(crossplatform::BlendOption o);
			static vk::BlendOp						toVulkanBlendOperation(crossplatform::BlendOperation o);
            static vk::Filter                       toVulkanMinFiltering(crossplatform::SamplerStateDesc::Filtering f);
            static vk::Filter                       toVulkanMaxFiltering(crossplatform::SamplerStateDesc::Filtering f);
			static vk::SamplerMipmapMode			toVulkanMipmapMode(crossplatform::SamplerStateDesc::Filtering f);
			static vk::SamplerAddressMode			toVulkanWrapping(crossplatform::SamplerStateDesc::Wrapping w);
			static vk::Format						ToVulkanFormat(crossplatform::PixelFormat p);
			static                                  crossplatform::PixelFormat FromVulkanFormat(vk::Format p);
			static vk::Format						ToVulkanExternalFormat(crossplatform::PixelFormat p);
			//static                                  Vulkanenum DataType(crossplatform::PixelFormat p);
			static int                              FormatTexelBytes(crossplatform::PixelFormat p);
			static int                              FormatCount(crossplatform::PixelFormat p);
			bool									memory_type_from_properties(uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t *typeIndex);
            
            //! Makes the handle resident only if its not resident already
			vulkan::Texture*						GetDummyTextureCube();
            //! Returns 2D dummy texture 1 white texel
            vulkan::Texture*                        GetDummy2D();
            //! Returns 3D dummy texture 1 white texel
            vulkan::Texture*                        GetDummy3D();
			//! Returns dummy texture chosen from resource type.
			vulkan::Texture *						GetDummyTexture(crossplatform::ShaderResourceType);
			
			vk::Framebuffer *						GetCurrentVulkanFramebuffer(crossplatform::DeviceContext& deviceContext);
			crossplatform::PixelFormat				GetActivePixelFormat(crossplatform::DeviceContext &deviceContext);
			
			uint32_t								FindMemoryType(uint32_t typeFilter,vk::MemoryPropertyFlags properties);
			void									CreatVulkanBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory,const char *name);			
			void									CreateVulkanRenderpass(vk::RenderPass &renderPass,int num_colour,crossplatform::PixelFormat pixelFormat,crossplatform::PixelFormat depthFormat=crossplatform::PixelFormat::UNKNOWN,bool clear=false,int numOfSamples=1);
			vk::RenderPass							*GetActiveVulkanRenderPass(crossplatform::DeviceContext &deviceContext);
			static void								SetDefaultColourFormat(crossplatform::PixelFormat p);
        private:
			vk::Instance		*vulkanInstance=nullptr;
			vk::PhysicalDevice	*vulkanGpu=nullptr;
			vk::Device			*vulkanDevice=nullptr;
            vulkan::Texture*    mDummy2D=nullptr;
            vulkan::Texture*    mDummy3D=nullptr;
            vulkan::Texture*    mDummyTextureCube=nullptr;
            vulkan::Texture*    mDummyTextureCubeArray=nullptr;
			vk::DescriptorPool mDescriptorPool;
			static crossplatform::PixelFormat defaultColourFormat;
		};
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
