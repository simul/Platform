#pragma once
#include "Simul/Platform/OpenGL/OpenGLCallbackInterface.h"
#include "Simul/Platform/OpenGL/SimulGLWeatherRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLHDRRenderer.h"
#include "Simul/Platform/OpenGL/SimulOpticsRendererGL.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/Meta/Group.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace camera
	{
		class Camera;
	}
}

SIMUL_OPENGL_EXPORT_CLASS OpenGLRenderer
	:public OpenGLCallbackInterface
	,public simul::graph::meta::Group
{
public:
	OpenGLRenderer(const char *license_key);
	virtual ~OpenGLRenderer();
	META_BeginProperties
		META_ValueProperty(bool,ShowFlares,"Whether to draw light flares around the sun and moon.")
		META_ValueProperty(bool,ShowFades,"Show the fade textures as an overlay.")
	META_EndProperties
	virtual void paintGL();
	virtual void resizeGL(int w,int h);
	virtual void initializeGL();
	virtual void renderUI();
	SimulGLWeatherRenderer *GetSimulGLWeatherRenderer(){return simulWeatherRenderer.get();}
	SimulGLHDRRenderer *GetSimulGLHDRRenderer(){return simulHDRRenderer.get();}
	void SetCamera(simul::camera::Camera *c);
	void SetYVertical(bool y);
	void ReloadShaders();
protected:
	simul::base::SmartPtr<SimulGLWeatherRenderer> simulWeatherRenderer;
	simul::base::SmartPtr<SimulGLHDRRenderer> simulHDRRenderer;
	simul::base::SmartPtr<SimulOpticsRendererGL> simulOpticsRenderer;
	int width,height;
	simul::camera::Camera *cam;
	bool y_vertical;
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif