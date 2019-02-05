#pragma once
#include "Simul/Platform/OpenGL/OpenGLCallbackInterface.h"
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/OpenGL/Export.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
typedef struct GLFWwindow GLFWwindow;

namespace simul
{
	namespace opengl
	{
		class RenderPlatform;
		SIMUL_OPENGL_EXPORT_CLASS DeviceManager
			: public crossplatform::GraphicsDeviceInterface
		{
		public:
			DeviceManager();
			virtual ~DeviceManager();
			// GDI:
			void	Initialize(bool use_debug, bool instrument, bool default_driver) override;
			bool	IsActive() const override;
			void	Shutdown() override;
			void*	GetDevice() override;
			void*	GetDeviceContext() override;
			int		GetNumOutputs() override;
			crossplatform::Output	GetOutput(int i) override;

			// OGLCI:
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();

			void Activate();
			void Deactivate();

			void ReloadTextures();
			// called late to start debug output.
			void InitDebugging();
			simul::opengl::RenderPlatform *renderPlatformOpenGL;
		protected:
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h);
			GLFWwindow* offscreen_context;
			HGLRC           hRC;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif