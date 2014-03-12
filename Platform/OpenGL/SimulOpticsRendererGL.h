#ifndef SIMULOPTICSRENDERERGL_H
#define SIMULOPTICSRENDERERGL_H
#include "Simul/Camera/BaseOpticsRenderer.h"
#include "Simul/Camera/LensFlare.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Platform/OpenGL/Export.h"
#include <vector>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif


SIMUL_OPENGL_EXPORT_CLASS SimulOpticsRendererGL:public simul::camera::BaseOpticsRenderer
{
public:
	SimulOpticsRendererGL(simul::base::MemoryInterface *m);
	virtual ~SimulOpticsRendererGL();
	virtual void RestoreDeviceObjects(void *device);
	virtual void InvalidateDeviceObjects();
	virtual void RenderFlare(void *context,float exposure,void *depthTexture,const float *v,const float *p,const float *dir,const float *light);
	virtual void RecompileShaders();

	GLuint			flare_program;
	GLint			flareColour_param;
	GLuint			flare_texture;
	GLint			flareTexture_param;

	std::vector<GLuint> halo_textures;
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#endif