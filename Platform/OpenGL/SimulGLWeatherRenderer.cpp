
#ifdef _MSC_VER
#include <GL/glew.h>
#include <GL/glut.h>
#endif

#include "FramebufferGL.h"
#include "LoadGLProgram.h"
#include "SimulGLWeatherRenderer.h"
#include "SimulGLSkyRenderer.h"
#include "SimulGLCloudRenderer.h"
#include "SimulGL2DCloudRenderer.h"
#include "SimulGLLightningRenderer.h"
#include "SimulGLAtmosphericsRenderer.h"
#include "SimulGLUtilities.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/Decay.h"

#ifdef _MSC_VER
	// for wglGetProcAddress
	#include <Windows.h>
#endif

static const GLenum buffer_tex_format		=GL_FLOAT;
static const GLenum internal_buffer_format	=GL_RGBA32F_ARB;

SimulGLWeatherRenderer::SimulGLWeatherRenderer(simul::clouds::Environment *env,bool usebuffer,bool tonemap,int width,
		int height,bool sky,bool clouds3d,bool clouds2d,bool rain)
		:BaseWeatherRenderer(env,sky,clouds3d,clouds2d,rain)
		,BufferWidth(0)
		,BufferHeight(0)
		,device_initialized(false)
		,scene_buffer(NULL)
{
	simul::sky::SkyKeyframer *sk=environment->skyKeyframer.get();
	simul::clouds::CloudKeyframer *ck2d=environment->cloud2DKeyframer.get();
	simul::clouds::CloudKeyframer *ck3d=environment->cloudKeyframer.get();
	if(ShowSky)
	{
		simulSkyRenderer=new SimulGLSkyRenderer(sk);
		baseSkyRenderer=simulSkyRenderer.get();
	}
	simulCloudRenderer=new SimulGLCloudRenderer(ck3d);
	baseCloudRenderer=simulCloudRenderer.get();
	base2DCloudRenderer=simul2DCloudRenderer=NULL;//new SimulGL2DCloudRenderer(ck2d);
	
	simulLightningRenderer=new SimulGLLightningRenderer(environment->lightning.get());
	baseLightningRenderer=simulLightningRenderer.get();

	simulAtmosphericsRenderer=new SimulGLAtmosphericsRenderer;
	baseAtmosphericsRenderer=simulAtmosphericsRenderer.get();

	EnableCloudLayers(clouds3d,clouds2d);
	SetScreenSize(width,height);
}


void SimulGLWeatherRenderer::EnableCloudLayers(bool clouds3d,bool clouds2d)
{
	simul::clouds::BaseWeatherRenderer::EnableCloudLayers(clouds3d,clouds2d);
	if(simulSkyRenderer)
	{
		if(device_initialized)
			simulSkyRenderer->RestoreDeviceObjects(NULL);
	}
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetBaseSkyInterface());
		simul2DCloudRenderer->Create();
		if(device_initialized)
			simul2DCloudRenderer->RestoreDeviceObjects(NULL);
	}
	if(simulCloudRenderer)
	{
		simulCloudRenderer->Create();
		if(device_initialized)
			simulCloudRenderer->RestoreDeviceObjects(NULL);
	}
	if(simulLightningRenderer)
	{
		if(device_initialized)
			simulLightningRenderer->RestoreDeviceObjects();
	}
	ConnectInterfaces();
}

SimulGLWeatherRenderer::~SimulGLWeatherRenderer()
{
	InvalidateDeviceObjects();
}

void SimulGLWeatherRenderer::SetScreenSize(int w,int h)
{
	simulAtmosphericsRenderer->SetBufferSize(w,h);
	BufferWidth=w/Downscale;
	BufferHeight=h/Downscale;
    if(scene_buffer)
	{
		delete scene_buffer;
		scene_buffer=new FramebufferGL(BufferWidth,BufferHeight,GL_TEXTURE_2D);
		scene_buffer->InitColor_Tex(0,internal_buffer_format,buffer_tex_format);
		scene_buffer->SetShader(0);
	}
}

void SimulGLWeatherRenderer::RestoreDeviceObjects(void*)
{
	GLenum res=glewInit();
	
	/*const char* extensionsString = (const char*)glGetString(GL_EXTENSIONS);
// If the GL_GREMEDY_string_marker extension is supported:
	if(glewIsSupported("GL_GREMEDY_string_marker"))
	{
		// Get a pointer to the glStringMarkerGREMEDY function:
		glStringMarkerGREMEDY = (PFNGLSTRINGMARKERGREMEDYPROC)wglGetProcAddress("glStringMarkerGREMEDY");
	}
	CheckGLError(__FILE__,__LINE__,res);
	if(!GLEW_VERSION_2_0)
	{
		std::cerr<<"GL ERROR: No OpenGL 2.0 support on this hardware!\n";
	}*/
//	CheckExtension("GL_VERSION_2_0");
	CheckExtension("GL_ARB_fragment_program");
	CheckExtension("GL_ARB_vertex_program");
	CheckExtension("GL_ARB_texture_float");
	CheckExtension("GL_ARB_color_buffer_float");
	CheckExtension("GL_EXT_framebuffer_object");
    if(scene_buffer)
        delete scene_buffer;
	scene_buffer=new FramebufferGL(BufferWidth,BufferHeight,GL_TEXTURE_2D);
	scene_buffer->InitColor_Tex(0,internal_buffer_format,buffer_tex_format);
	scene_buffer->SetShader(0);
	device_initialized=true;
	EnableCloudLayers(layer1,layer2);
	///simulSkyRenderer->RestoreDeviceObjects();
	//simulCloudRenderer->RestoreDeviceObjects(NULL);
	//simulLightningRenderer->RestoreDeviceObjects();
	simulAtmosphericsRenderer->RestoreDeviceObjects(NULL);
}
void SimulGLWeatherRenderer::InvalidateDeviceObjects()
{
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	if(simulLightningRenderer)
		simulLightningRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();
	if(scene_buffer)
		scene_buffer->InvalidateDeviceObjects();
}

bool SimulGLWeatherRenderer::RenderSky(bool buffered,bool is_cubemap)
{
	ERROR_CHECK
static simul::base::Timer timer;
	timer.TimeSum=0;
	timer.StartTime();
	BaseWeatherRenderer::RenderSky(buffered,is_cubemap);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	if(simulSkyRenderer)
	{
		ERROR_CHECK
		simulSkyRenderer->RenderPointStars();
		ERROR_CHECK
		simulSkyRenderer->RenderPlanets();
		ERROR_CHECK
		simulSkyRenderer->RenderSun();
		ERROR_CHECK
	}
	// Everything between Activate() and DeactivateAndRender() is drawn to the buffer object.
	if(buffered)
	{
		scene_buffer->Activate();
		glClearColor(0.f,0.f,0.f,1.f);
		ERROR_CHECK
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		ERROR_CHECK
	}
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	ERROR_CHECK
	if(simulSkyRenderer)
	{
		// We call the sky renderer, telling it to blend if we're not buffering the sky,
		// because otherwise it would overwrite the planets
		simulSkyRenderer->Render(!buffered);
	}
	// Do this AFTER sky render, to catch any changes to texture definitions:
	UpdateSkyAndCloudHookup();
	ERROR_CHECK
	timer.UpdateTime();
	simul::math::FirstOrderDecay(sky_timing,timer.Time,0.1f,0.01f);
	ERROR_CHECK
    if(simul2DCloudRenderer)
		simul2DCloudRenderer->Render(false,false,false);
	ERROR_CHECK
	// Render the sky to the screen, then set up to render the clouds to the buffer.
	if(buffered)
	{
		scene_buffer->DeactivateAndRender(true);
	}
	if(buffered)
	{
		scene_buffer->Activate();
		glClearColor(0.f,0.f,0.f,1.f);
		ERROR_CHECK
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		ERROR_CHECK
	}
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	if(simulCloudRenderer&&layer1)
		simulCloudRenderer->Render(false,false,UseDefaultFog);
	timer.UpdateTime();
	simul::math::FirstOrderDecay(cloud_timing,timer.Time,0.1f,0.01f);
	if(buffered)
		scene_buffer->DeactivateAndRender(true);
	ERROR_CHECK
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
	ERROR_CHECK
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	ERROR_CHECK
	int d=0;
	glGetIntegerv(GL_ATTRIB_STACK_DEPTH,&d);
	glPopAttrib();
	ERROR_CHECK
	timer.FinishTime();
	simul::math::FirstOrderDecay(final_timing,timer.Time,0.1f,0.01f);
	simul::math::FirstOrderDecay(total_timing,timer.TimeSum,0.1f,0.01f);
	return true;
}

void SimulGLWeatherRenderer::RenderLightning()
{
	if(simulCloudRenderer&&simulLightningRenderer&&layer1)
		simulLightningRenderer->Render();
}


void SimulGLWeatherRenderer::RenderClouds(bool buffered,bool depth_testing,bool default_fog)
{
static simul::base::Timer timer;
	timer.StartTime();
	ERROR_CHECK
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	if(buffered)
	{
		scene_buffer->Activate();
		glClearColor(0.f,0.f,0.f,1.f);
		ERROR_CHECK
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		ERROR_CHECK
	}
    if(simulCloudRenderer)
		simulCloudRenderer->Render(false,depth_testing,default_fog);
	ERROR_CHECK

	if(buffered)
		scene_buffer->DeactivateAndRender(true);
	ERROR_CHECK
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	glPopAttrib();
	ERROR_CHECK
	timer.FinishTime();
	simul::math::FirstOrderDecay(cloud_timing,timer.Time,0.1f,0.01f);
}


class SimulGLSkyRenderer *SimulGLWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer.get();
}

class SimulGLCloudRenderer *SimulGLWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer.get();
}

class SimulGL2DCloudRenderer *SimulGLWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer.get();
}

const char *SimulGLWeatherRenderer::GetDebugText() const
{
	static char debug_text[256];
	sprintf_s(debug_text,256,"RENDER %3.3g ms (clouds %3.3g ms, sky %3.3g ms, final %3.3g)\r\n"
		"UPDATE %3.3g ms (clouds %3.3g ms, sky %3.3g ms)",
			total_timing,cloud_timing,sky_timing,final_timing,
			total_update_timing,cloud_update_timing,sky_update_timing);
	return debug_text;
}

GLuint SimulGLWeatherRenderer::GetFramebufferTexture()
{
	return scene_buffer->GetColorTex(0);
}