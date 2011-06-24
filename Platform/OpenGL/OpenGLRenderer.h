#pragma once
#include "Simul/Platform/OpenGL/OpenGLCallbackInterface.h"
#include "Simul/Platform/OpenGL/SimulGLWeatherRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLHDRRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Graph/Meta/Group.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace graph
	{
		namespace camera
		{
			class Camera;
		}
	}
}

SIMUL_OPENGL_EXPORT_CLASS OpenGLRenderer
	:public OpenGLCallbackInterface
	,public simul::graph::meta::Group
{
public:
	OpenGLRenderer();
	virtual ~OpenGLRenderer();
	virtual void paintGL();
	virtual void resizeGL(int w,int h);
	virtual void initializeGL();
	virtual void renderUI();
	SimulGLWeatherRenderer *GetSimulGLWeatherRenderer(){return simulWeatherRenderer.get();}
	void SetCamera(simul::graph::camera::Camera *c);
protected:
	simul::base::SmartPtr<SimulGLWeatherRenderer> simulWeatherRenderer;
	simul::base::SmartPtr<SimulGLHDRRenderer> simulHDRRenderer;
	int width,height;
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif