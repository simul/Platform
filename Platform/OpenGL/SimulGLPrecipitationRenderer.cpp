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
using namespace simul;
using namespace opengl;

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

void SimulGLPrecipitationRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *)
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
GL_ERROR_CHECK
	program						=MakeProgram("simul_rain");
GL_ERROR_CHECK
	rain_texture				=LoadGLImage("Rain.jpg",GL_REPEAT);
GL_ERROR_CHECK
}

SimulGLPrecipitationRenderer::~SimulGLPrecipitationRenderer()
{
	InvalidateDeviceObjects();
}

void SimulGLPrecipitationRenderer::Render(crossplatform::DeviceContext &/*deviceContext*/,const void * /*depth_tex*/
				,float /*max_fade_distance_metres*/
				,simul::sky::float4 /*viewportTextureRegionXYWH*/)
{
	if(!baseSkyInterface)
		return;
	if(Intensity<=0)
		return;
}