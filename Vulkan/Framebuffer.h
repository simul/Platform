#pragma once

#include <stack>
#include "stdint.h"
#include "Platform/Vulkan/Export.h"
#include "Platform/Vulkan/Texture.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
        //! Vulkan Framebuffer implementation
		class SIMUL_VULKAN_EXPORT Framebuffer:public simul::crossplatform::BaseFramebuffer
		{
		public:
            Framebuffer(const char *name = nullptr);
            virtual ~Framebuffer();
            void RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
            void ActivateDepth(crossplatform::DeviceContext &) override;
            void SetAntialiasing(int s) override;

            void InvalidateDeviceObjects() override;
            bool CreateBuffers() override;
            void Activate(crossplatform::DeviceContext &deviceContext) override;
            void SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth) override;
            void Deactivate(crossplatform::DeviceContext &deviceContext) override;
            void DeactivateDepth(crossplatform::DeviceContext &deviceContext) override;
            void Clear(crossplatform::DeviceContext &deviceContext,float,float,float,float,float,int mask = 0) override;
            void ClearColour(crossplatform::DeviceContext &deviceContext,float,float,float,float) override;
            void SetWidthAndHeight(int w,int h,int mips = -1) override;
            void SetAsCubemap(int face_size,int num_mips = 1,crossplatform::PixelFormat f = crossplatform::RGBA_32_FLOAT) override;
            void SetFormat(crossplatform::PixelFormat) override;
            void SetDepthFormat(crossplatform::PixelFormat) override;
            bool IsValid() const override;
			
			vk::Framebuffer *GetVulkanFramebuffer(crossplatform::DeviceContext &deviceContext,int cube_face=-1);
			vk::RenderPass *GetVulkanRenderPass(crossplatform::DeviceContext &deviceContext);
        protected:
			enum RPType
			{
				NONE=0
				,COLOUR=1
				,DEPTH=2
				,CLEAR=4
			};
			vk::RenderPass mDummyRenderPasses[8];
			std::vector<vk::Framebuffer> mFramebuffers[8];
			void InitVulkanFramebuffer(crossplatform::DeviceContext &deviceContext);
			bool initialized=false;
			void InvalidateFramebuffers();
			bool clear_next=true;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

