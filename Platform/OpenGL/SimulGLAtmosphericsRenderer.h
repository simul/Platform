// Copyright (c) 2011-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#ifndef _SIMULGLATMOSPHERICSRENDERER
#define _SIMULGLATMOSPHERICSRENDERER
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/Export.h"

SIMUL_OPENGL_EXPORT_CLASS SimulGLAtmosphericsRenderer : public simul::sky::BaseAtmosphericsRenderer
{
public:
	SimulGLAtmosphericsRenderer();
	virtual ~SimulGLAtmosphericsRenderer();
	//standard ogl object interface functions
	void RecompileShaders();
	void RestoreDeviceObjects(void *);
	void InvalidateDeviceObjects();
	// Interface
	void SetBufferSize(int w,int h);
	// Assign the clouds framebuffer texture
	void SetCloudsTexture(void* t)
	{
		clouds_texture=(GLuint)t;
	}
	void SetLossTexture(void* t)
	{
		loss_texture=(GLuint)t;
	}
	void SetInscatterTextures(void* t,void *s)
	{
		inscatter_texture=(GLuint)t;
		skylight_texture=(GLuint)s;
	}
	void SetInputTextures(void* image,void* depth)
	{
		input_texture=(GLuint)image;
		depth_texture=(GLuint)depth;
	}
	void SetCloudShadowTexture(void *c)
	{
		cloud_shadow_texture=(GLuint)c;
	}
	//! Render the Atmospherics.
	void RenderAsOverlay(void *context,const void *depthTexture,float exposure);
private:
	bool initialized;
	GLuint loss_program;
	GLuint insc_program;
	GLuint earthshadow_insc_program;

	GLuint godrays_program;

	GLuint loss_texture,inscatter_texture,skylight_texture;
	GLuint input_texture,depth_texture;
	GLuint clouds_texture;
	GLuint cloud_shadow_texture;

	GLuint		earthShadowUniformsUBO;
	GLuint		atmosphericsUniformsUBO;
	GLuint		atmosphericsUniforms2UBO;

	FramebufferGL *framebuffer;
};

#endif