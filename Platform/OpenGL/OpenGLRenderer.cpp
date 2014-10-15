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
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/SimulGLCloudRenderer.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Platform/OpenGL/Profiler.h"
#include "Simul/Scene/Scene.h"
#include "Simul/Scene/Object.h"
#include "Simul/Scene/BaseObjectRenderer.h"
#include "Simul/Scene/BaseSceneRenderer.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include <stdint.h> // for uintptr_t

#pragma comment(lib,"opengl32")
#pragma comment(lib,"glfx")

#ifdef SIMUL_DYNAMIC_LINK
#ifdef _DEBUG
#pragma comment(lib,"glew32d")
#else
#pragma comment(lib,"glew32")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib,"glew32sd")
#else
#pragma comment(lib,"glew32s")
#endif
#endif

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace simul;
using namespace opengl;

using namespace simul;
using namespace opengl;
static bool glut_initialized=false;
simul::opengl::RenderPlatform *renderPlatform=NULL;

OpenGLRenderer::OpenGLRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m,bool init_glut)
	:ScreenWidth(0)
	,ScreenHeight(0)
	,cam(NULL)
	,ShowCompositing(false)
	,ShowFlares(true)
	,ShowTerrain(true)
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
	baseOpticsRenderer		=new simul::crossplatform::BaseOpticsRenderer(m);
	baseTerrainRenderer		=new simul::terrain::BaseTerrainRenderer(NULL);
	baseTerrainRenderer->SetBaseSkyInterface(simulWeatherRenderer->GetSkyKeyframer());
	if(!renderPlatform)
		renderPlatform		=new opengl::RenderPlatform;
	if(sc)
		sceneRenderer		=new scene::BaseSceneRenderer(sc,renderPlatform);
	simul::opengl::Profiler::GetGlobalProfiler().Initialize(NULL);
	
	//sceneCache=new scene::BaseObjectRenderer(gScene,&renderPlatform);
	if(init_glut&&!glut_initialized)
	{
		char argv[]="no program";
		char *a=argv;
		int argc=1;
	    glutInit(&argc,&a);
		glut_initialized=true;
	}
}

void OpenGLRenderer::InvalidateDeviceObjects()
{
	int err=errno;
	std::cout<<"Errno "<<err<<std::endl;
	errno=0;
ERRNO_CHECK
GL_ERROR_CHECK
	SAFE_DELETE_PROGRAM(simple_program);
GL_ERROR_CHECK
	if(baseTerrainRenderer)
		baseTerrainRenderer->InvalidateDeviceObjects();
GL_ERROR_CHECK
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
GL_ERROR_CHECK
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	simul::opengl::Profiler::GetGlobalProfiler().Uninitialize();
	depthFramebuffer.InvalidateDeviceObjects();
GL_ERROR_CHECK
}

OpenGLRenderer::~OpenGLRenderer()
{
GL_ERROR_CHECK
	InvalidateDeviceObjects();
GL_ERROR_CHECK
	delete sceneRenderer;
GL_ERROR_CHECK
	delete renderPlatform;
	delete simulHDRRenderer;
	delete simulWeatherRenderer;
	delete baseOpticsRenderer;
	delete baseTerrainRenderer;
}

void OpenGLRenderer::initializeGL()
{
GL_ERROR_CHECK
ERRNO_CHECK
    GLenum glewError = glewInit();
    if( glewError != GLEW_OK )
    {
        std::cerr<<"Error initializing GLEW! "<<glewGetErrorString( glewError )<<"\n";
        return;
    }
ERRNO_CHECK
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
ERRNO_CHECK
	CheckExtension("GL_VERSION_2_0");
GL_ERROR_CHECK
	const GLubyte* pVersion = glGetString(GL_VERSION); 
	std::cout<<"GL_VERSION: "<<pVersion<<std::endl;
GL_ERROR_CHECK
	renderPlatform->RestoreDeviceObjects(NULL);
	depthFramebuffer.RestoreDeviceObjects(renderPlatform);
	depthFramebuffer.InitColor_Tex(0,crossplatform::RGBA_32_FLOAT);
	depthFramebuffer.SetDepthFormat(crossplatform::D_32_FLOAT);
ERRNO_CHECK
ERRNO_CHECK
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(renderPlatform);
ERRNO_CHECK
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects(renderPlatform);
ERRNO_CHECK
	if(baseOpticsRenderer)
		baseOpticsRenderer->RestoreDeviceObjects(renderPlatform);
ERRNO_CHECK
	if(baseTerrainRenderer)
		baseTerrainRenderer->RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
ERRNO_CHECK
}
void OpenGLRenderer::shutdownGL()
{
	InvalidateDeviceObjects();
}

void OpenGLRenderer::paintGL()
{
	static int viewport_id=0;
	
	const crossplatform::CameraViewStruct &cameraViewStruct=cam->GetCameraViewStruct();
	crossplatform::DeviceContext deviceContext;
	deviceContext.renderPlatform=renderPlatform;
	deviceContext.viewStruct.view_id=viewport_id;
	deviceContext.viewStruct.view	=cam->MakeViewMatrix();
	if(ReverseDepth)
		deviceContext.viewStruct.proj	=(cam->MakeDepthReversedProjectionMatrix((float)ScreenWidth/(float)ScreenHeight));
	else
		deviceContext.viewStruct.proj	=(cam->MakeProjectionMatrix((float)ScreenWidth/(float)ScreenHeight));
	
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
	if(baseTerrainRenderer)
		baseTerrainRenderer->SetReverseDepth(ReverseDepth);
	glClearColor(0,1,0,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glPushAttrib(GL_ENABLE_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(deviceContext.viewStruct.proj);
	glViewport(0,0,ScreenWidth,ScreenHeight);
	static float exposure=1.0f;
	if(simulWeatherRenderer)
	{
		GL_ERROR_CHECK
		simulWeatherRenderer->PreRenderUpdate(deviceContext);
		
		glDisable(GL_FOG);
		GL_ERROR_CHECK
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{

			simulHDRRenderer->StartRender(deviceContext);
			//simulWeatherRenderer->SetExposureHint(simulHDRRenderer->GetExposure());
		}
		else
		{
			glClearColor(0,0,0,1.f);
			glClearDepth(ReverseDepth?0.f:1.f);
			glDepthMask(GL_TRUE);
			glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		}
		depthFramebuffer.Activate(deviceContext);
		depthFramebuffer.Clear(deviceContext, 0.f, 0.f, 0.f, 0.f, 1.f, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
		if(sceneRenderer)
		{
			crossplatform::PhysicalLightRenderData physicalLightRenderData;
			physicalLightRenderData.diffuseCubemap=NULL;
			physicalLightRenderData.lightColour=simulWeatherRenderer->GetSkyKeyframer()->GetLocalIrradiance(0.0f);
			physicalLightRenderData.dirToLight=simulWeatherRenderer->GetSkyKeyframer()->GetDirectionToLight(0.0f);
			sceneRenderer->Render(deviceContext,physicalLightRenderData);
		}
		
		if(baseTerrainRenderer&&ShowTerrain)
			baseTerrainRenderer->Render(deviceContext,1.f);
		simulWeatherRenderer->RenderCelestialBackground(deviceContext,depthFramebuffer.GetDepthTexture(),exposure);
		depthFramebuffer.Deactivate(deviceContext);
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)(uintptr_t)depthFramebuffer.GetTexture()->AsGLuint());
			glUseProgram(simple_program);
			GLint image_texture		=glGetUniformLocation(simple_program,"image_texture");
			glUniform1i(image_texture,0);
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			glDisable(GL_BLEND);
			depthFramebuffer.Render(deviceContext.platform_context, false);
			glBindTexture(GL_TEXTURE_2D,(GLuint)0);
		}
		simulWeatherRenderer->RenderSkyAsOverlay(deviceContext,false,exposure,UseSkyBuffer,depthFramebuffer.GetDepthTexture()
			,depthFramebuffer.GetDepthTexture()
			,simul::sky::float4(0,0,1.f,1.f),true);
		simulWeatherRenderer->DoOcclusionTests(deviceContext);

		if(baseOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light,cam_pos;
			dir=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
			CalcCameraPosition(cam_pos);
			light=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetLocalIrradiance(cam_pos.z/1000.f);
			float occ=simulWeatherRenderer->GetBaseSkyRenderer()->GetSunOcclusion();
			float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
			baseOpticsRenderer->RenderFlare(deviceContext,exp,depthFramebuffer.GetDepthTexture(),dir,light);
		}
		if(simulHDRRenderer&&UseHdrPostprocessor)
			simulHDRRenderer->FinishRender(deviceContext,cameraViewStruct.exposure,cameraViewStruct.gamma);
//		bool vertical_screen=ScreenHeight>ScreenWidth;
GL_ERROR_CHECK
		if(ShowCompositing)
		{
			RenderDepthBuffers(deviceContext,ScreenWidth/2,0,ScreenWidth/2,ScreenHeight/2);
			simulWeatherRenderer->RenderCompositingTextures(deviceContext,ScreenWidth/2,0,ScreenWidth/2,ScreenHeight/2);
GL_ERROR_CHECK
		}
GL_ERROR_CHECK
		if(simulWeatherRenderer)
			simulWeatherRenderer->RenderOverlays(deviceContext);
GL_ERROR_CHECK
		if(ShowOSD&&simulWeatherRenderer->GetBaseCloudRenderer())
			simulWeatherRenderer->GetBaseCloudRenderer()->RenderDebugInfo(deviceContext,ScreenWidth,ScreenHeight);
	GL_ERROR_CHECK
	}
	renderUI();
	glPopAttrib();
	simul::opengl::Profiler::GetGlobalProfiler().EndFrame();
}

void OpenGLRenderer::renderUI()
{
	GL_ERROR_CHECK
	glUseProgram(0);
	glBindTexture(GL_TEXTURE_2D,0);
	SetOrthoProjection(ScreenWidth,ScreenHeight);
	static char text[500];
	int y=12;
	static int line_height=16;
	crossplatform::DeviceContext deviceContext;
	GL_ERROR_CHECK
	renderPlatform->Print(deviceContext,12,y+=line_height,"OpenGL");
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
		renderPlatform->Print(deviceContext,12,y+=line_height,osd_text);
		if(simulWeatherRenderer)
			renderPlatform->Print(deviceContext,12,y+=line_height,simulWeatherRenderer->GetDebugText());
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

void OpenGLRenderer::SetCamera(simul::crossplatform::Camera *c)
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
	if(baseTerrainRenderer)
		baseTerrainRenderer->RecompileShaders();
	SAFE_DELETE_PROGRAM(simple_program);
	simple_program=MakeProgram("simple.vert",NULL,"simple.frag");
}

void OpenGLRenderer::SaveScreenshot(const char *filename_utf8)
{
	SaveGLImage(filename_utf8,(simulHDRRenderer->framebuffer.GetTexture()->AsGLuint()));
}

void OpenGLRenderer::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy)
{
	renderPlatform->DrawDepth(deviceContext,x0,y0,dx/2,dy/2,depthFramebuffer.GetDepthTexture());
GL_ERROR_CHECK
	//MixedResolutionView *view	=viewManager.GetView(view_id);
	//view->RenderDepthBuffers(deviceContext,x0,y0,dx,dy);
	if(simulWeatherRenderer)
	{
		//simulWeatherRenderer->RenderFramebufferDepth(deviceContext,x0+w	,y0	,w,l);
		//simulWeatherRenderer->RenderCompositingTextures(deviceContext,x0,y0+2*l,dx,dy);
	}
}

void OpenGLRenderer::ReverseDepthChanged()
{
	// We do not yet support ReverseDepth on OpenGL, because GL matrices do not take advantage of this.
	ReverseDepth=false;
}