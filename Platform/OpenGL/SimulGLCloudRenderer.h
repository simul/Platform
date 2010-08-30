// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Clouds/BaseCloudRenderer.h"
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
		class FadeTableInterface;
		class OvercastCallback;
	}
}

class SimulGLCloudRenderer : public simul::clouds::BaseCloudRenderer
{
public:
	SimulGLCloudRenderer();
	virtual ~SimulGLCloudRenderer();
	void SetSkyInterface(simul::sky::BaseSkyInterface *si);
	void SetFadeTable(simul::sky::FadeTableInterface *fti);
	//standard ogl object interface functions
	bool Create( );
	bool RestoreDeviceObjects( );
	bool InvalidateDeviceObjects();
	bool Destroy();
	bool Render(bool depth_testing=false);
	void SetWind(float spd,float dir_deg);
	void SetCloudiness(float h);
	const char *GetDebugText();
	// implementing CloudRenderCallback:
	void SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z);
	//! Stub for sequential fill because we implement block fill instead.
	void FillCloudTextureSequentially(int ,int ,int ,const unsigned *){exit(1);}
	//! Callback implementation for filling cloud texture.
	void FillCloudTextureBlock(int texture_index,int x,int y,int z,int w,int l,int d,const unsigned *uint32_array);
	void CycleTexturesForward();
	void SetDetail(float d);
	// a callback function that translates from daytime values to overcast settings. Used for
	// clouds to tell sky when it is overcast.
	simul::sky::OvercastCallback *GetOvercastCallback();
	// Save and load a sky sequence
	std::ostream &Save(std::ostream &os) const;
	std::istream &Load(std::istream &is) const;
	//! Clear the sequence()
	void New();
protected:
	float detail;
	GLuint clouds_program;
	GLuint clouds_vertex_shader,clouds_fragment_shader;

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

	GLint cloudDensity1_param;
	GLint cloudDensity2_param;
	GLint noiseSampler_param;

	GLint cloudEccentricity_param;
	GLint skyEccentricity_param;
	GLint mieRayleighRatio_param;

	GLuint		cloud_tex[3];
	GLuint		noise_tex;
	GLuint		image_tex;
	float		cam_pos[3];

	bool CreateNoiseTexture();
	bool CreateCloudEffect();
	bool RenderCloudsToBuffer();

	float texture_scale;
	float scale;
	float texture_effect;

	unsigned max_octave;
};

