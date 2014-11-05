// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include <GL/glew.h>
#include "Simul/Platform/OpenGL/SimulGLHDRRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include <stdint.h>  // for uintptr_t
using namespace simul;
using namespace opengl;

SimulGLHDRRenderer::SimulGLHDRRenderer(int w,int h)
	:Gamma(0.45f),Exposure(1.f)
{
}

SimulGLHDRRenderer::~SimulGLHDRRenderer()
{
}

void SimulGLHDRRenderer::SetBufferSize(int w,int h)
{
	if(w!=framebuffer.GetWidth()||h!=framebuffer.GetHeight())
	{
		HdrRenderer::SetBufferSize(w,h);
		if(renderPlatform)
			RestoreDeviceObjects(renderPlatform);
	}
}

void SimulGLHDRRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	HdrRenderer::RestoreDeviceObjects(r);
	hdrConstants.RestoreDeviceObjects(r);
ERRNO_CHECK
	framebuffer.RestoreDeviceObjects(renderPlatform);
	framebuffer.InitColor_Tex(0,crossplatform::RGBA_32_FLOAT);
	glow_fb->SetFormat(crossplatform::RGBA_32_FLOAT);
	alt_fb->SetFormat(crossplatform::RGBA_32_FLOAT);
ERRNO_CHECK
	{
		framebuffer.SetDepthFormat(crossplatform::D_32_FLOAT);
		glow_fb->SetDepthFormat(crossplatform::D_32_FLOAT);
		alt_fb->SetDepthFormat(crossplatform::D_32_FLOAT);
	}
ERRNO_CHECK
	GL_ERROR_CHECK
	RecompileShaders();
}


void SimulGLHDRRenderer::RecompileShaders()
{
	GL_ERROR_CHECK
		HdrRenderer::RecompileShaders();
GL_ERROR_CHECK
}

void SimulGLHDRRenderer::InvalidateDeviceObjects()
{
	HdrRenderer::InvalidateDeviceObjects();
}

bool SimulGLHDRRenderer::StartRender(crossplatform::DeviceContext &deviceContext)
{
	framebuffer.Activate(deviceContext);
	framebuffer.Clear(deviceContext, 1.f, 0.f, 0.f, 1.f, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	GL_ERROR_CHECK
	return true;
}

bool SimulGLHDRRenderer::FinishRender(crossplatform::DeviceContext &deviceContext,float exposure,float gamma)
{
GL_ERROR_CHECK
	framebuffer.Deactivate(deviceContext);
	RenderGlowTexture(deviceContext);
	hdr_effect->Apply(deviceContext,hdr_effect->GetTechniqueByName("tonemap"),0);
	
	hdr_effect->SetTexture(deviceContext,"image_texture",framebuffer.GetTexture());
	hdr_effect->SetTexture(deviceContext,"glowTexture",glow_fb->GetTexture());
GL_ERROR_CHECK
	hdrConstants.exposure	=exposure;//=vec4(1.0,0,1.0,0.5);
	hdrConstants.gamma		=gamma;
	hdrConstants.Apply(deviceContext);
GL_ERROR_CHECK
	deviceContext.renderPlatform->DrawQuad(deviceContext);
	hdr_effect->Unapply(deviceContext);
GL_ERROR_CHECK
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
	hdr_effect->Apply(deviceContext,hdr_effect->GetTechniqueByName("glow"),0);
	glow_fb->Activate(deviceContext);
	{
		hdr_effect->SetTexture(deviceContext,"image_texture",framebuffer.GetTexture());
		int glow_viewport[4];
		glGetIntegerv(GL_VIEWPORT,glow_viewport);
		SetOrthoProjection(glow_viewport[2],glow_viewport[3]);
		hdrConstants.offset=vec2(1.f/(float)main_viewport[2],1.f/main_viewport[3]);
		hdrConstants.exposure=Exposure;
		hdrConstants.Apply(deviceContext);
		::DrawQuad(0,0,glow_viewport[2],glow_viewport[3]);
	}
	glow_fb->Deactivate(deviceContext);
	hdr_effect->Unapply(deviceContext);
	// blur horizontally:
	hdr_effect->Apply(deviceContext,hdr_effect->GetTechniqueByName("blur"),0);
	alt_fb->Activate(deviceContext);
	{
		int glow_viewport[4];
		glGetIntegerv(GL_VIEWPORT,glow_viewport);
		SetOrthoProjection(glow_viewport[2],glow_viewport[3]);
		hdr_effect->SetTexture(deviceContext,"image_texture",glow_fb->GetTexture());
		hdrConstants.offset=vec2(1.f/(float)glow_viewport[2],0.f);
		hdrConstants.Apply(deviceContext);
		::DrawQuad(0,0,glow_viewport[2],glow_viewport[3]);
	}
	alt_fb->Deactivate(deviceContext);

	glow_fb->Activate(deviceContext);
	{
		int glow_viewport[4];
		glGetIntegerv(GL_VIEWPORT,glow_viewport);
		SetOrthoProjection(glow_viewport[2],glow_viewport[3]);
		hdr_effect->SetTexture(deviceContext,"image_texture",alt_fb->GetTexture());
		hdrConstants.offset=vec2(0.f,0.5f/(float)glow_viewport[3]);
		::DrawQuad(0,0,glow_viewport[2],glow_viewport[3]);
	}
	glow_fb->Deactivate(deviceContext);
	hdr_effect->Unapply(deviceContext);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}