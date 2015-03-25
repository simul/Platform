#pragma once
#include "Simul/Platform/OpenGL/OpenGLCallbackInterface.h"
#include "Simul/Platform/OpenGL/SimulGLWeatherRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLHDRRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/TrueSkyRenderer.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace clouds
	{
		class Environment;
	}
	namespace crossplatform
	{
		class Camera;
	}
	namespace scene
	{
		class Scene;
		class BaseSceneRenderer;
	}
	namespace terrain
	{
		class BaseTerrainRenderer;
	}
	namespace opengl
	{
		class RenderPlatform;
		SIMUL_OPENGL_EXPORT_CLASS OpenGLRenderer
			:public OpenGLCallbackInterface
			,public simul::clouds::TrueSkyRenderer
		{
		public:
			OpenGLRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m,bool init_glut=true);
			virtual ~OpenGLRenderer();
			META_BeginProperties
				META_ValueProperty(bool,ShowWater,"Show water surfaces.")
				META_ValueProperty(float,Exposure,"A linear multiplier for rendered brightness.")
			META_EndProperties
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
			void InvalidateDeviceObjects() override;

			int AddGLView() override;
			void RenderGL(int) override;
			void ResizeGL(int view_id,int w,int h) override;
			void InitializeGL() override;
			void ShutdownGL() override;

			simul::clouds::BaseWeatherRenderer *GetSimulWeatherRenderer(){return simulWeatherRenderer;}
			void ReloadTextures();
			void SaveScreenshot(const char *filename_utf8);
			simul::opengl::RenderPlatform *renderPlatformOpenGL;
		protected:
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h);
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif