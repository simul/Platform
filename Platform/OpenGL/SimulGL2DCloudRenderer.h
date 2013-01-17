// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Base/SmartPtr.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/Export.h"
namespace simul
{
	namespace clouds
	{
		class CloudInterface;
		class CloudKeyframer;
		class FastCloudNode;
	}
	namespace sky
	{
		class AtmosphericScatteringInterface;
	}
}

SIMUL_OPENGL_EXPORT_CLASS SimulGL2DCloudRenderer : public simul::clouds::Base2DCloudRenderer
{
public:
	SimulGL2DCloudRenderer(simul::clouds::CloudKeyframer *ck=NULL);
	virtual ~SimulGL2DCloudRenderer();
	//standard ogl object interface functions
	bool Create();
	//! OpenGL Implementation of device object creation - needs a GL context to be present.
	void RestoreDeviceObjects(void*);
	void RecompileShaders();
	//! OpenGL Implementation of device invalidation - not strictly needed in GL.
	void InvalidateDeviceObjects();
	//! OpenGL Implementation of 2D cloud rendering.
	bool Render(bool cubemap,void *depth_alpha_tex,bool default_fog,bool write_alpha);
	void RenderCrossSections(int width,int height);
	//! Set the platform-dependent atmospheric loss texture.
	void SetLossTexture(void *l);
	//! Set the platform-dependent atmospheric inscatter texture.
	void SetInscatterTextures(void *i,void *s);
	void SetWindVelocity(float x,float y);

	void SetCloudTextureSize(unsigned width_x,unsigned length_y);
	void FillCloudTextureBlock(int texture_index,int x,int y,int w,int l,const unsigned *uint32_array);
	void FillCloudTextureSequentially(int,int,int,const unsigned int *){}
	//CloudShadowCallback
	void **GetCloudTextures(){return 0;}
	void *GetCloudShadowTexture() {return NULL;}
protected:
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate();
	void EnsureTextureCycle();
	void EnsureCorrectIlluminationTextureSizes(){}
	void EnsureIlluminationTexturesAreUpToDate(){}
	
	GLuint clouds_program;
	
	GLuint cross_section_program;

	GLint eyePosition_param;
	GLint lightResponse_param;
	GLint lightDir_param;
	GLint skylightColour_param;
	GLint sunlightColour_param;
	GLint fractalScale_param;
	GLint interp_param;
	GLint layerDensity_param;
	GLint textureEffect_param;
	GLint imageTexture_param;
	GLint lossSampler_param;
	GLint inscatterSampler_param;
	GLint skylightSampler_param;
	GLint cloudEccentricity_param;
	GLint hazeEccentricity_param;
	GLint mieRayleighRatio_param;
	GLint maxFadeDistanceMetres_param;

	GLint coverageTexture1;
	GLint coverageTexture2;
	GLint cloudInterp;
	
	
	GLuint	coverage_tex[3];
	
	GLuint	loss_tex;
	GLuint	inscatter_tex;
	GLuint	skylight_tex;
	GLuint	noise_tex;
	GLuint	image_tex;

	FramebufferGL	detail_fb;
	bool CreateNoiseTexture(bool override_file=false);
	void CreateImageTexture();
	bool CreateCloudEffect();
	bool RenderCloudsToBuffer();
	simul::base::SmartPtr<simul::clouds::FastCloudNode> cloudNode;

	float texture_scale;
	float scale;
	float texture_effect;
	unsigned char *cloud_data;
	unsigned tex_width;
};

