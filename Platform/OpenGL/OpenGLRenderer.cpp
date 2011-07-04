#include "OpenGLRenderer.h"
#include <GL/glew.h>
// For font definition define:
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Graph/Camera/Camera.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Sky/Float4.h"
#define GLUT_BITMAP_HELVETICA_12	((void*)7)
simul::graph::camera::Camera *cam=NULL;


OpenGLRenderer::OpenGLRenderer(const char *license_key):width(0),height(0)
{
	simul::opengl::SetShaderPath("Media/GLSL/");		// path relative to the root
	simulWeatherRenderer=new SimulGLWeatherRenderer(license_key);
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
	float field_of_view=cam->GetVerticalFieldOfViewDegrees();
	SetPerspectiveProjection(width,height,field_of_view);
	if(simulWeatherRenderer.get())
	{
		simulWeatherRenderer->Update(0.0);
		GLuint fogMode[]={GL_EXP,GL_EXP2,GL_LINEAR};	// Storage For Three Types Of Fog
		GLuint fogfilter=0;								// Which Fog To Use
		simul::sky::float4 fogColor=simulWeatherRenderer->GetHorizonColour(cam->GetPosition()[2]);
		glFogi(GL_FOG_MODE,fogMode[fogfilter]);			// Fog Mode
		glFogfv(GL_FOG_COLOR,fogColor);					// Set Fog Color
		glFogf(GL_FOG_DENSITY,0.35f);					// How Dense Will The Fog Be
		glFogf(GL_FOG_START,1.0f);						// Fog Start Depth
		glFogf(GL_FOG_END,5.0f);						// Fog End Depth
		glEnable(GL_FOG);
		simulHDRRenderer->StartRender();
		simulWeatherRenderer->RenderSky(false,false);
		simulWeatherRenderer->RenderClouds(false,false,false);
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
	cam=new simul::graph::camera::Camera();
	cam->LookInDirection(simul::math::Vector3(1.f,0,0),simul::math::Vector3(0,0,1.f));

	simulWeatherRenderer->RestoreDeviceObjects();
	simulHDRRenderer->RestoreDeviceObjects();
}

void OpenGLRenderer::SetCamera(simul::graph::camera::Camera *c)
{
	cam=c;
}

