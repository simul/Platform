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
simul::opengl::RenderPlatform *renderPlatformOpenGL=NULL;

OpenGLRenderer::OpenGLRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m,bool init_glut)
		:clouds::TrueSkyRenderer(env,sc,m)
	,ShowWater(true)
	,Exposure(1.0f)
	,simple_program(0)
{
	simulHDRRenderer		=new SimulGLHDRRenderer(1,1);
	//simulWeatherRenderer	=new SimulGLWeatherRenderer(env,NULL,1,1);
	//baseOpticsRenderer		=new simul::crossplatform::BaseOpticsRenderer(m);
	//baseTerrainRenderer		=new simul::terrain::BaseTerrainRenderer(NULL);
	//baseTerrainRenderer->SetBaseSkyInterface(simulWeatherRenderer->GetSkyKeyframer());
	if(!renderPlatformOpenGL)
		renderPlatformOpenGL		=new opengl::RenderPlatform;
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
	clouds::TrueSkyRenderer::InvalidateDeviceObjects();
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
//	depthFramebuffer.InvalidateDeviceObjects();
GL_ERROR_CHECK
}

OpenGLRenderer::~OpenGLRenderer()
{
GL_ERROR_CHECK
	InvalidateDeviceObjects();
GL_ERROR_CHECK
	delete renderPlatformOpenGL;
}

int OpenGLRenderer::AddGLView() 
{
	return TrueSkyRenderer::AddView(false);
}
void OpenGLRenderer::InitializeGL()
{
	RestoreDeviceObjects(renderPlatformOpenGL);
}

void OpenGLRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
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
	clouds::TrueSkyRenderer::RestoreDeviceObjects(r);
	renderPlatform->RestoreDeviceObjects(NULL);
	//depthFramebuffer.RestoreDeviceObjects(renderPlatform);
	//depthFramebuffer.InitColor_Tex(0,crossplatform::RGBA_32_FLOAT);
	//depthFramebuffer.SetDepthFormat(crossplatform::D_32_FLOAT);
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
	if(sceneRenderer)
		sceneRenderer->RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
ERRNO_CHECK
}

void OpenGLRenderer::ShutdownGL()
{
	InvalidateDeviceObjects();
}

void OpenGLRenderer::RenderGL(int view_id)
{
	crossplatform::MixedResolutionView *view	=viewManager.GetView(view_id);
	const crossplatform::CameraOutputInterface *cam=cameras[view_id];
	if (!cam)
		return;
	const crossplatform::CameraViewStruct &cameraViewStruct=cam->GetCameraViewStruct();
	crossplatform::DeviceContext deviceContext;
	deviceContext.renderPlatform	=renderPlatform;
	deviceContext.viewStruct.view_id=view_id;
	deviceContext.viewStruct.view	=cam->MakeViewMatrix();
	crossplatform::Viewport 		viewport=renderPlatform->GetViewport(deviceContext,view_id);

	view->SetResolution(viewport.w,viewport.h);
	EnsureCorrectBufferSizes(view_id);
	TrueSkyRenderer::Render(deviceContext);
	return;
	if(ReverseDepth)
		deviceContext.viewStruct.proj	=(cam->MakeDepthReversedProjectionMatrix((float)viewport.w/(float)viewport.h));
	else
		deviceContext.viewStruct.proj	=(cam->MakeProjectionMatrix((float)viewport.w/(float)viewport.h));
	
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
	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glPushAttrib(GL_ENABLE_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(deviceContext.viewStruct.proj);
	glViewport(0,0,viewport.w,viewport.h);
	static float exposure=1.0f;
	float real_time=0;
	if(simulWeatherRenderer)
	{
		GL_ERROR_CHECK
		simulWeatherRenderer->PreRenderUpdate(deviceContext,real_time);
		glDisable(GL_FOG);
		GL_ERROR_CHECK
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			//simulHDRRenderer->StartRender(deviceContext);
			//simulWeatherRenderer->SetExposureHint(simulHDRRenderer->GetExposure());
		}
		else
		{
			glClearColor(0,0,0,1.f);
			glClearDepth(ReverseDepth?0.f:1.f);
			glDepthMask(GL_TRUE);
			glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		}
		//depthFramebuffer.Activate(deviceContext);
		view->GetFramebuffer()->Activate(deviceContext);
		view->GetFramebuffer()->Clear(deviceContext, 0.0f, 0.f, 0.f, 0.f, 1.f, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
#if 1
		if(baseTerrainRenderer&&ShowTerrain)
			baseTerrainRenderer->Render(deviceContext,1.f);
		if(sceneRenderer)
		{
			crossplatform::PhysicalLightRenderData physicalLightRenderData;
			physicalLightRenderData.diffuseCubemap=NULL;
			physicalLightRenderData.lightColour	=simulWeatherRenderer->GetSkyKeyframer()->GetLocalIrradiance(0.0f);
			physicalLightRenderData.dirToLight	=simulWeatherRenderer->GetSkyKeyframer()->GetDirectionToLight(0.0f);
			sceneRenderer->Render(deviceContext,physicalLightRenderData);
		}
		view->GetFramebuffer()->DeactivateDepth(deviceContext);
		crossplatform::Texture *depthTexture=NULL;
		depthTexture	=view->GetFramebuffer()->GetDepthTexture();
		simulWeatherRenderer->RenderCelestialBackground(deviceContext,depthTexture,simul::sky::float4(0, 0, 1.f, 1.f),exposure);
		
	/*	{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)(uintptr_t)depthTexture->AsGLuint());
			glUseProgram(simple_program);
			GLint image_texture		=glGetUniformLocation(simple_program,"image_texture");
			glUniform1i(image_texture,0);
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			glDisable(GL_BLEND);
			depthFramebuffer.Render(deviceContext.platform_context, false);
			glBindTexture(GL_TEXTURE_2D,(GLuint)0);
		}*/
		if(simulWeatherRenderer)
		{
			crossplatform::Viewport viewport={0,0,depthTexture->width,depthTexture->length,0.f,1.f};
	/*		viewManager.DownscaleDepth(deviceContext
										,depthTexture
										,NULL
										,simulWeatherRenderer->GetAtmosphericDownscale()
										,simulWeatherRenderer->GetDownscale()
										,simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetMaxDistanceKm()*1000.0f
										,trueSkyRenderMode==clouds::MIXED_RESOLUTION);*/
		}
		simulWeatherRenderer->RenderSkyAsOverlay(deviceContext,false,exposure,1.0f,UseSkyBuffer
			,depthTexture
			,simul::sky::float4(0,0,1.f,1.f),true,vec2(0,0));
		simulWeatherRenderer->DoOcclusionTests(deviceContext);

		if(baseOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light,cam_pos;
			dir=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
			CalcCameraPosition(cam_pos);
			light=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetLocalIrradiance(cam_pos.z/1000.f);
			float occ=simulWeatherRenderer->GetBaseSkyRenderer()->GetSunOcclusion();
			float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
			baseOpticsRenderer->RenderFlare(deviceContext,exp,depthTexture,dir,light);
		}
#endif
		view->GetFramebuffer()->Deactivate(deviceContext);
		if(simulHDRRenderer&&UseHdrPostprocessor)
		//	simulHDRRenderer->FinishRender(deviceContext,cameraViewStruct.exposure,cameraViewStruct.gamma);
			simulHDRRenderer->Render(deviceContext,view->GetResolvedHDRBuffer(),cameraViewStruct.exposure,cameraViewStruct.gamma);
	
//		bool vertical_screen=ScreenHeight>ScreenWidth;
GL_ERROR_CHECK
		if(simulWeatherRenderer->GetShowCompositing())
		{
			RenderDepthBuffers(deviceContext,viewport.w/2,0,viewport.w/2,viewport.h/2);
			simulWeatherRenderer->RenderCompositingTextures(deviceContext,viewport.w/2,viewport.h/2,viewport.w/2,viewport.h/2);
GL_ERROR_CHECK
		}
GL_ERROR_CHECK
		RenderOverlays(deviceContext,cameraViewStruct);
GL_ERROR_CHECK
		if(ShowOSD&&simulWeatherRenderer->GetBaseCloudRenderer())
			simulWeatherRenderer->GetBaseCloudRenderer()->RenderDebugInfo(deviceContext,viewport.w,viewport.h);
	GL_ERROR_CHECK
	}
	glPopAttrib();
	simul::opengl::Profiler::GetGlobalProfiler().EndFrame();
}

void OpenGLRenderer::ResizeGL(int view_id,int w,int h)
{
	TrueSkyRenderer::ResizeView(view_id,w,h);
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(w,h);
	crossplatform::DeviceContext deviceContext;
	deviceContext.renderPlatform	=renderPlatform;
	deviceContext.viewStruct.view_id=view_id;
	//depthFramebuffer.SetWidthAndHeight(w,h);
	crossplatform::Viewport viewport;
	memset(&viewport,0,sizeof(viewport));
	viewport.w=w;
	viewport.h=h;
	renderPlatformOpenGL->SetViewports(deviceContext,1,&viewport);
}

void OpenGLRenderer::SetCamera(int view_id,const simul::crossplatform::CameraOutputInterface *c)
{
	cameras[view_id]=c;
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
	crossplatform::Texture *depthTexture=NULL;
	crossplatform::MixedResolutionView *view	=viewManager.GetView(deviceContext.viewStruct.view_id);
		depthTexture	=view->GetFramebuffer()->GetDepthTexture();
	renderPlatform->DrawDepth(deviceContext,x0,y0,dx/2,dy/2,depthTexture);
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