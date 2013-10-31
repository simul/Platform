#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#pragma warning(disable:4505)	// Fix GLUT warnings
#include <GL/glut.h>
#endif
#include "OpenGLRenderer.h"
// For font definition define:
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/SimulGLSkyRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLCloudRenderer.h"
#include "Simul/Platform/OpenGL/SimulGL2DCloudRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLTerrainRenderer.h"
#include "Simul/Platform/OpenGL/Profiler.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include <stdint.h> // for uintptr_t

#pragma comment(lib,"opengl32")
#pragma comment(lib,"glew32")
#pragma comment(lib,"freeglut")
#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif


#ifndef GLUT_BITMAP_HELVETICA_12
#define GLUT_BITMAP_HELVETICA_12	((void*)7)
#endif
using namespace simul::opengl;
OpenGLRenderer::OpenGLRenderer(simul::clouds::Environment *env,simul::base::MemoryInterface *m,bool init_glut)
	:ScreenWidth(0)
	,ScreenHeight(0)
	,cam(NULL)
	,ShowFlares(true)
	,ShowFades(false)
	,ShowTerrain(true)
	,ShowCloudCrossSections(false)
	,Show2DCloudTextures(false)
	,CelestialDisplay(false)
	,UseHdrPostprocessor(true)
	,ShowOSD(false)
	,ShowWater(true)
	,ReverseDepth(false)
	,MixCloudsAndTerrain(false)
	,Exposure(1.0f)
	,simple_program(0)
{
	simulHDRRenderer	=new SimulGLHDRRenderer(ScreenWidth,ScreenHeight);
	simulWeatherRenderer=new SimulGLWeatherRenderer(env,NULL,ScreenWidth,ScreenHeight);
	simulOpticsRenderer	=new SimulOpticsRendererGL(m);
	simulTerrainRenderer=new SimulGLTerrainRenderer(NULL);
	simulTerrainRenderer->SetBaseSkyInterface(simulWeatherRenderer->GetSkyKeyframer());
	simul::opengl::Profiler::GetGlobalProfiler().Initialize(NULL);
	if(init_glut)
	{
		char argv[]="no program";
		char *a=argv;
		int argc=1;
	    glutInit(&argc,&a);
}
}

OpenGLRenderer::~OpenGLRenderer()
{
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	simul::opengl::Profiler::GetGlobalProfiler().Uninitialize();
	depthFramebuffer.InvalidateDeviceObjects();
	SAFE_DELETE_PROGRAM(simple_program);
	delete simulHDRRenderer;
	delete simulWeatherRenderer;
	delete simulOpticsRenderer;
	delete simulTerrainRenderer;
}

void OpenGLRenderer::initializeGL()
{
GL_ERROR_CHECK
	//glewExperimental=GL_TRUE;
    GLenum glewError = glewInit();
    if( glewError != GLEW_OK )
    {
        std::cerr<<"Error initializing GLEW! "<<glewGetErrorString( glewError )<<"\n";
        return;
    }
    //Make sure OpenGL 2.1 is supported
    if( !GLEW_VERSION_2_1 )
    {
        std::cerr<<"OpenGL 2.1 not supported!\n" ;
        return;
    }
GL_ERROR_CHECK
	if(!GLEW_VERSION_2_0)
	{
		std::cerr<<"GL ERROR: No OpenGL 2.0 support on this hardware!\n";
	}
	CheckExtension("GL_VERSION_2_0");
GL_ERROR_CHECK
	const GLubyte* pVersion = glGetString(GL_VERSION); 
	std::cout<<"GL_VERSION: "<<pVersion<<std::endl;
GL_ERROR_CHECK
	depthFramebuffer.InitColor_Tex(0,GL_RGBA32F_ARB);
	depthFramebuffer.SetDepthFormat(GL_DEPTH_COMPONENT32F);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(NULL);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(NULL);
	if(simulTerrainRenderer)
		simulTerrainRenderer->RestoreDeviceObjects(NULL);
	RecompileShaders();
}

void OpenGLRenderer::paintGL()
{
	void *context=NULL;
	static int viewport_id=0;
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	if(simulTerrainRenderer)
		simulTerrainRenderer->SetReverseDepth(ReverseDepth);
	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glPushAttrib(GL_ENABLE_BIT);
	// If called from some other OpenGL program, we should already have a modelview and projection matrix.
	// Here we will generate the modelview matrix from the camera class:
    glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(cam->MakeViewMatrix());
	glMatrixMode(GL_PROJECTION);
	static float nearPlane=1.0f;
	static float farPlane=250000.f;
	if(ReverseDepth)
		glLoadMatrixf(cam->MakeDepthReversedProjectionMatrix(nearPlane,farPlane,(float)ScreenWidth/(float)ScreenHeight));
	else
		glLoadMatrixf(cam->MakeProjectionMatrix(nearPlane,farPlane,(float)ScreenWidth/(float)ScreenHeight,false));
	glViewport(0,0,ScreenWidth,ScreenHeight);
	static float exposure=1.0f;
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->PreRenderUpdate(context);
		GLuint fogMode[]={GL_EXP,GL_EXP2,GL_LINEAR};	// Storage For Three Types Of Fog
		GLuint fogfilter=0;								// Which Fog To Use
		simul::sky::float4 fogColor=simulWeatherRenderer->GetHorizonColour(0.001f*cam->GetPosition()[2]);
		glFogi(GL_FOG_MODE,fogMode[fogfilter]);			// Fog Mode
		glFogfv(GL_FOG_COLOR,fogColor);					// Set Fog Color
		glFogf(GL_FOG_DENSITY,0.35f);					// How Dense Will The Fog Be
		glFogf(GL_FOG_START,1.0f);						// Fog Start Depth
		glFogf(GL_FOG_END,5.0f);						// Fog End Depth
		glDisable(GL_FOG);
GL_ERROR_CHECK
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			simulHDRRenderer->StartRender(context);
			//simulWeatherRenderer->SetExposureHint(simulHDRRenderer->GetExposure());
		}
		else
		{
			glClearColor(0,0,0,1.f);
			glClearDepth(ReverseDepth?0.f:1.f);
			glDepthMask(GL_TRUE);
			glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		}
		depthFramebuffer.Activate(context);
		depthFramebuffer.Clear(context,0.f,0.f,0.f,0.f,1.f,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		if(simulTerrainRenderer&&ShowTerrain)
			simulTerrainRenderer->Render(context,1.f);
		simulWeatherRenderer->RenderCelestialBackground(context,exposure);
		depthFramebuffer.Deactivate(context);
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)(uintptr_t)depthFramebuffer.GetColorTex());
			glUseProgram(simple_program);
			GLint image_texture		=glGetUniformLocation(simple_program,"image_texture");
			glUniform1i(image_texture,0);
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			glDisable(GL_BLEND);
			depthFramebuffer.Render(context,false);
			glBindTexture(GL_TEXTURE_2D,(GLuint)0);
		}
		simulWeatherRenderer->RenderLightning(context,viewport_id);
		simulWeatherRenderer->RenderSkyAsOverlay(context,exposure,UseSkyBuffer,false,depthFramebuffer.GetDepthTex(),viewport_id,simul::sky::float4(0,0,1.f,1.f));
		simulWeatherRenderer->DoOcclusionTests();
		simulWeatherRenderer->RenderPrecipitation(context);
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light,cam_pos;
			dir=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
			CalcCameraPosition(cam_pos);
			light=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetLocalIrradiance(cam_pos.z/1000.f);
			float occ=simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion();
			float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
			simulOpticsRenderer->RenderFlare(context,exp,dir,light);
		}
		if(simulHDRRenderer&&UseHdrPostprocessor)
			simulHDRRenderer->FinishRender(context);
		if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(context,ScreenWidth,ScreenHeight);
		SetTopDownOrthoProjection(ScreenWidth,ScreenHeight);
		if(ShowFades&&simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(context,ScreenWidth,ScreenHeight);
		if(ShowCloudCrossSections&&simulWeatherRenderer->GetCloudRenderer()&&simulWeatherRenderer->GetCloudRenderer()->GetCloudKeyframer()->GetVisible())
			simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(context,ScreenWidth,ScreenHeight);
		if(Show2DCloudTextures&&simulWeatherRenderer->Get2DCloudRenderer()&&simulWeatherRenderer->Get2DCloudRenderer()->GetCloudKeyframer()->GetVisible())
			simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(context,ScreenWidth,ScreenHeight);
		if(ShowOSD&&simulWeatherRenderer->GetCloudRenderer())
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(NULL,ScreenWidth,ScreenHeight);
	}
	renderUI();
	glPopAttrib();
	simul::opengl::Profiler::GetGlobalProfiler().EndFrame();
}

void OpenGLRenderer::renderUI()
{
	glUseProgram(0);
	glDisable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,0);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_1D);
	SetOrthoProjection(ScreenWidth,ScreenHeight);
	static char text[500];
	float y=12.f;
	static int line_height=16;
	RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,"OpenGL");
	if(ShowOSD)
	{
	static simul::base::Timer timer;
		timer.TimeSum=0;
		float t=timer.FinishTime();
		if(t<1.f)
			t=1.f;
		if(t>1000.f)
			t=1000.f;
		static float framerate=1.f;
		framerate*=0.95f;
		framerate+=0.05f*(1000.f/t);
		static char osd_text[256];
		sprintf_s(osd_text,256,"%3.3f fps",framerate);
		RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,osd_text);
		if(simulWeatherRenderer)
			RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,simulWeatherRenderer->GetDebugText());
		timer.StartTime();
	}
}

void OpenGLRenderer::resizeGL(int w,int h)
{
	ScreenWidth=w;
	ScreenHeight=h;
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetScreenSize(ScreenWidth,ScreenHeight);
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(ScreenWidth,ScreenHeight);
	depthFramebuffer.SetWidthAndHeight(ScreenWidth,ScreenHeight);
}

void OpenGLRenderer::SetCamera(simul::camera::Camera *c)
{
	cam=c;
}

void OpenGLRenderer::ReloadTextures()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->ReloadTextures();
}

void OpenGLRenderer::RecompileShaders()
{
	if(simulHDRRenderer)
		simulHDRRenderer->RecompileShaders();
	if(simulWeatherRenderer)
		simulWeatherRenderer->RecompileShaders();
	if(simulTerrainRenderer)
		simulTerrainRenderer->RecompileShaders();
	SAFE_DELETE_PROGRAM(simple_program);
	simple_program=MakeProgram("simple.vert",NULL,"simple.frag");
}

void OpenGLRenderer::ReverseDepthChanged()
{
	// We do not yet support ReverseDepth on OpenGL, because GL matrices do not take advantage of this.
	ReverseDepth=false;
}