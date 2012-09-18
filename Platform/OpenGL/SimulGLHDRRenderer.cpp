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
{
	framebuffer=new FramebufferGL(w,h,GL_TEXTURE_2D,"tonemap");
}

SimulGLHDRRenderer::~SimulGLHDRRenderer()
{
	delete framebuffer;
}

void SimulGLHDRRenderer::RecompileShaders()
{
	framebuffer->RecompileShaders();
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
	framebuffer->SetGamma(Gamma);
	framebuffer->SetExposure(Exposure);
	framebuffer->DeactivateAndRender(false);
	ERROR_CHECK
	return true;
}
