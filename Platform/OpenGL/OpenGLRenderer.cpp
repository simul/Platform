#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#pragma warning(disable:4505)	// Fix GLUT warnings
#include <GL/glut.h>
#endif
#include "OpenGLRenderer.h"
// For font definition define:
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/LoadGLImage.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/SimulGLSkyRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLCloudRenderer.h"
#include "Simul/Platform/OpenGL/SimulGL2DCloudRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLTerrainRenderer.h"
#include "Simul/Platform/OpenGL/Profiler.h"
#include "Simul/Scene/Scene.h"
#include "Simul/Scene/Object.h"
#include "Simul/Scene/BaseObjectRenderer.h"
#include "Simul/Scene/BaseSceneRenderer.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include <stdint.h> // for uintptr_t

#pragma comment(lib,"opengl32")
#pragma comment(lib,"glew32")
#pragma comment(lib,"freeglut")
#ifdef USE_GLFX
#pragma comment(lib,"glfx")
#endif

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace simul;
using namespace opengl;

using namespace simul;
using namespace opengl;

simul::opengl::RenderPlatform *renderPlatform=NULL;

OpenGLRenderer::OpenGLRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m,bool init_glut)
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
	,sceneRenderer(NULL)
{
	simulHDRRenderer		=new SimulGLHDRRenderer(ScreenWidth,ScreenHeight);
	simulWeatherRenderer	=new SimulGLWeatherRenderer(env,NULL,ScreenWidth,ScreenHeight);
	simulOpticsRenderer		=new SimulOpticsRendererGL(m);
	simulTerrainRenderer	=new SimulGLTerrainRenderer(NULL);
	simulTerrainRenderer->SetBaseSkyInterface(simulWeatherRenderer->GetSkyKeyframer());
	if(!renderPlatform)
		renderPlatform		=new opengl::RenderPlatform;
	if(sc)
		sceneRenderer		=new scene::BaseSceneRenderer(sc,renderPlatform);
	simul::opengl::Profiler::GetGlobalProfiler().Initialize(NULL);
	
	//sceneCache=new scene::BaseObjectRenderer(gScene,&renderPlatform);
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
GL_ERROR_CHECK
	delete sceneRenderer;
GL_ERROR_CHECK
	delete renderPlatform;
GL_ERROR_CHECK
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
GL_ERROR_CHECK
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
GL_ERROR_CHECK
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
	renderPlatform->RestoreDeviceObjects(NULL);
	RecompileShaders();
}

void OpenGLRenderer::paintGL()
{
	void *context=NULL;
	static int viewport_id=0;
	
	const camera::CameraViewStruct &cameraViewStruct=cam->GetCameraViewStruct();
	crossplatform::DeviceContext deviceContext;
	deviceContext.renderPlatform=renderPlatform;
	deviceContext.viewStruct.view_id=viewport_id;
	deviceContext.viewStruct.view	=cam->MakeViewMatrix();
	if(ReverseDepth)
		deviceContext.viewStruct.proj	=(cam->MakeDepthReversedProjectionMatrix(cameraViewStruct.nearZ,cameraViewStruct.farZ,(float)ScreenWidth/(float)ScreenHeight));
	else
		deviceContext.viewStruct.proj	=(cam->MakeProjectionMatrix(cameraViewStruct.nearZ,cameraViewStruct.farZ,(float)ScreenWidth/(float)ScreenHeight));
	
	//simul::math::Matrix4x4 view;
	//glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	//simul::math::Matrix4x4 proj;
	//glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	// If called from some other OpenGL program, we should already have a modelview and projection matrix.
	// Here we will generate the modelview matrix from the camera class:
    glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(deviceContext.viewStruct.view);

	if(renderPlatform)
		renderPlatform->SetReverseDepth(ReverseDepth);
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	if(simulTerrainRenderer)
		simulTerrainRenderer->SetReverseDepth(ReverseDepth);
	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glPushAttrib(GL_ENABLE_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(deviceContext.viewStruct.proj);
	glViewport(0,0,ScreenWidth,ScreenHeight);
/*	crossplatform::ViewStruct viewStruct={	viewport_id
												,view
												,proj
												};*/
	static float exposure=1.0f;
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->PreRenderUpdate(deviceContext);
		/*GLuint fogMode[]={GL_EXP,GL_EXP2,GL_LINEAR};	// Storage For Three Types Of Fog
		GLuint fogfilter=0;								// Which Fog To Use
		simul::sky::float4 fogColor=simulWeatherRenderer->GetHorizonColour(0.001f*cam->GetPosition()[2]);
		glFogi(GL_FOG_MODE,fogMode[fogfilter]);			// Fog Mode
		glFogfv(GL_FOG_COLOR,fogColor);					// Set Fog Color
		glFogf(GL_FOG_DENSITY,0.35f);					// How Dense Will The Fog Be
		glFogf(GL_FOG_START,1.0f);						// Fog Start Depth
		glFogf(GL_FOG_END,5.0f);						// Fog End Depth*/
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
		
		if(sceneRenderer)
			sceneRenderer->Render(NULL,deviceContext.viewStruct);
//		gScene->OnTimerClick();
		
		if(simulTerrainRenderer&&ShowTerrain)
			simulTerrainRenderer->Render(deviceContext,1.f);
		simulWeatherRenderer->RenderCelestialBackground(deviceContext,exposure);
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
		
		simulWeatherRenderer->RenderSkyAsOverlay(deviceContext,false,exposure,UseSkyBuffer,depthFramebuffer.GetDepthTex()
			,depthFramebuffer.GetDepthTex()
			,simul::sky::float4(0,0,1.f,1.f),true);
		simulWeatherRenderer->DoOcclusionTests(deviceContext);
		simulWeatherRenderer->RenderPrecipitation(context);
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light,cam_pos;
			dir=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
			CalcCameraPosition(cam_pos);
			light=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetLocalIrradiance(cam_pos.z/1000.f);
			float occ=simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion();
			float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
			simulOpticsRenderer->RenderFlare(context,exp,depthFramebuffer.GetDepthTex(),deviceContext.viewStruct.view,deviceContext.viewStruct.proj,dir,light);
		}
		if(simulHDRRenderer&&UseHdrPostprocessor)
			simulHDRRenderer->FinishRender(context);
		if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(deviceContext);
		SetTopDownOrthoProjection(ScreenWidth,ScreenHeight);
		bool vertical_screen=ScreenHeight>ScreenWidth;
		if(ShowFades&&simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
		{
			int x0=ScreenWidth/2;
			int y0=8;
			if(vertical_screen)
			{
				x0=8;
				y0=ScreenHeight/2;
			}
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(deviceContext,x0,y0,ScreenWidth/2,ScreenHeight/2);
		}
		if(ShowCloudCrossSections&&simulWeatherRenderer->GetCloudRenderer()&&simulWeatherRenderer->GetCloudRenderer()->GetCloudKeyframer()->GetVisible())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(deviceContext,0,0,ScreenWidth/2,ScreenHeight/2);
			simulWeatherRenderer->GetCloudRenderer()->RenderAuxiliaryTextures(deviceContext,0,0,ScreenWidth/2,ScreenHeight/2);
		}
		if(Show2DCloudTextures&&simulWeatherRenderer->Get2DCloudRenderer()&&simulWeatherRenderer->Get2DCloudRenderer()->GetCloudKeyframer()->GetVisible())
		{
			simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(deviceContext,0,0,ScreenWidth,ScreenHeight);
		}
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
	int y=12;
	static int line_height=16;
	renderPlatform->Print(NULL,12,y+=line_height,"OpenGL");
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
		renderPlatform->Print(NULL,12,y+=line_height,osd_text);
		if(simulWeatherRenderer)
			renderPlatform->Print(NULL,12,y+=line_height,simulWeatherRenderer->GetDebugText());
		timer.StartTime();
	}
}

void OpenGLRenderer::resizeGL(int w,int h)
{
	ScreenWidth=w;
	ScreenHeight=h;
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetScreenSize(0,ScreenWidth,ScreenHeight);
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
	renderPlatform->RecompileShaders();
	if(simulHDRRenderer)
		simulHDRRenderer->RecompileShaders();
	if(simulWeatherRenderer)
		simulWeatherRenderer->RecompileShaders();
	if(simulTerrainRenderer)
		simulTerrainRenderer->RecompileShaders();
	SAFE_DELETE_PROGRAM(simple_program);
	simple_program=MakeProgram("simple.vert",NULL,"simple.frag");
}

void OpenGLRenderer::SaveScreenshot(const char *filename_utf8)
{
	SaveGLImage(filename_utf8,(GLuint)(simulHDRRenderer->framebuffer.GetColorTex()));
}


void OpenGLRenderer::ReverseDepthChanged()
{
	// We do not yet support ReverseDepth on OpenGL, because GL matrices do not take advantage of this.
	ReverseDepth=false;
}