#ifndef FRAMEBUFFERGL_H
#define FRAMEBUFFERGL_H

#include <stack>
#include "stdint.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT FramebufferGL:public simul::crossplatform::BaseFramebuffer
		{
		public:
            FramebufferGL(const char *name = nullptr);
            virtual ~FramebufferGL();
            void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform) override;
            void ActivateDepth(crossplatform::DeviceContext &) override;
            void SetAntialiasing(int s) override;

            void InvalidateDeviceObjects() override;
            bool CreateBuffers() override;
            void Activate(crossplatform::DeviceContext &deviceContext) override;
            void SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth) override;
            void ActivateViewport(crossplatform::DeviceContext &deviceContext,float x,float y,float w,float h) override;
            void ActivateColour(crossplatform::DeviceContext& deviceContext, const float viewportXYWH[4])override;
            void Deactivate(crossplatform::DeviceContext &deviceContext) override;
            void DeactivateDepth(crossplatform::DeviceContext &deviceContext) override;
            void Clear(crossplatform::DeviceContext &deviceContext,float,float,float,float,float,int mask = 0) override;
            void ClearColour(crossplatform::DeviceContext &deviceContext,float,float,float,float) override;
            void SetWidthAndHeight(int w,int h,int mips = -1) override;
            void SetAsCubemap(int face_size,int num_mips = 1,crossplatform::PixelFormat f = crossplatform::RGBA_32_FLOAT) override;
            void SetFormat(crossplatform::PixelFormat) override;
            void SetDepthFormat(crossplatform::PixelFormat) override;
            bool IsValid() const override;

        protected:
            GLuint mFBOId;

		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#endif // RENDER_TEXTURE_FBO__H
