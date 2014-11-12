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
			virtual void paintGL();
			virtual void resizeGL(int view_id,int w,int h);
			virtual void initializeGL();
			virtual void shutdownGL();
			virtual void renderUI();
			simul::clouds::BaseWeatherRenderer *GetSimulWeatherRenderer(){return simulWeatherRenderer;}
			SimulGLHDRRenderer *GetSimulGLHDRRenderer(){return simulHDRRenderer;}
			class simul::terrain::BaseTerrainRenderer *GetTerrainRenderer(){return baseTerrainRenderer;}
			void	SetCamera(int view_id,const simul::crossplatform::CameraOutputInterface *c);
			void ReloadTextures();
			void RecompileShaders();
			void SaveScreenshot(const char *filename_utf8);
		protected:
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h);
			void ReverseDepthChanged();
			SimulGLHDRRenderer *simulHDRRenderer;
			FramebufferGL depthFramebuffer;
			GLuint simple_program;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif