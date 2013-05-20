// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
namespace simul
{
	namespace clouds
	{
		class CloudInterface;
		class CloudGeometryHelper;
		class FastCloudNode;
		class CloudKeyframer;
	}
	namespace sky
	{
		class BaseSkyInterface;
		class OvercastCallback;
	}
}
//! A renderer for clouds in OpenGL.
SIMUL_OPENGL_EXPORT_CLASS SimulGLCloudRenderer : public simul::clouds::BaseCloudRenderer
{
public:
	SimulGLCloudRenderer(simul::clouds::CloudKeyframer *cloudKeyframer);
	virtual ~SimulGLCloudRenderer();
	//standard ogl object interface functions
	bool Create();
	void RecompileShaders();
	void RestoreDeviceObjects(void*);
	void InvalidateDeviceObjects();
	//! Render the clouds.
	bool Render(void *context,bool cubemap,void *depth_alpha_tex,bool default_fog,bool write_alpha);
	//! Show the cross sections on-screen.
	void RenderCrossSections(void *,int width,int height);
	void SetLossTexture(void *);
	void SetInscatterTextures(void *,void *);
	
	void *GetCloudShadowTexture();
	const char *GetDebugText();
	// implementing CloudRenderCallback:
	void SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z);
	
	void SetIlluminationGridSize(unsigned ,unsigned ,unsigned );
	void FillIlluminationSequentially(int ,int ,int ,const unsigned char *);
	void FillIlluminationBlock(int ,int ,int ,int ,int ,int ,int ,const unsigned char *);
	void GPUTransferDataToTexture(	unsigned char *target_texture
									,const unsigned char *direct_grid
									,const unsigned char *indirect_grid
									,const unsigned char *ambient_grid);

	// a callback function that translates from daytime values to overcast settings. Used for
	// clouds to tell sky when it is overcast.
	simul::sky::OvercastCallback *GetOvercastCallback();
	//! Clear the sequence()
	void New();
protected:
	void SwitchShaders(GLuint program);
	void DrawLines(void *,VertexXyzRgba *vertices,int vertex_count,bool strip);
	bool init;
	// Make up to date with respect to keyframer:
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate(void *);
	void EnsureCorrectIlluminationTextureSizes();
	void EnsureIlluminationTexturesAreUpToDate();
	void EnsureTextureCycle();

	GLuint clouds_background_program;
	GLuint clouds_foreground_program;
	GLuint noise_prog;
	GLuint edge_noise_prog;
	GLuint current_program;
void UseShader(GLuint program);

	GLuint cross_section_program;

	GLuint cloud_shadow_program;
	GLint eyePosition_param;

	GLint cloudDensity1_param;
	GLint cloudDensity2_param;
	GLint noiseSampler_param;
	GLint lossSampler_param;
	GLint inscatterSampler_param;
	GLint illumSampler_param;
	GLint skylightSampler_param;
	GLint depthAlphaTexture;
	GLint layerDistance_param;
unsigned short *pIndices;

	GLint cloudConstants;
	GLuint cloudConstantsUBO;
	GLint cloudConstantsBindingIndex;

	GLint hazeEccentricity_param;
	GLint mieRayleighRatio_param;
	
	GLint		maxFadeDistanceMetres_param;
	GLuint		illum_tex;

	GLuint		cloud_tex[3];
	// 2D textures (x=distance, y=elevation) for fades, updated per-frame:
	GLuint		loss_tex;
	GLuint		inscatter_tex;
	GLuint		skylight_tex;
	
	// 2D texture
	GLuint		noise_tex;
	GLuint		volume_noise_tex;

	GLuint		sphere_vbo;
	GLuint		sphere_ibo;

	void CreateVolumeNoise();
	virtual bool CreateNoiseTexture(void *);
	bool CreateCloudEffect();
	bool RenderCloudsToBuffer();

	float texture_scale;
	float scale;
	float texture_effect;

	unsigned max_octave;
	bool BuildSphereVBO();
	FramebufferGL	cloud_shadow;
};

