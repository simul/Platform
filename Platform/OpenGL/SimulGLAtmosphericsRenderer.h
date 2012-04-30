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
#include <GL/glew.h>

SIMUL_OPENGL_EXPORT_CLASS SimulGLAtmosphericsRenderer : public simul::sky::BaseAtmosphericsRenderer
{
public:
	SimulGLAtmosphericsRenderer();
	virtual ~SimulGLAtmosphericsRenderer();
	//standard ogl object interface functions
	void RecompileShaders();
	bool RestoreDeviceObjects(void *);
	bool InvalidateDeviceObjects();
	// Interface
	void SetBufferSize(int w,int h);
	void SetMaxFadeDistanceKm(float dist_km);
	void SetYVertical(bool )
	{
	}
	// Assign the clouds framebuffer texture
	void SetCloudsTexture(void* t)
	{
		clouds_texture=(GLuint)t;
	}
	void SetLossTexture(void* t)
	{
		loss_texture=(GLuint)t;
	}
	void SetInscatterTexture(void* t)
	{
		inscatter_texture=(GLuint)t;
	}
	void SetInputTextures(void* image,void* depth)
	{
		input_texture=(GLuint)image;
		depth_texture=(GLuint)depth;
	}
	//! Render the Atmospherics.
	void StartRender();
	void FinishRender();
private:
	bool initialized;
	GLuint cloudmix_vertex_shader;
	GLuint cloudmix_fragment_shader;
	GLuint cloudmix_program;

	GLuint loss_texture,inscatter_texture;
	GLuint input_texture,depth_texture;
	GLuint clouds_texture;
	GLint clouds_texture_param;

	GLint image_texture_param;
	GLint loss_texture_param;
	GLint insc_texture_param;

	GLint hazeEccentricity_param;
	GLint lightDir_param;
	GLint invViewProj_param;
	GLint mieRayleighRatio_param;
	float max_fade_distance_metres;
	FramebufferGL *framebuffer;
};

#endif