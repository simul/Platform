#include "OpenGLRenderer.h"
#include <GL/glew.h>
// For font definition define:
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/SimulGLSkyRenderer.h"
#include "Simul/Sky/Float4.h"
#define GLUT_BITMAP_HELVETICA_12	((void*)7)


OpenGLRenderer::OpenGLRenderer(const char *license_key)
	:width(0)
	,height(0)
	,cam(NULL)
	,y_vertical(false)
	,ShowFlares(true)
	,ShowFades(false)
{
	simul::opengl::SetTexturePath("Media/Textures");
	simul::opengl::SetShaderPath("Media/GLSL/");		// path relative to the root
	simulWeatherRenderer=new SimulGLWeatherRenderer(license_key);
	simulOpticsRenderer=new SimulOpticsRendererGL();
	SetYVertical(y_vertical);
}

OpenGLRenderer::~OpenGLRenderer()
{
}

void OpenGLRenderer::paintGL()
{
	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glPushAttrib(GL_ENABLE_BIT);
	// If called from some other OpenGL program, we should already have a modelview and projection matrix.
	// Here we will generate the modelview matrix from the camera class:
    glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(cam->MakeViewMatrix(false));
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(cam->MakeProjectionMatrix(1.f,250000.f,(float)width/(float)height,y_vertical));
	glViewport(0,0,width,height);
	if(simulWeatherRenderer.get())
	{
		simulWeatherRenderer->Update(0.0);
		GLuint fogMode[]={GL_EXP,GL_EXP2,GL_LINEAR};	// Storage For Three Types Of Fog
		GLuint fogfilter=0;								// Which Fog To Use
		simul::sky::float4 fogColor=simulWeatherRenderer->GetHorizonColour(0.001f*cam->GetPosition()[2]);
		glFogi(GL_FOG_MODE,fogMode[fogfilter]);			// Fog Mode
		glFogfv(GL_FOG_COLOR,fogColor);					// Set Fog Color
		glFogf(GL_FOG_DENSITY,0.35f);					// How Dense Will The Fog Be
		glFogf(GL_FOG_START,1.0f);						// Fog Start Depth
		glFogf(GL_FOG_END,5.0f);						// Fog End Depth
		glEnable(GL_FOG);
		simulHDRRenderer->StartRender();
		simulWeatherRenderer->RenderSky(true,false);
		//simulWeatherRenderer->RenderClouds(false,false,false);

		if(ShowFades&&simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
			simulWeatherRenderer->GetSkyRenderer()->RenderFades();
		simulWeatherRenderer->DoOcclusionTests();
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light;
			dir=simulWeatherRenderer->GetSkyRenderer()->GetDirectionToLight();
			light=simulWeatherRenderer->GetSkyRenderer()->GetLightColour();
//		simulOpticsRenderer->SetMatrices(view,proj);
			simulOpticsRenderer->RenderFlare(
				simulHDRRenderer->GetExposure()*(1.f-simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion())
				,dir,light);
		}
		//if(GetShowFlares())
		//	simulWeatherRenderer->RenderFlares(simulHDRRenderer->GetExposure());
		simulHDRRenderer->FinishRender();
	}
	glPopAttrib();
}

void OpenGLRenderer::renderUI()
{
	glUseProgram(NULL);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_1D);
	SetOrthoProjection(width,height);
	static char text[500];
	float y=12.f;
	static int line_height=16;
	RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,"OpenGL");
}

void OpenGLRenderer::resizeGL(int w,int h)
{
	width=w;
	height=h;
	if(!simulHDRRenderer.get())
		simulHDRRenderer=new SimulGLHDRRenderer(width,height);
	else
		simulHDRRenderer->SetBufferSize(width,height);
}

void OpenGLRenderer::initializeGL()
{
	//if(!cam)
	//cam=new simul::camera::Camera();
	if(cam)
		cam->LookInDirection(simul::math::Vector3(1.f,0,0),simul::math::Vector3(0,0,1.f));

	simulWeatherRenderer->RestoreDeviceObjects();
	simulHDRRenderer->RestoreDeviceObjects();
	if(simulOpticsRenderer)
	{
		simulOpticsRenderer->RestoreDeviceObjects(NULL);
	}
}

void OpenGLRenderer::SetCamera(simul::camera::Camera *c)
{
	cam=c;
}

void OpenGLRenderer::SetYVertical(bool y)
{
	y_vertical=y;
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->SetYVertical(y);
	//if(simulTerrainRenderer.get())
	//	simulTerrainRenderer->SetYVertical(y_vertical);
	if(simulOpticsRenderer)
		simulOpticsRenderer->SetYVertical(y_vertical);
}

void OpenGLRenderer::ReloadShaders()
{
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->ReloadShaders();
}