#pragma once
#include <vulkan/vulkan.hpp>

#include "Platform/CrossPlatform/Framebuffer.h"
#include "Platform/Vulkan/Export.h"
#include "Platform/Vulkan/Texture.h"
#include "stdint.h"
#include <stack>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace platform
{
	namespace vulkan
	{
		//! Vulkan Framebuffer implementation
		class SIMUL_VULKAN_EXPORT Framebuffer : public platform::crossplatform::Framebuffer
		{
		public:
			Framebuffer(const char *name = nullptr);
			virtual ~Framebuffer();
			void SetAntialiasing(int s) override;
			void InvalidateDeviceObjects() override;
			bool CreateBuffers() override;
			void Activate(crossplatform::GraphicsDeviceContext &deviceContext) override;
			void ActivateDepth(crossplatform::GraphicsDeviceContext &) override;
			void Deactivate(crossplatform::GraphicsDeviceContext &deviceContext) override;
			void DeactivateDepth(crossplatform::GraphicsDeviceContext &deviceContext) override;
			void SetAsCubemap(int face_size, int num_mips = 1, crossplatform::PixelFormat f = crossplatform::RGBA_MAX_FLOAT) override;
			void SetFormat(crossplatform::PixelFormat) override;
			void SetDepthFormat(crossplatform::PixelFormat) override;
			bool IsValid() const override;

			vk::Framebuffer *GetVulkanFramebuffer(crossplatform::GraphicsDeviceContext &deviceContext, int cube_face = -1);
			vk::RenderPass *GetVulkanRenderPass(crossplatform::GraphicsDeviceContext &deviceContext);

		protected:
			enum RPType
			{
				NONE = 0,
				COLOUR = 1,
				DEPTH = 2,
				CLEAR = 4
			};
			vk::RenderPass mDummyRenderPasses[8];
			std::vector<vk::Framebuffer> mFramebuffers[8];
			void InitVulkanFramebuffer(crossplatform::GraphicsDeviceContext &deviceContext);
			bool initialized = false;
			void InvalidateFramebuffers();
			bool clear_next = true;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
