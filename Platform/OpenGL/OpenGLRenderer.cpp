#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#pragma warning(disable:4505)	// Fix GLUT warnings
#include <GL/glut.h>
#endif
#include "OpenGLRenderer.h"
// For font definition define:
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
#include "Simul/Platform/CrossPlatform/HdrRenderer.h"
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

OpenGLRenderer::OpenGLRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m,bool init_glut)
		:clouds::TrueSkyRenderer(env,sc,m)
	,ShowWater(true)
	,Exposure(1.0f)
	,renderPlatformOpenGL(NULL)
{
	if(!renderPlatformOpenGL)
		renderPlatformOpenGL		=new opengl::RenderPlatform;
	renderPlatformOpenGL->SetShaderBuildMode(crossplatform::BUILD_IF_CHANGED|crossplatform::TRY_AGAIN_ON_FAIL);
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
glewExperimental= GL_TRUE;
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
	if(!view)
	{
		view_id	=viewManager.AddView(false);
		view	=viewManager.GetView(view_id);
	}
	const crossplatform::CameraOutputInterface *cam=cameras[view_id];
	if (!cam)
		return;
	static double dd=-1.0;
	static double  DD = 1.0;
	// Note: glDepthRange and glDepthRangef both CLAMP dd and DD to [0,1], rendering these functions near-useless.
	glDepthRange(dd, DD);
	// So we use the NV extension. AMD have promised to support this also:
	if (glewIsSupported("GL_NV_depth_buffer_float"))
	{
		glDepthRangedNV(dd, DD);
	}
	else
	{
		SIMUL_ASSERT_WARN_ONCE(glewIsSupported("GL_NV_depth_buffer_float"),"glDepthRangedNV is not supported.");
	}
	crossplatform::DeviceContext deviceContext;
	deviceContext.renderPlatform		=renderPlatform;
	deviceContext.viewStruct.view_id	=view_id;
	deviceContext.viewStruct.view		=cam->MakeViewMatrix();
	crossplatform::Viewport viewport	=renderPlatform->GetViewport(deviceContext,view_id);

	view->SetResolution(viewport.w,viewport.h);
	EnsureCorrectBufferSizes(view_id);
	glPushAttrib(GL_ENABLE_BIT);
	TrueSkyRenderer::Render(deviceContext);
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

void OpenGLRenderer::ReloadTextures()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->ReloadTextures();
}

void OpenGLRenderer::SaveScreenshot(const char *filename_utf8)
{
	//SaveGLImage(filename_utf8,(simulHDRRenderer->get->AsGLuint()));
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