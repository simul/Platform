// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulGLCloudRenderer.cpp A renderer for 3d clouds.

#include <GL/glew.h>
#include "Simul/Base/Timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <map>
#include <math.h>

#include "FreeImage.h"
#include <fstream>

#include "SimulGLCloudRenderer.h"
#include "SimulGLUtilities.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Platform/OpenGL/Profiler.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Simul/Math/Pi.h"
#include "Simul/Base/SmartPtr.h"
#include "LoadGLProgram.h"

#include <algorithm>

bool god_rays=false;
using std::map;
using namespace std;

class CumulonimbusHumidityCallback:public simul::clouds::HumidityCallbackInterface
{
public:
	virtual float GetHumidityMultiplier(float x,float y,float z) const
	{
		static float base_layer=0.125f;
		static float anvil_radius=0.6f;

		float val=1.f;
#if 1
		float R=0.5f;
#if 1
		if(z>base_layer)
			R*=anvil_radius*z;
#endif
		float dx=x-0.5f;
		float dy=y-0.5f;
		float dr=sqrt(dx*dx+dy*dy);
		if(dr>0.7f*R)
			val=(1.f-dr/R)/0.3f;
		else if(dr>R)
			val=0;
#endif
		static float mul=1.f;
		static float cutoff=0.1f;
		if(z<cutoff)
			return val;
		return mul*val;
	}
};
CumulonimbusHumidityCallback cb;

SimulGLCloudRenderer::SimulGLCloudRenderer(simul::clouds::CloudKeyframer *ck)
	:BaseCloudRenderer(ck)
	,texture_scale(1.f)
	,scale(2.f)
	,texture_effect(1.f)
	,loss_tex(0)
	,inscatter_tex(0)
	,skylight_tex(0)
	,illum_tex(0)
	,init(false)
	,clouds_background_program(0)
	,clouds_foreground_program(0)
	,raytrace_program(0)
	,noise_prog(0)
	,edge_noise_prog(0)
	,current_program(0)
	,cross_section_program(0)
	,cloud_shadow_program(0)

	,cloudConstantsUBO(0)
	,cloudConstantsBindingIndex(2)

	,cloudPerViewConstantsUBO(0)
	,cloudPerViewConstantsBindingIndex(13)

	,layerDataConstantsUBO(0)
	,layerDataConstantsBindingIndex(4)
{
	for(int i=0;i<3;i++)
	{
		cloud_tex[i]=NULL;
		seq_texture_iterator[i].texture_index=i;
	}
	for(int i=0;i<4;i++)
		seq_illum_texture_iterator[i].texture_index=i;
}

bool SimulGLCloudRenderer::Create()
{
	return true;
}

void SimulGLCloudRenderer::CreateVolumeNoise()
{
	GetCloudInterface()->GetNoiseOctaves();
	GetCloudInterface()->GetNoisePeriod();
	GetCloudInterface()->GetNoisePersistence();
	int size=GetCloudInterface()->GetNoiseResolution();
ERROR_CHECK
    glGenTextures(1,&volume_noise_tex);
    glBindTexture(GL_TEXTURE_3D,volume_noise_tex);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	const float *data=GetCloudGridInterface()->GetNoiseInterface()->GetData();
    glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F_ARB,size,size,size,0,GL_RGBA,GL_FLOAT,data);
	//glGenerateMipmap(GL_TEXTURE_3D);
ERROR_CHECK
}

bool SimulGLCloudRenderer::CreateNoiseTexture(void *context)
{
	if(!init)
		return false;
	int noise_texture_size		=cloudKeyframer->GetEdgeNoiseTextureSize();
	int noise_texture_frequency	=cloudKeyframer->GetEdgeNoiseFrequency();
	int texture_octaves			=cloudKeyframer->GetEdgeNoiseOctaves();
	float texture_persistence	=cloudKeyframer->GetEdgeNoisePersistence();
	SAFE_DELETE_TEXTURE(noise_tex);
    glGenTextures(1,&noise_tex);
    glBindTexture(GL_TEXTURE_2D,noise_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_R,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	//GL_RGBA8_SNORM is not yet properly supported!
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,noise_texture_size,noise_texture_size,0,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,0);
ERROR_CHECK
glGenerateMipmap(GL_TEXTURE_2D);
	FramebufferGL noise_fb(noise_texture_frequency,noise_texture_frequency,GL_TEXTURE_2D);
	noise_fb.SetWrapClampMode(GL_REPEAT);
	noise_fb.InitColor_Tex(0,GL_RGBA32F_ARB);
ERROR_CHECK
	noise_fb.Activate(context);
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,1.0,0,1.0,-1.0,1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glUseProgram(noise_prog);
		DrawQuad(0,0,1,1);
	}
	noise_fb.Deactivate(context);
	glUseProgram(0);
ERROR_CHECK	
	FramebufferGL n_fb(noise_texture_size,noise_texture_size,GL_TEXTURE_2D);
ERROR_CHECK	
	n_fb.SetWidthAndHeight(noise_texture_size,noise_texture_size);
	n_fb.SetWrapClampMode(GL_REPEAT);
	n_fb.InitColor_Tex(0,GL_RGBA);
ERROR_CHECK	
	n_fb.Activate(context);
	{
	ERROR_CHECK
		n_fb.Clear(context,0.f,0.f,0.f,0.f,1.f);
	ERROR_CHECK
		Ortho();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,(GLuint)noise_fb.GetColorTex());
	ERROR_CHECK
		glUseProgram(edge_noise_prog);
	ERROR_CHECK
		setParameter(edge_noise_prog,"persistence",texture_persistence);
		setParameter(edge_noise_prog,"octaves",texture_octaves);
		DrawFullScreenQuad();
	}
ERROR_CHECK	
	//glReadBuffer(GL_COLOR_ATTACHMENT0);
ERROR_CHECK	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,noise_tex);
ERROR_CHECK	
	glCopyTexSubImage2D(GL_TEXTURE_2D,
 						0,0,0,0,0,
 						noise_texture_size,
 						noise_texture_size);
ERROR_CHECK	
	glGenerateMipmap(GL_TEXTURE_2D);
ERROR_CHECK	
	n_fb.Deactivate(context);
	glUseProgram(0);
ERROR_CHECK
	return true;
}
	
void SimulGLCloudRenderer::SetIlluminationGridSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	glGenTextures(1,&illum_tex);
	glBindTexture(GL_TEXTURE_3D,illum_tex);
	glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA,width_x,length_y,depth_z,0,GL_RGBA,GL_UNSIGNED_INT,0);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
}

void SimulGLCloudRenderer::FillIlluminationSequentially(int ,int ,int ,const unsigned char *)
{
}

void SimulGLCloudRenderer::FillIlluminationBlock(int ,int x,int y,int z,int w,int l,int d,const unsigned char *uchar8_array)
{
	glBindTexture(GL_TEXTURE_3D,illum_tex);
	glTexSubImage3D(	GL_TEXTURE_3D,0,
						x,y,z,
						w,l,d,
						GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
						uchar8_array);
}

static void glGetMatrix(GLfloat *m,GLenum src=GL_PROJECTION_MATRIX)
{
	glGetFloatv(src,m);
}

void Inverse(const simul::math::Matrix4x4 &Mat,simul::math::Matrix4x4 &Inv)
{
	const simul::math::Vector3 *XX=reinterpret_cast<const simul::math::Vector3*>(Mat.RowPointer(0));
	const simul::math::Vector3 *YY=reinterpret_cast<const simul::math::Vector3*>(Mat.RowPointer(1));
	const simul::math::Vector3 *ZZ=reinterpret_cast<const simul::math::Vector3*>(Mat.RowPointer(2));
	Mat.Transpose(Inv);
	const simul::math::Vector3 &xe=*(reinterpret_cast<const simul::math::Vector3*>(Mat.RowPointer(3)));
	Inv(0,3)=0;
	Inv(1,3)=0;
	Inv(2,3)=0;
	Inv(3,0)=-((xe)*(*XX));
	Inv(3,1)=-((xe)*(*YY));
	Inv(3,2)=-((xe)*(*ZZ));
	Inv(3,3)=1.f;
}
static float saturate(float c)
{
	return std::max(std::min(1.f,c),0.f);
}

void SimulGLCloudRenderer::Update(void *context)
{
	EnsureTexturesAreUpToDate(context);
}

simul::math::Matrix4x4 ConvertReversedToRegularProjectionMatrix(const simul::math::Matrix4x4 &proj)
{
	simul::math::Matrix4x4 p=proj;
	if(proj._43>0)
	{
		float zF=proj._43/proj._33;
		float zN=proj._43*zF/(zF+proj._43);
		p._33=-zF/(zF-zN);
		p._43=-zN*zF/(zF-zN);
	}
	return p;
}
#include "Simul/Camera/Camera.h"
static float transitionDistance=0.01f;
//we require texture updates to occur while GL is active
// so better to update from within Render()
bool SimulGLCloudRenderer::Render(void *,float exposure,bool cubemap,const void *depth_alpha_tex,bool default_fog,bool write_alpha,const simul::sky::float4& viewportTextureRegionXYWH)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	simul::opengl::ProfileBlock profileBlock("SimulCloudRendererDX1x::Render");
	simul::base::Timer timer;
	timer.StartTime();
ERROR_CHECK
	cubemap;
//cloud buffer alpha to screen = ?
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,write_alpha?GL_TRUE:GL_FALSE);
	if(glStringMarkerGREMEDY)
		glStringMarkerGREMEDY(38,"SimulGLCloudRenderer::Render");
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	using namespace simul::clouds;
	simul::math::Vector3 X1,X2;
	GetCloudInterface()->GetExtents(X1,X2);
	if(god_rays)
		X1.z=2.f*X1.z-X2.z;
	simul::math::Vector3 DX=X2-X1;
	simul::math::Matrix4x4 modelview;
	glGetMatrix(modelview.RowPointer(0),GL_MODELVIEW_MATRIX);
	simul::math::Matrix4x4 viewInv;
	Inverse(modelview,viewInv);
	cam_pos.x=viewInv(3,0);
	cam_pos.y=viewInv(3,1);
	cam_pos.z=viewInv(3,2);
	modelview(3,0)=modelview(3,1)=modelview(3,2)=0.f;
ERROR_CHECK
Raytrace=false;
	if(Raytrace)
	{
		glDisable(GL_BLEND);
		glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glEnable(GL_BLEND);
		if(god_rays)
			glBlendFunc(GL_ONE,GL_SRC_ALPHA);
		else
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	}
	simul::sky::float4 gl_fog;
	if(default_fog)
	{
		glEnable(GL_FOG);
		glGetFloatv(GL_FOG_COLOR,gl_fog);
	}
	else
		glDisable(GL_FOG);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE,GL_SRC_ALPHA,GL_ZERO,GL_SRC_ALPHA);
ERROR_CHECK
	glDisable(GL_STENCIL_TEST);
	glDepthMask(GL_FALSE);
	// disable alpha testing - if we enable this, the usual reference alpha is reversed because
	// the shaders return transparency, not opacity, in the alpha channel.
    glDisable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
ERROR_CHECK
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
ERROR_CHECK
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);

    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,cloud_tex[0]);

    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,cloud_tex[1]);

    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,noise_tex);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,loss_tex);

    glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D,inscatter_tex);

    glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D,skylight_tex);

    glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_3D,illum_tex);

    glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D,(GLuint)depth_alpha_tex);

	GLuint program=depth_alpha_tex>0?clouds_foreground_program:clouds_background_program;

	if(Raytrace)
		program=raytrace_program;
	UseShader(program);
	glUseProgram(program);

	glUniform1i(cloudDensity1_param,0);
	glUniform1i(cloudDensity2_param,1);
	glUniform1i(noiseSampler_param,2);
	glUniform1i(lossSampler_param,3);

	glUniform1i(inscatterSampler_param,4);

	glUniform1i(skylightSampler_param,5);

	glUniform1i(illumSampler_param,6);

	glUniform1i(depthTexture,7);

	
	static simul::sky::float4 scr_offset(0,0,0,0);
	
ERROR_CHECK
float time=skyInterface->GetTime();
const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,0);

	CloudConstants cloudConstants;
	CloudPerViewConstants cloudPerViewConstants;
	if(lightningRenderInterface)
	{
		static float bb=.1f;
		simul::sky::float4 lightning_multipliers;
		static float lightning_effect_on_cloud=0.2f;
		simul::sky::float4 lightning_colour=lightningRenderInterface->GetLightningColour();
		for(int i=0;i<4;i++)
		{
			if(i<lightningRenderInterface->GetNumLightSources())
				lightning_multipliers[i]=bb*lightningRenderInterface->GetLightSourceBrightness(time);
			else lightning_multipliers[i]=0;
		}
		lightning_colour.w=lightning_effect_on_cloud;
		lightning_colour*=lightning_effect_on_cloud*lightningRenderInterface->GetLightSourceBrightness(time);
		simul::sky::float4 source_pos=lightningRenderInterface->GetSourcePosition();
		cloudConstants.lightningSourcePos=source_pos;
		cloudConstants.lightningColour=lightning_colour;
	}
	cloudConstants.maxFadeDistanceMetres=max_fade_distance_metres;
	cloudConstants.rain=cloudKeyframer->GetInterpolatedKeyframe().precipitation;
ERROR_CHECK

	static float direct_light_mult=0.25f;
	static float indirect_light_mult=0.03f;
	simul::sky::float4 light_response(	direct_light_mult*GetCloudInterface()->GetLightResponse()
										,indirect_light_mult*GetCloudInterface()->GetSecondaryLightResponse()
										,0
										,0);
	
	simul::sky::float4 fractal_scales=helper->GetFractalScales(GetCloudInterface());

	glUniform3f(eyePosition_param,cam_pos.x,cam_pos.y,cam_pos.z);
	float base_alt_km=X1.z*.001f;
	float t=0.f;
	simul::sky::float4 sunlight1,sunlight2;
	
	if(skyInterface)
	{
		simul::sky::float4 light_dir=skyInterface->GetDirectionToLight(base_alt_km);
		cloudConstants.lightDir=light_dir;
		simul::sky::float4 amb=skyInterface->GetAmbientLight(X1.z*.001f);
		amb*=GetCloudInterface()->GetAmbientLightResponse();
		simul::sky::EarthShadow e=skyInterface->GetEarthShadow(X1.z/1000.f,skyInterface->GetDirectionToSun());
	//	glUniform1f(distanceToIllumination_param,e.illumination_altitude*e.planet_radius*1000.f/max_fade_distance_metres);
		cloudConstants.hazeEccentricity=skyInterface->GetMieEccentricity();
		simul::sky::float4 mie_rayleigh_ratio=skyInterface->GetMieRayleighRatio();
		cloudConstants.mieRayleighRatio=mie_rayleigh_ratio;
		cloudConstants.ambientColour=amb;
		float above=(base_alt_km-e.illumination_altitude)/e.planet_radius+transitionDistance;
		cloudConstants.earthshadowMultiplier=saturate(above/transitionDistance);
		t=skyInterface->GetTime();

		sunlight1=skyInterface->GetLocalIrradiance(base_alt_km)*saturate(base_alt_km-e.illumination_altitude);
		float top_alt_km=X2.z*.001f;
		sunlight2=skyInterface->GetLocalIrradiance(top_alt_km)*saturate(top_alt_km-e.illumination_altitude);
	}
	simul::math::Vector3 view_pos(cam_pos.x,cam_pos.y,cam_pos.z);
	simul::math::Vector3 eye_dir(-viewInv(2,0),-viewInv(2,1),-viewInv(2,2));
	simul::math::Vector3 up_dir	(viewInv(1,0),viewInv(1,1),viewInv(1,2));

	float delta_t=(t-last_time)*cloudKeyframer->GetTimeFactor();
	if(!last_time)
		delta_t=0;
	last_time=t;

	helper->SetChurn(GetCloudInterface()->GetChurn());
	helper->Update(view_pos,GetCloudInterface()->GetWindOffset(),eye_dir,up_dir,delta_t,cubemap);

	simul::math::Matrix4x4 proj;
	glGetMatrix(proj.RowPointer(0),GL_PROJECTION_MATRIX);
	simul::math::Matrix4x4 view;
	glGetMatrix(view.RowPointer(0),GL_MODELVIEW_MATRIX);
	SetCloudPerViewConstants(cloudPerViewConstants,view,proj,exposure,simul::sky::float4(0.f,0.f,1.f,1.f));
	cloudPerViewConstants.exposure=exposure;

	FixGlProjectionMatrix(helper->GetMaxCloudDistance()*1.1f);
	simul::math::Matrix4x4 worldViewProj;
	simul::math::Multiply4x4(worldViewProj,modelview,proj);
	setMatrixTranspose(program,"worldViewProj",worldViewProj);

	float left	=proj(0,0)+proj(0,3);
	float right	=proj(0,0)-proj(0,3);

	float tan_half_fov_vertical=1.f/proj(1,1);
	float tan_half_fov_horizontal=std::max(1.f/left,1.f/right);
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	helper->MakeGeometry(GetCloudInterface(),GetCloudGridInterface(),god_rays,X1.z,god_rays);

helper->Update2DNoiseCoords();
	cloudConstants.fractalScale			=fractal_scales;
	cloudConstants.lightResponse		=light_response;
	cloudConstants.cloud_interp			=cloudKeyframer->GetInterpolation();

	cloudConstants.cloudEccentricity	=GetCloudInterface()->GetMieAsymmetry();

	cloudConstants.lightningMultipliers;
	cloudConstants.lightningColour;
	cloudConstants.screenCoordOffset	=scr_offset;
	cloudConstants.sunlightColour1		=sunlight1;
	cloudConstants.sunlightColour2		=sunlight2;
	simul::math::Vector3 InverseDX	=cloudKeyframer->GetCloudInterface()->GetInverseScales();

	cloudConstants.cornerPos			=X1;
	cloudConstants.inverseScales		=InverseDX;

	glBindBuffer(GL_UNIFORM_BUFFER,cloudConstantsUBO);
	glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof(CloudConstants),&cloudConstants);
	glBindBuffer(GL_UNIFORM_BUFFER,0);
	glBindBufferBase(GL_UNIFORM_BUFFER,cloudConstantsBindingIndex,cloudConstantsUBO);

	UPDATE_CONSTANT_BUFFER(cloudPerViewConstantsUBO,cloudPerViewConstants,cloudPerViewConstantsBindingIndex)
	
	if(Raytrace)
	{
		UseShader(raytrace_program);
		glUseProgram(raytrace_program);
		glDisable(GL_BLEND);
		glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,1.0,0,1.0,-1.0,1.0);
		DrawFullScreenQuad();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glDisable(GL_BLEND);
		glUseProgram(NULL);
		glDisable(GL_TEXTURE_3D);
		glDisable(GL_TEXTURE_2D);
		glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		glPopAttrib();
		return true;
	}
	// Draw the layers of cloud from the furthest to the nearest. Each layer is a spherical shell,
	// which is drawn as a latitude-longitude sphere. But we only draw the parts that:
	// a) are in the view frustum
	//  ...and...
	// b) are in the cloud volume
	ERROR_CHECK
	int idx=0;
	float wavelength=cloudKeyframer->GetCloudInterface()->GetFractalRepeatLength();
	for(std::vector<CloudGeometryHelper::Slice*>::const_iterator i=helper->GetSlices().begin();
		i!=helper->GetSlices().end();i++,idx++)
	{
		LayerData &inst=layerConstants.layers[idx];
		// How thick is this layer, optically speaking?
		simul::clouds::CloudGeometryHelper::Slice *s=*i;
		float dens=s->fadeIn;
		if(!dens)
			continue;
		float noise_offset[]={	s->noise_tex_x/wavelength
								,s->noise_tex_y/wavelength
								,0,0};
		inst.layerFade			=s->fadeIn;
		inst.layerDistance		=s->distance;
		inst.noiseScale			=s->distance/wavelength;
		inst.noiseOffset.x		=noise_offset[0];
		inst.noiseOffset.y		=noise_offset[1];
	}
	layerConstants.layerCount	=helper->GetSlices().size();
	UPDATE_CONSTANT_BUFFER(layerDataConstantsUBO,layerConstants,layerDataConstantsBindingIndex)
	idx=0;
	for(std::vector<CloudGeometryHelper::Slice*>::const_iterator i=helper->GetSlices().begin();i!=helper->GetSlices().end();i++,idx++)
	{
	ERROR_CHECK
		simul::clouds::CloudGeometryHelper::Slice *s=*i;
		helper->MakeLayerGeometry(GetCloudInterface(),s);
		const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
		size_t qs_vert=0;
		setParameter(program,"layerNumber",(int)idx);
		glBegin(GL_QUAD_STRIP);
		if(quad_strip_vertices.size())
		for(std::vector<const CloudGeometryHelper::QuadStrip*>::const_iterator j=(*i)->quad_strips.begin();
			j!=(*i)->quad_strips.end();j++)
		{
			// The distance-fade for these clouds. At distance dist, how much of the cloud's colour is lost?
			for(unsigned k=0;k<(*j)->num_vertices;k++,qs_vert++)
			{
				if(qs_vert<0||qs_vert>=quad_strip_vertices.size())
					continue;
				int v=quad_strip_vertices[qs_vert];
				if(v<0||v>=(int)helper->GetVertices().size())
					continue;
				const CloudGeometryHelper::Vertex &V=helper->GetVertices()[v];
				// Here we're passing sunlight values per-vertex, loss and inscatter
				// The per-vertex sunlight allows different altitudes of cloud to have different
				// sunlight colour - good for dawn/sunset.
				// The per-vertex loss and inscatter is cheap for the pixel shader as it
				// then doesn't need fade-texture lookups.
				glVertex3f(V.x,V.y,V.z);
			}
		}
		glEnd();
	ERROR_CHECK
	}
ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_BLEND);
    glUseProgram(NULL);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_2D);
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	glPopAttrib();
ERROR_CHECK
	timer.FinishTime();
	gpu_time=profileBlock.GetTime();
	return true;
}

void SimulGLCloudRenderer::SetLossTexture(void *l)
{
	if(l)
	loss_tex=((GLuint)l);
}

void SimulGLCloudRenderer::SetInscatterTextures(void *i,void *s)
{
	inscatter_tex=((GLuint)i);
	skylight_tex=((GLuint)s);
}

void SimulGLCloudRenderer::UseShader(GLuint program)
{
	if(current_program==program)
		return;
	current_program=program;
	eyePosition_param			=glGetUniformLocation(program,"eyePosition");
	//hazeEccentricity_param		=glGetUniformLocation(program,"hazeEccentricity");
	//mieRayleighRatio_param		=glGetUniformLocation(program,"mieRayleighRatio");
	maxFadeDistanceMetres_param		=glGetUniformLocation(program,"maxFadeDistanceMetres");

	cloudDensity1_param				=glGetUniformLocation(program,"cloudDensity1");
	cloudDensity2_param				=glGetUniformLocation(program,"cloudDensity2");
	noiseSampler_param				=glGetUniformLocation(program,"noiseSampler");
	illumSampler_param				=glGetUniformLocation(program,"illumSampler");
	lossSampler_param				=glGetUniformLocation(program,"lossSampler");
	inscatterSampler_param			=glGetUniformLocation(program,"inscatterSampler");
	skylightSampler_param			=glGetUniformLocation(program,"skylightSampler");
	depthTexture					=glGetUniformLocation(program,"depthTexture");

	GLint cloudConstants			=glGetUniformBlockIndex(program,"CloudConstants");
	GLint cloudPerViewConstants		=glGetUniformBlockIndex(program,"CloudPerViewConstants");
	//directLightMultiplier	=glGetUniformLocation(current_program,"directLightMultiplier");
ERROR_CHECK
	// If that block IS in the shader program, then BIND it to the relevant UBO.
	if(cloudConstants>=0)
		glUniformBlockBinding(program,cloudConstants,cloudConstantsBindingIndex);
	if(cloudPerViewConstants>=0)
		glUniformBlockBinding(program,cloudPerViewConstants,cloudPerViewConstantsBindingIndex);
ERROR_CHECK
	
	GLint layerDataConstants			=glGetUniformBlockIndex(program,"LayerConstants");
	if(layerDataConstants>=0)
		glUniformBlockBinding(program,layerDataConstants,layerDataConstantsBindingIndex);
ERROR_CHECK
}

void SimulGLCloudRenderer::RecompileShaders()
{
	if(!init)
		return;
current_program=0;
ERROR_CHECK
	SAFE_DELETE_PROGRAM(clouds_background_program);
	SAFE_DELETE_PROGRAM(clouds_foreground_program);
	SAFE_DELETE_PROGRAM(raytrace_program);
	
	SAFE_DELETE_PROGRAM(noise_prog);
	SAFE_DELETE_PROGRAM(edge_noise_prog);
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	defines["DETAIL_NOISE"]="1";
	clouds_background_program	=MakeProgram("simul_clouds",defines);
	defines["USE_DEPTH_TEXTURE"]="1";
	clouds_foreground_program	=MakeProgram("simul_clouds",defines);
	raytrace_program			=MakeProgram("simple.vert",NULL,"simul_raytrace_clouds.frag",defines);
	noise_prog=MakeProgram("simple.vert",NULL,"simul_noise.frag");
	edge_noise_prog=MakeProgram("simple.vert",NULL,"simul_2d_noise.frag");
ERROR_CHECK
	cross_section_program	=MakeProgram("simul_cloud_cross_section");
	
	SAFE_DELETE_PROGRAM(cloud_shadow_program);
	cloud_shadow_program=MakeProgram("simple.vert",NULL,"simul_cloud_shadow.frag");
	glBindBufferRange(GL_UNIFORM_BUFFER,cloudConstantsBindingIndex,cloudConstantsUBO,0, sizeof(CloudConstants));
	glBindBufferRange(GL_UNIFORM_BUFFER,layerDataConstantsBindingIndex,layerDataConstantsUBO,0, sizeof(LayerConstants));
	glBindBufferRange(GL_UNIFORM_BUFFER,cloudPerViewConstantsBindingIndex,cloudPerViewConstantsUBO,0, sizeof(CloudPerViewConstants));

ERROR_CHECK
	glUseProgram(0);
}

void SimulGLCloudRenderer::RestoreDeviceObjects(void *)
{
	init=true;
	
/*	glGenBuffers(1, &cloudConstantsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, cloudConstantsUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(CloudConstants), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);*/
	
	MAKE_CONSTANT_BUFFER(cloudConstantsUBO,CloudConstants,cloudConstantsBindingIndex);
	MAKE_CONSTANT_BUFFER(layerDataConstantsUBO,LayerConstants,layerDataConstantsBindingIndex);
	MAKE_CONSTANT_BUFFER(cloudPerViewConstantsUBO,CloudPerViewConstants,cloudPerViewConstantsBindingIndex);

	RecompileShaders();
	//CreateVolumeNoise();
	using namespace simul::clouds;
	cloudKeyframer->SetBits(CloudKeyframer::DENSITY,CloudKeyframer::BRIGHTNESS,
		CloudKeyframer::SECONDARY,CloudKeyframer::AMBIENT);
//	cloudKeyframer->SetRenderCallback(this);
	glUseProgram(NULL);
	BuildSphereVBO();
	helper->GenerateSphereVertices();
}

struct vertt
{
	float x,y,z;
};
bool SimulGLCloudRenderer::BuildSphereVBO()
{
ERROR_CHECK
	unsigned el=0,az=0;
	helper->GetGrid(el,az);

	int vertex_count=(el+1)*(az+1);
	vertt *pVertices=new vertt[vertex_count];
	vertt *vert=pVertices;
	for(int i=0;i<(int)el+1;i++)
	{
		float elevation=((float)i-(float)el/2.f)/(float)el*pi;
		float z=sin(elevation);
		float ce=cos(elevation);
		for(unsigned j=0;j<az+1;j++)
		{
			float azimuth=(float)j/(float)az*2.f*pi;
			vert->x=cos(azimuth)*ce;
			vert->y=sin(azimuth)*ce;
			vert->z=z;
			vert++;
		}
	}
	// Generate And Bind The Vertex Buffer
	glGenBuffersARB( 1, &sphere_vbo );					// Get A Valid Name
	glBindBufferARB( GL_ARRAY_BUFFER_ARB, sphere_vbo );			// Bind The Buffer
	// Load The Data
	glBufferDataARB( GL_ARRAY_BUFFER_ARB, vertex_count*3*sizeof(float), pVertices, GL_STATIC_DRAW_ARB );
ERROR_CHECK
	// Our Copy Of The Data Is No Longer Necessary, It Is Safe In The Graphics Card
	delete [] pVertices;
	glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );

	int index_count=(el)*(az+1)*2;
	pIndices=new unsigned short[index_count];
	unsigned short *idx=pIndices;
	for(unsigned i=0;i<el;i++)
	{
		int base=i*(az+1);
		for(unsigned j=0;j<az+1;j++)
		{
			*idx=(unsigned short)(base+j);
			idx++;
			*idx=(unsigned short)(base+(az+1)+j);
			idx++;
		}
	}
	glGenBuffersARB(1, &sphere_ibo);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sphere_ibo);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, index_count*sizeof(GLushort), pIndices, GL_STATIC_DRAW_ARB); //upload data

	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
ERROR_CHECK
	return true;
}


void SimulGLCloudRenderer::InvalidateDeviceObjects()
{
	init=false;
	SAFE_DELETE_TEXTURE(noise_tex);
	SAFE_DELETE_PROGRAM(cross_section_program);

	SAFE_DELETE_PROGRAM(clouds_background_program);
	SAFE_DELETE_PROGRAM(cloud_shadow_program);
	SAFE_DELETE_PROGRAM(clouds_foreground_program);
	SAFE_DELETE_PROGRAM(noise_prog);
	SAFE_DELETE_PROGRAM(edge_noise_prog);

	clouds_background_program				=0;
	eyePosition_param			=0;
	//hazeEccentricity_param		=0;
	//mieRayleighRatio_param		=0;
	
	cloudDensity1_param			=0;
	cloudDensity2_param			=0;
	noiseSampler_param			=0;
	illumSampler_param			=0;

	glDeleteBuffersARB(1,&sphere_vbo);
	glDeleteBuffersARB(1,&sphere_ibo);
	sphere_vbo=sphere_ibo=0;

	//glDeleteTexture(volume_noise_tex);
	volume_noise_tex=0;

	glDeleteBuffersARB(1,&cloudConstantsUBO);
	cloudConstantsUBO=0;
	
	ClearIterators();
}

void *SimulGLCloudRenderer::GetCloudShadowTexture()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
	cloud_shadow.SetWidthAndHeight(cloud_tex_width_x,cloud_tex_length_y);
	cloud_shadow.SetWrapClampMode(GL_REPEAT);
	cloud_shadow.InitColor_Tex(0,GL_RGBA);
	
	glUseProgram(cloud_shadow_program);
	glEnable(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,cloud_tex[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,cloud_tex[1]);
	setParameter(cloud_shadow_program,"cloudTexture1"	,0);
	setParameter(cloud_shadow_program,"cloudTexture2"	,1);
	setParameter(cloud_shadow_program,"interp"			,cloudKeyframer->GetInterpolation());
	
	cloud_shadow.Activate(NULL);
		//cloud_shadow.Clear(0.f,0.f,0.f,0.f);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,1.0,0,1.0,-1.0,1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		DrawQuad(0,0,1,1);
		ERROR_CHECK;
	cloud_shadow.Deactivate(NULL);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,0);
	glDisable(GL_TEXTURE_3D);
	glUseProgram(0);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
	glPopAttrib();
	return (void*)cloud_shadow.GetColorTex();
}

simul::sky::OvercastCallback *SimulGLCloudRenderer::GetOvercastCallback()
{
	return cloudKeyframer.get();
}

SimulGLCloudRenderer::~SimulGLCloudRenderer()
{
	InvalidateDeviceObjects();
}

const char *SimulGLCloudRenderer::GetDebugText()
{
	static char txt[100];
	sprintf_s(txt,100,"%3.3g",cloudKeyframer->GetInterpolation());
	return txt;
}


void SimulGLCloudRenderer::New()
{
	cloudKeyframer->New();
}
#pragma warning(disable:4127) // "Conditional expression is constant".
void SimulGLCloudRenderer::EnsureCorrectTextureSizes()
{
	simul::clouds::CloudKeyframer::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	if(cloud_tex_width_x==width_x&&cloud_tex_length_y==length_y&&cloud_tex_depth_z==depth_z
		&&cloud_tex[0]>0)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	for(int i=0;i<3;i++)
	{
		glGenTextures(1,&(cloud_tex[i]));
		glBindTexture(GL_TEXTURE_3D,cloud_tex[i]);
		if(sizeof(simul::clouds::CloudTexelType)==sizeof(GLushort))
			glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA4,width_x,length_y,depth_z,0,GL_RGBA,GL_UNSIGNED_SHORT,0);
		else if(sizeof(simul::clouds::CloudTexelType)==sizeof(GLuint))
			glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA,width_x,length_y,depth_z,0,GL_RGBA,GL_UNSIGNED_INT,0);

		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		if(GetCloudInterface()->GetWrap())
		{
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		}
		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
	}
// lighting is done in CreateCloudTexture, so memory has now been allocated
	unsigned cloud_mem=cloudKeyframer->GetMemoryUsage();
	std::cout<<"Cloud memory usage: "<<cloud_mem/1024<<"k"<<std::endl;

}

void SimulGLCloudRenderer::EnsureTexturesAreUpToDate(void *context)
{
	int a	=cloudKeyframer->GetEdgeNoiseTextureSize();
	int b	=cloudKeyframer->GetEdgeNoiseFrequency();
	int c	=cloudKeyframer->GetEdgeNoiseOctaves();
	float d	=cloudKeyframer->GetEdgeNoisePersistence();
	unsigned check=a+b+c+(*(unsigned*)(&d));
	if(check!=noise_checksum)
	{
		SAFE_DELETE_TEXTURE(noise_tex);
		noise_checksum=check;
	}
	if(!noise_tex)
		CreateNoiseTexture(context);
	EnsureCorrectTextureSizes();
ERROR_CHECK
	EnsureTextureCycle();
	typedef simul::clouds::CloudKeyframer::block_texture_fill iter;
	for(int i=0;i<3;i++)
	{
		if(!cloud_tex[i])
			continue;
		iter texture_fill;
		while((texture_fill=cloudKeyframer->GetBlockTextureFill(seq_texture_iterator[i])).w!=0)
		{
			if(!texture_fill.w||!texture_fill.l||!texture_fill.d)
				break;

			glBindTexture(GL_TEXTURE_3D,cloud_tex[i]);
ERROR_CHECK
			if(sizeof(simul::clouds::CloudTexelType)==sizeof(GLushort))
			{
				unsigned short *uint16_array=(unsigned short *)texture_fill.uint32_array;
				glTexSubImage3D(	GL_TEXTURE_3D,0,
									texture_fill.x,texture_fill.y,texture_fill.z,
									texture_fill.w,texture_fill.l,texture_fill.d,
									GL_RGBA,GL_UNSIGNED_SHORT_4_4_4_4,
									uint16_array);
ERROR_CHECK
			}
			else if(sizeof(simul::clouds::CloudTexelType)==sizeof(GLuint))
			{
				glTexSubImage3D(	GL_TEXTURE_3D,0,
									texture_fill.x,texture_fill.y,texture_fill.z,
									texture_fill.w,texture_fill.l,texture_fill.d,
									GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
									texture_fill.uint32_array);
ERROR_CHECK
			}
			//seq_texture_iterator[i].texel_index+=texture_fill.w*texture_fill.l*texture_fill.d;
		}
	}
}

void SimulGLCloudRenderer::EnsureCorrectIlluminationTextureSizes()
{
}

void SimulGLCloudRenderer::EnsureIlluminationTexturesAreUpToDate()
{
}

void SimulGLCloudRenderer::EnsureTextureCycle()
{
	int cyc=(cloudKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(cloud_tex[0],cloud_tex[1]);
		std::swap(cloud_tex[1],cloud_tex[2]);
		std::swap(seq_texture_iterator[0],seq_texture_iterator[1]);
		std::swap(seq_texture_iterator[1],seq_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}

void SimulGLCloudRenderer::DrawLines(void *,VertexXyzRgba *vertices,int vertex_count,bool strip)
{
	::DrawLines(vertices,vertex_count,strip);
}

void SimulGLCloudRenderer::RenderCrossSections(void *,int width,int height)
{
	static int u=3;
	int w=(width-8)/u;
	if(w>height/2)
		w=height/2;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
	GLint cloudDensity1_param	= glGetUniformLocation(cross_section_program,"cloud_density");
	GLint lightResponse_param	= glGetUniformLocation(cross_section_program,"lightResponse");
	GLint yz_param				= glGetUniformLocation(cross_section_program,"yz");
	GLint crossSectionOffset	= glGetUniformLocation(cross_section_program,"crossSectionOffset");

    glDisable(GL_BLEND);
(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
ERROR_CHECK
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	glUseProgram(cross_section_program);
ERROR_CHECK
static float mult=1.f;
	glUniform1i(cloudDensity1_param,0);
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		simul::sky::float4 light_response(mult*kf->direct_light,mult*kf->indirect_light,mult*kf->ambient_light,0);

	ERROR_CHECK
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,cloud_tex[i]);
		glUniform1f(crossSectionOffset,GetCloudInterface()->GetWrap()?0.5f:0.f);
		glUniform4f(lightResponse_param,light_response.x,light_response.y,light_response.z,light_response.w);
		glUniform1f(yz_param,0.f);
		DrawQuad(i*(w+8)+8,8,w,h);
		glUniform1f(yz_param,1.f);
		DrawQuad(i*(w+8)+8,h+16,w,w);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,noise_tex);
glUseProgram(Utilities::GetSingleton().simple_program);
	DrawQuad(width-(w+8),height-(w+8),w,w);
	glUseProgram(0);
ERROR_CHECK
}