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
	bool Render(bool cubemap,bool depth_testing,bool default_fog,bool write_alpha);
	//! Show the cross sections on-screen.
	void RenderCrossSections(int width,int height);
<<<<<<< HEAD
	void SetLossTexture(void *);
	void SetInscatterTextures(void *,void *);
=======
	void SetLossTextures(void *);
	void SetInscatterTextures(void *);
>>>>>>> master
	//! Get the list of three textures used for cloud rendering.
	void **GetCloudTextures();
	
	void *GetCloudShadowTexture();
	const char *GetDebugText();
	// implementing CloudRenderCallback:
	void SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z);
	//! Stub for sequential fill because we implement block fill instead.
	void FillCloudTextureSequentially(int ,int ,int ,const unsigned *){exit(1);}
	//! Callback implementation for filling cloud texture.
	void FillCloudTextureBlock(int texture_index,int x,int y,int z,int w,int l,int d,const unsigned *uint32_array);
	void CycleTexturesForward();
	
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
	//! This function does nothing as Y is never the vertical in this implementation
	virtual void SetYVertical(bool ){}
	bool IsYVertical() const{return false;}
protected:
	bool init;
	// Make up to date with respect to keyframer:
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate();
	void EnsureCorrectIlluminationTextureSizes();
	void EnsureIlluminationTexturesAreUpToDate();
	void EnsureTextureCycle();

	GLuint clouds_program;

	GLuint cross_section_program;

	GLuint cloud_shadow_program;
	GLint eyePosition_param;
	GLint lightResponse_param;
	GLint lightDir_param;
	GLint skylightColour_param;
	GLint sunlightColour_param;
	GLint fractalScale_param;
	GLint interp_param;
	GLint layerDensity_param;
	GLint textureEffect_param;
	GLint lightDirection_param;
	GLint layerDistance_param;

	GLint cloudDensity1_param;
	GLint cloudDensity2_param;
	GLint noiseSampler_param;
	GLint illumSampler_param;
	GLint lossSampler_param;
	GLint inscatterSampler_param;
	GLint skylightSampler_param;
unsigned short *pIndices;

	GLint illuminationOrigin_param;
	GLint illuminationScales_param;
	GLint lightningMultipliers_param;
	GLint lightningColour_param;
						 

	GLint cloudEccentricity_param;
	GLint hazeEccentricity_param;
	GLint mieRayleighRatio_param;
	GLint earthshadowMultiplier;
	
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

	//float		cam_pos[3];

	void CreateVolumeNoise();
	virtual bool CreateNoiseTexture(bool override_file=false);
	bool CreateCloudEffect();
	bool RenderCloudsToBuffer();

	float texture_scale;
	float scale;
	float texture_effect;

	unsigned max_octave;
	bool BuildSphereVBO();
	FramebufferGL	cloud_shadow;
};

