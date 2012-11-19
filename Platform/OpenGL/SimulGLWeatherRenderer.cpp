
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
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/Decay.h"

#ifdef _MSC_VER
	// for wglGetProcAddress
	#include <Windows.h>
#endif

static const GLenum buffer_tex_format		=GL_FLOAT;
static const GLenum internal_buffer_format	=GL_RGBA32F_ARB;

SimulGLWeatherRenderer::SimulGLWeatherRenderer(simul::clouds::Environment *env,bool usebuffer,bool tonemap,int width,
		int height,bool sky,bool rain)
		:BaseWeatherRenderer(env,sky,rain)
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
	base2DCloudRenderer=simul2DCloudRenderer=new SimulGL2DCloudRenderer(ck2d);
	
	simulLightningRenderer=new SimulGLLightningRenderer(environment->lightning.get());
	baseLightningRenderer=simulLightningRenderer.get();

	simulAtmosphericsRenderer=new SimulGLAtmosphericsRenderer;
	baseAtmosphericsRenderer=simulAtmosphericsRenderer.get();

	EnableCloudLayers(true,true);
	SetScreenSize(width,height);
}


void SimulGLWeatherRenderer::EnableCloudLayers(bool clouds3d,bool clouds2d)
{
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
		baseFramebuffer=scene_buffer=new FramebufferGL(BufferWidth,BufferHeight,GL_TEXTURE_2D);
		
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
	if(!CheckExtension("GL_VERSION_2_0"))
		throw simul::base::RuntimeError("OpenGL version 2.0 is not supported on this hardware");
	CheckExtension("GL_ARB_fragment_program");
	CheckExtension("GL_ARB_vertex_program");
	CheckExtension("GL_ARB_texture_float");
	CheckExtension("GL_ARB_color_buffer_float");
	CheckExtension("GL_EXT_framebuffer_object");
    if(scene_buffer)
        delete scene_buffer;
	baseFramebuffer=scene_buffer=new FramebufferGL(BufferWidth,BufferHeight,GL_TEXTURE_2D);
	scene_buffer->InitColor_Tex(0,internal_buffer_format,buffer_tex_format);
	scene_buffer->SetShader(0);
	device_initialized=true;
	EnableCloudLayers(true,true);
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
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	BaseWeatherRenderer::RenderSky(buffered,is_cubemap);
	if(buffered)
		scene_buffer->Render(true);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	int d=0;
	glGetIntegerv(GL_ATTRIB_STACK_DEPTH,&d);
	glPopAttrib();
	return true;
}

bool SimulGLWeatherRenderer::RenderLateCloudLayer(bool buffer)
{
	if(simulCloudRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
	{
		scene_buffer->Activate();
		scene_buffer->Clear(0,0,0,1.f);
		simulCloudRenderer->Render(false,false,UseDefaultFog);
		scene_buffer->Deactivate();
		scene_buffer->Render(true);
		return false;
	}
	return false;
}

void SimulGLWeatherRenderer::RenderLightning()
{
	if(simulCloudRenderer&&simulLightningRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render();
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