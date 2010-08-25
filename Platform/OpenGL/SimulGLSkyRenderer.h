// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once

#include "Simul/Sky/BaseSkyRenderer.h"
namespace simul
{
	namespace sky
	{
		class SiderealSkyInterface;
		class SkyInterface;
		class SkyNode;
		class FadeTableInterface;
		class AltitudeFadeTable;
		class OvercastCallback;
	}
}

class SimulGLSkyRenderer : public simul::sky::BaseSkyRenderer
{
public:
	SimulGLSkyRenderer();
	virtual ~SimulGLSkyRenderer();
	//standard ogl object interface functions
	bool Create(float start_alt_km=0.f);
	bool RestoreDeviceObjects();
	bool InvalidateDeviceObjects();
	bool Destroy();
	bool Render();

	// Implementing simul::sky::FadeTableCallback
	virtual void SetSkyTextureSize(unsigned size);
	virtual void SetFadeTextureSize(unsigned width_num_distances,unsigned height_num_elevations,unsigned num_altitudes);
	virtual void FillFadeTexturesSequentially(int ,int ,int ,const float *,const float *)
	{
		exit(1);
	}
	virtual void FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d,const float *loss_float4_array,const float *inscatter_float4_array);
	virtual void FillSkyTexture(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array);
	virtual void CycleTexturesForward();

	bool RenderPlanet(void* tex,float rad,const float *dir,const float *colr,bool do_lighting);
protected:
	void CreateFadeTextures();
	bool RenderAngledQuad(const float *dir,float half_angle_radians);
	GLuint		sky_tex[3];
	GLuint		loss_textures[3];
	GLuint		inscatter_textures[3];

	bool CreateSkyEffect();
	bool RenderSkyToBuffer();

	unsigned cloud_texel_index;
	unsigned char *sky_tex_data;
	GLuint sky_vertex_shader,sky_fragment_shader;
	GLuint sky_program;
	GLuint planet_program;
	GLint planetTexture_param;
	GLint planetLightDir_param;

	GLint altitudeTexCoord_param;
	GLint MieRayleighRatio_param;
	GLint hazeEccentricity_param;
	GLint lightDirection_sky_param;
	GLint skyInterp_param;
	
	GLint skyTexture1_param;
	GLint skyTexture2_param;

	unsigned skyTexSize;
	bool campos_updated;
	short *short_ptr;
	virtual bool IsYVertical()
	{
		return false;
	}
};

