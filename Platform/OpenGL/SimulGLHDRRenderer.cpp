// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include <GL/glew.h>
#include "SimulGLHDRRenderer.h"
#include "LoadGLProgram.h"

SimulGLHDRRenderer::SimulGLHDRRenderer(int w,int h)
{
	framebuffer=new FramebufferGL(w,h,GL_TEXTURE_2D);
}

SimulGLHDRRenderer::~SimulGLHDRRenderer()
{
	delete framebuffer;
}

bool SimulGLHDRRenderer::RestoreDeviceObjects()
{
	framebuffer->InitColor_Tex(0,GL_RGBA32F_ARB);
	ERROR_CHECK
	return true;
}

bool SimulGLHDRRenderer::InvalidateDeviceObjects()
{
	return true;
}

bool SimulGLHDRRenderer::StartRender()
{
	framebuffer->Activate();
	ERROR_CHECK
	return true;
}

bool SimulGLHDRRenderer::FinishRender()
{
	framebuffer->DeactivateAndRender(false);
	ERROR_CHECK
	return true;
}
