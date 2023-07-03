#pragma once

#include <stack>
#include "stdint.h"
#include "Platform/OpenGL/Export.h"
#include "Platform/OpenGL/Texture.h"
#include "Platform/CrossPlatform/Framebuffer.h"
#include "glad/glad.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace platform
{
	namespace opengl
	{
        //! GL Framebuffer implementation
		class SIMUL_OPENGL_EXPORT FramebufferGL:public platform::crossplatform::Framebuffer
		{
		public:
            FramebufferGL(const char *name = nullptr);
            virtual ~FramebufferGL();
            void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform) override;
            void ActivateDepth(crossplatform::GraphicsDeviceContext &) override;
            void SetAntialiasing(int s) override;

            void InvalidateDeviceObjects() override;
            bool CreateBuffers() override;
            void Activate(crossplatform::GraphicsDeviceContext &deviceContext) override;
            void SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth) override;
            void Deactivate(crossplatform::GraphicsDeviceContext &deviceContext) override;
            void DeactivateDepth(crossplatform::GraphicsDeviceContext &deviceContext) override;
            void Clear(crossplatform::GraphicsDeviceContext &deviceContext,float,float,float,float,float,int mask = 0) override;
            void ClearColour(crossplatform::GraphicsDeviceContext &deviceContext,float,float,float,float) override;
            void SetWidthAndHeight(int w,int h,int mips = -1) override;
            void SetAsCubemap(int face_size,int num_mips = 1,crossplatform::PixelFormat f = crossplatform::RGBA_32_FLOAT) override;
            void SetFormat(crossplatform::PixelFormat) override;
            void SetDepthFormat(crossplatform::PixelFormat) override;
            bool IsValid() const override;

        protected:
            std::vector<GLuint> mFBOId;// either one fb or six, depending if it's a cubemap. times the number of mips.
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

