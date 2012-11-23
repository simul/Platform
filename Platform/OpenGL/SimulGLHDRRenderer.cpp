// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include <GL/glew.h>
#include "SimulGLHDRRenderer.h"
#include "SimulGLUtilities.h"
#include "LoadGLProgram.h"

SimulGLHDRRenderer::SimulGLHDRRenderer(int w,int h)
	:Gamma(0.45f),Exposure(1.f)
	,initialized(false)
	,tonemap_program(0)
{
	framebuffer=new FramebufferGL(w,h,GL_TEXTURE_2D,"tonemap");
}

SimulGLHDRRenderer::~SimulGLHDRRenderer()
{
	delete framebuffer;
}

void SimulGLHDRRenderer::SetBufferSize(int w,int h)
{
	if(w!=framebuffer->GetWidth()||h!=framebuffer->GetHeight())
	{
		framebuffer->SetWidthAndHeight(w,h);
		if(initialized)
			RestoreDeviceObjects();
	}
}

void SimulGLHDRRenderer::RestoreDeviceObjects()
{
	initialized=true;
	framebuffer->InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	if(glewIsSupported("GL_EXT_packed_depth_stencil")||IsExtensionSupported("GL_EXT_packed_depth_stencil"))
	{
		framebuffer->InitDepth_RB(GL_DEPTH24_STENCIL8_EXT);
	}
	else
	{
		framebuffer->InitDepth_RB(GL_DEPTH_COMPONENT32);
	}
	ERROR_CHECK
	RecompileShaders();
}


void SimulGLHDRRenderer::RecompileShaders()
{
	tonemap_program		=LoadPrograms("simple.vert",NULL,"tonemap.frag");
    exposure_param		=glGetUniformLocation(tonemap_program,"exposure");
    gamma_param			=glGetUniformLocation(tonemap_program,"gamma");
    buffer_tex_param	=glGetUniformLocation(tonemap_program,"image_texture");
	ERROR_CHECK
}

void SimulGLHDRRenderer::InvalidateDeviceObjects()
{
}

bool SimulGLHDRRenderer::StartRender()
{
	framebuffer->Activate();
	glClearColor(0.f,0.f,0.f,1.f);
	ERROR_CHECK
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	ERROR_CHECK
	return true;
}

bool SimulGLHDRRenderer::FinishRender()
{
	framebuffer->Deactivate();
	
	glUseProgram(tonemap_program);

	glUniform1f(exposure_param,Exposure);
	glUniform1f(gamma_param,Gamma);
	glUniform1i(buffer_tex_param,0);
	
	framebuffer->Render(false);
	ERROR_CHECK
	glUseProgram(0);
	return true;
}
