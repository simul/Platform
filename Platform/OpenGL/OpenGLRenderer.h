#pragma once
#include "Simul/Platform/OpenGL/OpenGLCallbackInterface.h"
#include "Simul/Platform/OpenGL/SimulGLWeatherRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLHDRRenderer.h"
#include "Simul/Platform/OpenGL/SimulOpticsRendererGL.h"
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
	OpenGLRenderer(simul::clouds::Environment *env);
	virtual ~OpenGLRenderer();
	META_BeginProperties
		META_ValueProperty(bool,ShowCloudCrossSections,"Show cross-sections of the cloud volumes as an overlay.")
		META_ValueProperty(bool,ShowFlares,"Whether to draw light flares around the sun and moon.")
		META_ValueProperty(bool,ShowFades,"Show the fade textures as an overlay.")
		META_ValueProperty(bool,ShowTerrain,"Whether to draw the terrain.")
		META_ValueProperty(bool,UseHdrPostprocessor,"Whether to apply post-processing for exposure and gamma-correction using a post-processing renderer.")
		META_ValueProperty(bool,ShowOSD,"Show diagnostice onscren.")
	META_EndProperties
	virtual void paintGL();
	virtual void resizeGL(int w,int h);
	virtual void initializeGL();
	virtual void renderUI();
	void	SetCelestialDisplay(bool val);
	SimulGLWeatherRenderer *GetSimulGLWeatherRenderer(){return simulWeatherRenderer.get();}
	SimulGLHDRRenderer *GetSimulGLHDRRenderer(){return simulHDRRenderer.get();}
	class SimulGLTerrainRenderer *GetTerrainRenderer(){return simulTerrainRenderer.get();}
	void SetCamera(simul::camera::Camera *c);
	void SetYVertical(bool y);
	void RecompileShaders();
protected:
	simul::base::SmartPtr<SimulGLWeatherRenderer> simulWeatherRenderer;
	simul::base::SmartPtr<SimulGLHDRRenderer> simulHDRRenderer;
	simul::base::SmartPtr<SimulOpticsRendererGL> simulOpticsRenderer;
	simul::base::SmartPtr<class SimulGLTerrainRenderer> simulTerrainRenderer;
	int width,height;
	simul::camera::Camera *cam;
	bool celestial_display;
	bool y_vertical;
	Utilities ut;
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif