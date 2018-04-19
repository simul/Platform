#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#pragma warning(disable:4505)	// Fix GLUT warnings
#endif
#include "OpenGLRenderer.h"

#include "Simul/Platform/CrossPlatform/Camera.h"
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

}

void OpenGLRenderer::InvalidateDeviceObjects()
{
	trueSkyRenderer.InvalidateDeviceObjects();
	int err=errno;
	std::cout<<"Errno "<<err<<std::endl;
	errno=0;
ERRNO_CHECK
	simul::opengl::Profiler::GetGlobalProfiler().Uninitialize();
}

OpenGLRenderer::~OpenGLRenderer()
{
	InvalidateDeviceObjects();
	delete renderPlatformOpenGL;
}

int OpenGLRenderer::AddGLView() 
{
	static int v=0;
	trueSkyRenderer.AddView(++v);
	return v;
}
void OpenGLRenderer::InitializeGL()
{
}

void OpenGLRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
}

void OpenGLRenderer::ShutdownGL()
{
	InvalidateDeviceObjects();
}

void OpenGLRenderer::RenderGL(int view_id)
{
}

void OpenGLRenderer::ResizeGL(int view_id,int w,int h)
{
}

void OpenGLRenderer::ReloadTextures()
{
}

void OpenGLRenderer::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy)
{
}