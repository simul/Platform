#pragma once
#include "Simul/Platform/OpenGL/OpenGLCallbackInterface.h"
#include "Simul/Platform/OpenGL/SimulGLWeatherRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLHDRRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/Meta/Group.h"

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
			,public simul::base::Referenced
		{
		public:
			OpenGLRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m,bool init_glut=true);
			virtual ~OpenGLRenderer();
			META_BeginProperties
				META_ValueProperty(bool,ShowCompositing			,"Show the multi-resolution compositing textures.")
				META_ValueProperty(bool,ShowFlares,"Whether to draw light flares around the sun and moon.")
				META_ValueProperty(bool,ShowTerrain,"Whether to draw the terrain.")
				META_ValueProperty(bool,UseHdrPostprocessor,"Whether to apply post-processing for exposure and gamma-correction using a post-processing renderer.")
				META_ValueProperty(bool,UseSkyBuffer,"Render the sky to a low-res buffer to increase performance.")
				META_ValueProperty(bool,ShowOSD,"Show diagnostics onscreen.")
				META_ValueProperty(bool,ShowWater,"Show water surfaces.")
				META_ValuePropertyWithSetCall(bool,ReverseDepth,ReverseDepthChanged,"Reverse the direction of the depth (Z) buffer, so that depth 0 is the far plane. We do not yet support ReverseDepth on OpenGL, because GL matrices do not take advantage of this.")
				META_ValueProperty(bool,MixCloudsAndTerrain,"If true, clouds are drawn twice: once behind the terrain, and once in front.")
				META_ValueProperty(float,Exposure,"A linear multiplier for rendered brightness.")
			META_EndProperties
			virtual void paintGL();
			virtual void resizeGL(int w,int h);
			virtual void initializeGL();
			virtual void shutdownGL();
			virtual void renderUI();
			void InvalidateDeviceObjects();
			simul::opengl::SimulGLWeatherRenderer *GetSimulGLWeatherRenderer(){return simulWeatherRenderer;}
			SimulGLHDRRenderer *GetSimulGLHDRRenderer(){return simulHDRRenderer;}
			class simul::terrain::BaseTerrainRenderer *GetTerrainRenderer(){return baseTerrainRenderer;}
			void SetCamera(simul::crossplatform::Camera *c);
			void ReloadTextures();
			void RecompileShaders();
			void SaveScreenshot(const char *filename_utf8);
		protected:
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h);
			void ReverseDepthChanged();
			simul::opengl::SimulGLWeatherRenderer *simulWeatherRenderer;
			SimulGLHDRRenderer *simulHDRRenderer;
			simul::crossplatform::BaseOpticsRenderer *baseOpticsRenderer;
			class simul::terrain::BaseTerrainRenderer *baseTerrainRenderer;
			scene::BaseSceneRenderer *sceneRenderer;
			FramebufferGL depthFramebuffer;
			simul::crossplatform::Camera *cam;
			int ScreenWidth,ScreenHeight;
			GLuint simple_program;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif