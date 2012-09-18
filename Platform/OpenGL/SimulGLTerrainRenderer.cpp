#include "Simul/Platform/OpenGL/SimulGLTerrainRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimuLGLUtilities.h"
#include "Simul/Math/Vector3.h"

SimulGLTerrainRenderer::SimulGLTerrainRenderer()
	:program(0)
	,max_fade_distance_metres(200000.f)
{
}

SimulGLTerrainRenderer::~SimulGLTerrainRenderer()
{
}

void SimulGLTerrainRenderer::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(program);
	program			=MakeProgram("simul_terrain");
	ERROR_CHECK
	glUseProgram(program);
	ERROR_CHECK
	printProgramInfoLog(program);
	ERROR_CHECK
	eyePosition_param				= glGetUniformLocation(program,"eyePosition");
	maxFadeDistanceMetres_param		= glGetUniformLocation(program,"maxFadeDistanceMetres");
	printProgramInfoLog(program);
	ERROR_CHECK
}

void SimulGLTerrainRenderer::RestoreDeviceObjects(void*)
{
	RecompileShaders();
}

void SimulGLTerrainRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(program);
}

void SimulGLTerrainRenderer::SetMaxFadeDistanceKm(float dist_km)
{
	max_fade_distance_metres=dist_km*1000.f;
}

void SimulGLTerrainRenderer::Render()
{
	ERROR_CHECK
	glUseProgram(program);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	simul::math::Vector3 cam_pos;
	CalcCameraPosition(cam_pos);

	glUniform3f(eyePosition_param,cam_pos.x,cam_pos.y,cam_pos.z);
	glUniform1f(maxFadeDistanceMetres_param,max_fade_distance_metres);
ERROR_CHECK
	glBegin(GL_QUAD_STRIP);
	for(int i=0;i<100;i++)
	{
		float x1=i*100.f;
		float x2=(i+1)*1000.f;
		float z=0.f;
		for(int j=0;j<100;j++)
		{
			float y=j*1000.f;
			simul::math::Vector3 X1(x1,y,z);
			simul::math::Vector3 X2(x2,y,z);
			glVertex3f(X1.x,X1.y,X1.z);
			glVertex3f(X2.x,X2.y,X2.z);
		}
	}
	glEnd();
ERROR_CHECK
	glPopAttrib();
ERROR_CHECK
	glUseProgram(NULL);
ERROR_CHECK
}