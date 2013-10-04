// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once

#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/GpuSkyGenerator.h"
#include <cstdlib>
namespace simul
{
	namespace sky
	{
		class SiderealSkyInterface;
		class AtmosphericScatteringInterface;
		class Sky;
		class FadeTableInterface;
		class SkyKeyframer;
		class OvercastCallback;
	}
}

//! A sky rendering class for OpenGL.
SIMUL_OPENGL_EXPORT_CLASS SimulGLSkyRenderer : public simul::sky::BaseSkyRenderer
{
public:
	SimulGLSkyRenderer(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *m);
	virtual ~SimulGLSkyRenderer();
	//standard ogl object interface functions

	//! Create the API-specific objects to be used in rendering. This is usually called from the SimulGLWeatherRenderer that
	//! owns this object.
	void						RestoreDeviceObjects(void*);
	//! Destroy the API-specific objects used in rendering.
	void						InvalidateDeviceObjects();
	void						ReloadTextures();
	void						RecompileShaders();
	//! GL Implementation of render function.
	bool						Render(void *,bool blend);
	//! Render the stars, as points.
	bool						RenderPointStars(void *,float exposure);
	//! Draw the 2D fades to screen for debugging.
	bool						RenderFades(void *,int w,int h);

	// Implementing simul::sky::SkyTexturesCallback
	virtual void SetSkyTextureSize(unsigned ){}
	virtual void SetFadeTextureSize(unsigned ,unsigned ,unsigned ){}
	virtual void FillFadeTexturesSequentially(int ,int ,const float *,const float *)
	{
		exit(1);
	}
	virtual		void CycleTexturesForward(){}
	virtual		bool HasFastFadeLookup() const{return true;}
	virtual		const float *GetFastLossLookup(void* context,float distance_texcoord,float elevation_texcoord);
	virtual		const float *GetFastInscatterLookup(void* context,float distance_texcoord,float elevation_texcoord);

	void		RenderPlanet(void *,void* tex,float rad,const float *dir,const float *colr,bool do_lighting);
	void		RenderSun(void *context,float exposure);

	void		Get2DLossAndInscatterTextures(void* *l1,void* *i1,void * *s,void* *o);

	//! This function does nothing as Y is never the vertical in this implementation
	virtual		void SetYVertical(bool ){}
	const		char *GetDebugText();
	simul::sky::BaseGpuSkyGenerator *GetGpuSkyGenerator(){return &gpuSkyGenerator;}
protected:
	void RenderIlluminationBuffer(void *context);
	simul::opengl::GpuSkyGenerator gpuSkyGenerator;
	//! \internal Switch the current program, either sky_program or earthshadow_program.
	//! Also sets the parameter variables.	
	void		UseProgram(GLuint);
	void		SetFadeTexSize(int width_num_distances,int height_num_elevations,int num_altitudes);
	void		FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d
				,const float *loss_float4_array,const float *inscatter_float4_array,const float *skylight_float4_array);

	void		EnsureTexturesAreUpToDate(void *);
	void		EnsureTextureCycle();

	bool		initialized;
	bool		Render2DFades(void *context);
	void		CreateFadeTextures();
	void		CreateSkyTextures();

	GLuint		loss_textures[3];
	GLuint		inscatter_textures[3];
	GLuint		skylight_textures[3];

	bool CreateSkyEffect();
	bool RenderSkyToBuffer();

	unsigned		cloud_texel_index;
	unsigned char	*sky_tex_data;
	
	// Two alternative programs for rendering the sky:
	GLuint			sky_program;
	GLuint			earthshadow_program;
	// Whichever of those two we are currently using:
	GLuint			current_program;
	
	GLuint			planet_program;
	GLuint			sun_program;
	GLuint			stars_program;

	GLuint			fade_3d_to_2d_program;
	GLint			planetTexture_param;
	GLint			planetLightDir_param;
	GLint			planetColour_param;

	GLint			altitudeTexCoord_param;
	GLint			MieRayleighRatio_param;
	GLint			hazeEccentricity_param;
	GLint			lightDirection_sky_param;

	GLint			earthShadowUniforms;
	GLuint			earthShadowUniformsUBO;
	
	GLint			skyInterp_param;
	GLint			sunlight_param;
	
	GLint			starBrightness_param;

	GLint			skyTexture1_param;
	GLint			skyTexture2_param;

	GLint			skylightTexture_param;
	
	GLint			cloudOrigin;
	GLint			cloudScale;
	GLint			maxDistance;
	GLint			viewPosition;
	GLint			overcast_param;

	GLint			altitudeTexCoord_fade;
	GLint			skyInterp_fade;
	GLint			fadeTexture1_fade;
	GLint			fadeTexture2_fade;
	
	FramebufferGL	loss_2d;
	FramebufferGL	inscatter_2d;
	FramebufferGL	skylight_2d;
	FramebufferGL	overcast_2d;

	FramebufferGL	illumination_fb;

	GLuint			loss_texture;
	GLuint			insc_texture;
	GLuint			skyl_texture;

	bool campos_updated;
	short *short_ptr;
	void DrawLines(void *,Vertext *lines,int vertex_count,bool strip=false);
	void PrintAt3dPos(void *,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
};

