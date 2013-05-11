// Copyright (c) 2011-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.


#include <GL/glew.h>
#include "SimulGLPrecipitationRenderer.h"

#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Base/SmartPtr.h"
#include "Simul/Math/Pi.h"
#include "Simul/Sky/Float4.h"

SimulGLPrecipitationRenderer::SimulGLPrecipitationRenderer() :
	external_rain_texture(false)
		,program(0)
		,rain_texture(0)
{
}

void SimulGLPrecipitationRenderer::TextureRepeatChanged()
{
	MakeMesh();
}

void SimulGLPrecipitationRenderer::RestoreDeviceObjects(void *)
{
	MakeMesh();
	RecompileShaders();
}

bool SimulGLPrecipitationRenderer::SetExternalRainTexture(void* )
{
	return true;
}

void SimulGLPrecipitationRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(program);
	SAFE_DELETE_TEXTURE(rain_texture);
}

void SimulGLPrecipitationRenderer::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(program);
	SAFE_DELETE_TEXTURE(rain_texture);
	program						=MakeProgram("simul_rain");
	rain_texture				=LoadGLImage("Rain.jpg",GL_REPEAT);
}

SimulGLPrecipitationRenderer::~SimulGLPrecipitationRenderer()
{
	InvalidateDeviceObjects();
}

void SimulGLPrecipitationRenderer::Render(void*)
{
	if(!baseSkyInterface)
		return;
	if(rain_intensity<=0)
		return;
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	simul::sky::float4 cam_dir;
	CalcCameraPosition(cam_pos,cam_dir);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
		ERROR_CHECK
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
ERROR_CHECK
	glTranslatef(cam_pos[0],cam_pos[1],cam_pos[2]);
ERROR_CHECK
glUseProgram(program);
	setTexture(program,"rainTexture",0,rain_texture);
	setParameter(program,"intensity",rain_intensity);
	setParameter3(program,"lightColour",light_colour);
	setParameter3(program,"lightDir",light_dir);
	setParameter(program,"offset",offs);
	//float radius,height,offs,rain_intensity,wind_heading,wind_speed;
ERROR_CHECK
	glBegin(GL_TRIANGLE_STRIP);
	for(int i=0;i<ELEV;i++)
	{
		for(int j=0;j<CONE_SIDES+1;j++)
		{
			Vertex_t &v1=vertices[i*(CONE_SIDES+1)+j];
			Vertex_t &v2=vertices[(i+1)*(CONE_SIDES+1)+j];
			glMultiTexCoord3f(GL_TEXTURE0,v1.tex_x,v1.tex_y,v1.fade);
			glVertex3f(v1.x,v1.y,v1.z);
			glMultiTexCoord3f(GL_TEXTURE0,v2.tex_x,v2.tex_y,v2.fade);
			glVertex3f(v2.x,v2.y,v2.z);
		}
	}
	glEnd();
		ERROR_CHECK
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
		ERROR_CHECK
	glPopAttrib();
}