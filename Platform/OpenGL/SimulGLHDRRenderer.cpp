// Copyright (c) 2007-2014 Simul Software Ltd
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
#include <stdint.h>  // for uintptr_t
using namespace simul;
using namespace opengl;

SimulGLHDRRenderer::SimulGLHDRRenderer(int w,int h)
	:Gamma(0.45f),Exposure(1.f)
	,initialized(false)
	,tonemap_program(0)
	,glow_fb(w/2,h/2,GL_TEXTURE_2D)
	,alt_fb(w/2,h/2,GL_TEXTURE_2D)
{
}

SimulGLHDRRenderer::~SimulGLHDRRenderer()
{
}

void SimulGLHDRRenderer::SetBufferSize(int w,int h)
{
	if(w!=framebuffer.GetWidth()||h!=framebuffer.GetHeight())
	{
		framebuffer.SetWidthAndHeight(w,h);
		glow_fb.SetWidthAndHeight(w/2,h/2);
		alt_fb.SetWidthAndHeight(w/2,h/2);
		if(initialized)
			RestoreDeviceObjects();
	}
}

void SimulGLHDRRenderer::RestoreDeviceObjects()
{
ERRNO_CHECK
	initialized=true;
	framebuffer.InitColor_Tex(0,GL_RGBA32F_ARB);
	glow_fb.InitColor_Tex(0,GL_RGBA32F_ARB);
	alt_fb.InitColor_Tex(0,GL_RGBA32F_ARB);
ERRNO_CHECK
	{
		framebuffer.InitDepth_RB(GL_DEPTH_COMPONENT32);
		glow_fb.InitDepth_RB(GL_DEPTH_COMPONENT32);
		alt_fb.InitDepth_RB(GL_DEPTH_COMPONENT32);
	}
ERRNO_CHECK
	GL_ERROR_CHECK
	RecompileShaders();
}


void SimulGLHDRRenderer::RecompileShaders()
{
ERRNO_CHECK
	tonemap_program		=MakeProgram("simple.vert",NULL,"tonemap.frag");
    exposure_param		=glGetUniformLocation(tonemap_program,"exposure");
    gamma_param			=glGetUniformLocation(tonemap_program,"gamma");
    buffer_tex_param	=glGetUniformLocation(tonemap_program,"image_texture");
	GL_ERROR_CHECK
ERRNO_CHECK

	glow_program		=MakeProgram("simple.vert",NULL,"simul_glow.frag");
	blur_program		=MakeProgram("simple.vert",NULL,"simul_hdr_blur.frag");
ERRNO_CHECK
}

void SimulGLHDRRenderer::InvalidateDeviceObjects()
{
}
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
bool SimulGLHDRRenderer::StartRender(crossplatform::DeviceContext &deviceContext)
{
	framebuffer.Activate(deviceContext);
	framebuffer.Clear(deviceContext.platform_context,0.f,0.f,0.f,1.f,GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	GL_ERROR_CHECK
	return true;
}

bool SimulGLHDRRenderer::FinishRender(crossplatform::DeviceContext &deviceContext)
{
	framebuffer.Deactivate(deviceContext.platform_context);
	RenderGlowTexture(deviceContext);

	glUseProgram(tonemap_program);
	setTexture(tonemap_program,"image_texture",0,(GLuint)framebuffer.GetColorTex());
	GL_ERROR_CHECK
	glUniform1f(exposure_param,Exposure);
	glUniform1f(gamma_param,Gamma);
	glUniform1i(buffer_tex_param,0);
	setTexture(tonemap_program,"glowTexture",1,(GLuint)glow_fb.GetColorTex());

	framebuffer.Render(deviceContext.platform_context,false);
	GL_ERROR_CHECK
	glUseProgram(0);
	return true;
}

void SimulGLHDRRenderer::RenderGlowTexture(crossplatform::DeviceContext &deviceContext)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
	int main_viewport[4];
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	// Render to the low-res glow.
	glDisable(GL_BLEND);
	glUseProgram(glow_program);
	glow_fb.Activate(deviceContext);
	{
		setTexture(glow_program,"image_texture",0,(GLuint)framebuffer.GetColorTex());
		int glow_viewport[4];
		glGetIntegerv(GL_VIEWPORT,glow_viewport);
		SetOrthoProjection(glow_viewport[2],glow_viewport[3]);
		setParameter(glow_program,"offset",1.f/(float)main_viewport[2],1.f/main_viewport[3]);
		setParameter(glow_program,"exposure",Exposure);
		::DrawQuad(0,0,glow_viewport[2],glow_viewport[3]);
	}
	glow_fb.Deactivate(deviceContext.platform_context);

	// blur horizontally:
	glUseProgram(blur_program);
	alt_fb.Activate(deviceContext);
	{
		int glow_viewport[4];
		glGetIntegerv(GL_VIEWPORT,glow_viewport);
		SetOrthoProjection(glow_viewport[2],glow_viewport[3]);
		setTexture(blur_program,"image_texture",0,(GLuint)glow_fb.GetColorTex());
		setParameter(blur_program,"offset",1.f/(float)glow_viewport[2],0.f);
		::DrawQuad(0,0,glow_viewport[2],glow_viewport[3]);
	}
	alt_fb.Deactivate(deviceContext.platform_context);

	glow_fb.Activate(deviceContext);
	{
		int glow_viewport[4];
		glGetIntegerv(GL_VIEWPORT,glow_viewport);
		SetOrthoProjection(glow_viewport[2],glow_viewport[3]);
		setTexture(blur_program,"image_texture",0,(GLuint)alt_fb.GetColorTex());
		setParameter(blur_program,"offset",0.f,0.5f/(float)glow_viewport[3]);
		::DrawQuad(0,0,glow_viewport[2],glow_viewport[3]);
	}
	glow_fb.Deactivate(deviceContext.platform_context);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}