#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#pragma warning(disable:4505)	// Fix GLUT warnings
#endif
#include "OpenGLRenderer.h"
// For font definition define:
#include "Simul/Platform/OpenGL/LoadGLImage.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
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
#include <iomanip>

#pragma comment(lib,"opengl32")

#ifdef _DEBUG
#pragma comment(lib,"glew32sd")
#else
#pragma comment(lib,"glew32s")
#endif

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace simul;
using namespace opengl;

using namespace simul;
using namespace opengl;
using namespace std;
static bool glut_initialized=false;

OpenGLRenderer::OpenGLRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m,bool )
		:trueSkyRenderer(env,sc,m,true)
	,ShowWater(true)
	,Exposure(1.0f)
	,renderPlatformOpenGL(NULL)
{
	if(!renderPlatformOpenGL)
		renderPlatformOpenGL		=new opengl::RenderPlatform;
	renderPlatformOpenGL->SetShaderBuildMode(crossplatform::BUILD_IF_CHANGED|crossplatform::TRY_AGAIN_ON_FAIL|crossplatform::BREAK_ON_FAIL);
	simul::opengl::Profiler::GetGlobalProfiler().Initialize(NULL);
	//sceneCache=new scene::BaseObjectRenderer(gScene,&renderPlatform);
	/*if(init_glut&&!glut_initialized)
	{
		char argv[]="no program";
		char *a=argv;
		int argc = 1;
		glutInitContextVersion(4, 2);
		glutInitContextProfile(GLUT_CORE_PROFILE);
		glutInitContextFlags(GLUT_DEBUG | GLUT_FORWARD_COMPATIBLE);
	    glutInit(&argc,&a);
		glut_initialized=true;
	//	GLint n;
	//	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
//		for (int i = 0; i < n; i++)
		{
		//	const unsigned char *s=glGetStringi(GL_EXTENSIONS,i);
		//	std::cout<<s<<std::endl;
		}

	}*/
}

void OpenGLRenderer::InvalidateDeviceObjects()
{
	trueSkyRenderer.InvalidateDeviceObjects();
	int err=errno;
	std::cout<<"Errno "<<err<<std::endl;
	errno=0;
ERRNO_CHECK
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
	return trueSkyRenderer.AddView(false);
}
void OpenGLRenderer::InitializeGL()
{
	RestoreDeviceObjects(renderPlatformOpenGL);
}

void OpenGLRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	GL_ERROR_CHECK
	r->RestoreDeviceObjects(NULL);
	trueSkyRenderer.RestoreDeviceObjects(r);
	GL_ERROR_CHECK
}

void OpenGLRenderer::ShutdownGL()
{
	InvalidateDeviceObjects();
}

void OpenGLRenderer::RenderGL(int view_id)
{
	crossplatform::MixedResolutionView *view	=trueSkyRenderer.viewManager.GetView(view_id);
	if(!view)
	{
		view_id	=trueSkyRenderer.viewManager.AddView(false);
		view	=trueSkyRenderer.viewManager.GetView(view_id);
	}
	GL_ERROR_CHECK
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
		SIMUL_ASSERT_WARN_ONCE(GL_TRUE==glewIsSupported("GL_NV_depth_buffer_float"),"glDepthRangedNV is not supported. This means that depth-reverse will not work for OpenGL with the current drivers.");
	}

	crossplatform::DeviceContext deviceContext;
	deviceContext.renderPlatform		=renderPlatformOpenGL;
	deviceContext.viewStruct.view_id	=view_id;
	GL_ERROR_CHECK
	crossplatform::Viewport viewport	=renderPlatformOpenGL->GetViewport(deviceContext,view_id);

	view->SetResolution(viewport.w,viewport.h);
//	trueSkyRenderer.EnsureCorrectBufferSizes(view_id);
	simul::crossplatform::SetGpuProfilingInterface(deviceContext,&simul::opengl::Profiler::GetGlobalProfiler());
	
	simul::opengl::Profiler::GetGlobalProfiler().StartFrame(deviceContext);
	trueSkyRenderer.Render(deviceContext);
	
	simul::opengl::Profiler::GetGlobalProfiler().EndFrame(deviceContext);
}

void OpenGLRenderer::ResizeGL(int view_id,int w,int h)
{
	GL_ERROR_CHECK
	trueSkyRenderer.ResizeView(view_id,w,h);
	GL_ERROR_CHECK
	crossplatform::DeviceContext deviceContext;
	deviceContext.renderPlatform	=renderPlatformOpenGL;
	deviceContext.viewStruct.view_id=view_id;
	//depthFramebuffer.SetWidthAndHeight(w,h);
	crossplatform::Viewport viewport;
	memset(&viewport,0,sizeof(viewport));
	viewport.w=w;
	viewport.h=h;
	GL_ERROR_CHECK
	renderPlatformOpenGL->SetViewports(deviceContext,1,&viewport);
	GL_ERROR_CHECK
}

void OpenGLRenderer::ReloadTextures()
{
	trueSkyRenderer.ReloadTextures();
}

void OpenGLRenderer::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy)
{
	crossplatform::Texture *depthTexture=NULL;
	crossplatform::MixedResolutionView *view	=trueSkyRenderer.viewManager.GetView(deviceContext.viewStruct.view_id);
		depthTexture	=view->GetFramebuffer()->GetDepthTexture();
	renderPlatformOpenGL->DrawDepth(deviceContext,x0,y0,dx/2,dy/2,depthTexture);
}