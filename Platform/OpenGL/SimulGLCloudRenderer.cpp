// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulGLCloudRenderer.cpp A renderer for 3d clouds.
#define NOMINMAX

#ifdef _MSC_VER
#include <windows.h>
#else
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

#include <GL/glew.h>
#ifdef USE_GLFX
#include <GL/glfx.h>
#endif
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
#include "Simul/Scene/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Profiler.h"
#include "Simul/Platform/CrossPlatform/SL/noise_constants.sl"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/Base/SmartPtr.h"
#include "LoadGLProgram.h"

#include <algorithm>
#include <stdint.h>  // for uintptr_t

using namespace simul;
using namespace opengl;
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

SimulGLCloudRenderer::SimulGLCloudRenderer(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem)
	:BaseCloudRenderer(ck,mem)
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
//	,effect(0)
{
	for(int i=0;i<3;i++)
	{
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
GL_ERROR_CHECK
    glGenTextures(1,&volume_noise_tex);
    glBindTexture(GL_TEXTURE_3D,volume_noise_tex);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	const float *data=GetCloudGridInterface()->GetNoiseInterface()->GetData();
    glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,size,size,size,0,GL_RGBA,GL_FLOAT,data);
	//glGenerateMipmap(GL_TEXTURE_3D);
GL_ERROR_CHECK
}

void SimulGLCloudRenderer::CreateNoiseTexture(void *context)
{
	if(!init)
		return ;
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
GL_ERROR_CHECK
glGenerateMipmap(GL_TEXTURE_2D);
	FramebufferGL noise_fb(noise_texture_frequency,noise_texture_frequency,GL_TEXTURE_2D);
	noise_fb.SetWrapClampMode(GL_REPEAT);
	noise_fb.InitColor_Tex(0,GL_RGBA32F);
GL_ERROR_CHECK
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
GL_ERROR_CHECK	
	FramebufferGL n_fb(noise_texture_size,noise_texture_size,GL_TEXTURE_2D);
GL_ERROR_CHECK	
	n_fb.SetWidthAndHeight(noise_texture_size,noise_texture_size);
	n_fb.SetWrapClampMode(GL_REPEAT);
	n_fb.InitColor_Tex(0,GL_RGBA);
GL_ERROR_CHECK	
	n_fb.Activate(context);
	{
	GL_ERROR_CHECK
		n_fb.Clear(context,0.f,0.f,0.f,0.f,1.f);
	GL_ERROR_CHECK
		Ortho();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,(GLuint)(uintptr_t)noise_fb.GetColorTex());
	GL_ERROR_CHECK
		glUseProgram(edge_noise_prog);
	GL_ERROR_CHECK
		simul::opengl::ConstantBuffer<RendernoiseConstants> rendernoiseConstants;
		rendernoiseConstants.RestoreDeviceObjects();
		rendernoiseConstants.LinkToProgram(edge_noise_prog,"RendernoiseConstants",0);
		rendernoiseConstants.octaves		=texture_octaves;
		rendernoiseConstants.persistence	=texture_persistence;
		rendernoiseConstants.Apply();
		DrawFullScreenQuad();
		rendernoiseConstants.Release();
	}
GL_ERROR_CHECK	
	//glReadBuffer(GL_COLOR_ATTACHMENT0);
GL_ERROR_CHECK	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,noise_tex);
GL_ERROR_CHECK	
	glCopyTexSubImage2D(GL_TEXTURE_2D,
 						0,0,0,0,0,
 						noise_texture_size,
 						noise_texture_size);
GL_ERROR_CHECK	
	glGenerateMipmap(GL_TEXTURE_2D);
GL_ERROR_CHECK	
	n_fb.Deactivate(context);
	glUseProgram(0);
GL_ERROR_CHECK
}
	
void SimulGLCloudRenderer::SetIlluminationGridSize(unsigned ,unsigned ,unsigned )
{
}

void SimulGLCloudRenderer::FillIlluminationSequentially(int ,int ,int ,const unsigned char *)
{
}

void SimulGLCloudRenderer::FillIlluminationBlock(int ,int ,int ,int ,int ,int ,int ,const unsigned char *)
{
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

void SimulGLCloudRenderer::PreRenderUpdate(void *context)
{
	EnsureTexturesAreUpToDate(context);
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
	glBindTexture(GL_TEXTURE_3D,cloud_textures[(texture_cycle+0)%3].tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,cloud_textures[(texture_cycle+1)%3].tex);
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
		GL_ERROR_CHECK;
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
bool SimulGLCloudRenderer::Render(void *,float exposure,bool cubemap,bool /*near_pass*/,const void *depth_alpha_tex,bool default_fog,bool write_alpha
								  ,int viewport_id,const simul::sky::float4& viewportTextureRegionXYWH,const simul::sky::float4& mixedResTransformXYWH)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	simul::opengl::ProfileBlock profileBlock("SimulGLCloudRenderer::Render");
GL_ERROR_CHECK
	cubemap;
//cloud buffer alpha to screen = ?
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,write_alpha?GL_TRUE:GL_FALSE);
	if(glStringMarkerGREMEDY)
		glStringMarkerGREMEDY(38,"SimulGLCloudRenderer::Render");
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	using namespace simul::clouds;
	simul::math::Vector3 X1,X2;
	GetCloudInterface()->GetExtents(X1,X2);

	simul::math::Vector3 DX=X2-X1;
	simul::math::Matrix4x4 modelview;
	glGetMatrix(modelview.RowPointer(0),GL_MODELVIEW_MATRIX);
	simul::math::Matrix4x4 viewInv;
	Inverse(modelview,viewInv);
	cam_pos.x=viewInv(3,0);
	cam_pos.y=viewInv(3,1);
	cam_pos.z=viewInv(3,2);
	modelview(3,0)=modelview(3,1)=modelview(3,2)=0.f;
GL_ERROR_CHECK
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
GL_ERROR_CHECK
	glDisable(GL_STENCIL_TEST);
	glDepthMask(GL_FALSE);
	// disable alpha testing - if we enable this, the usual reference alpha is reversed because
	// the shaders return transparency, not opacity, in the alpha channel.
    glDisable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
GL_ERROR_CHECK
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
GL_ERROR_CHECK
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);

    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,cloud_textures[(texture_cycle+0)%3].tex);

    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,cloud_textures[(texture_cycle+1)%3].tex);

    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,noise_tex);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,loss_tex);

    glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D,overcast_tex);

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
	
//const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,0);

	//CloudPerViewConstants cloudPerViewConstants;
GL_ERROR_CHECK

	static float direct_light_mult=0.25f;
	static float indirect_light_mult=0.03f;
	simul::sky::float4 light_response(	direct_light_mult*GetCloudInterface()->GetLightResponse()
										,indirect_light_mult*GetCloudInterface()->GetSecondaryLightResponse()
										,0,0);
	
	simul::sky::float4 fractal_scales=simul::clouds::CloudGeometryHelper::GetFractalScales(GetCloudInterface());

	glUniform3f(eyePosition_param,cam_pos.x,cam_pos.y,cam_pos.z);
//	float base_alt_km=X1.z*.001f;
	float t=0.f;
	
	if(skyInterface)
		t=skyInterface->GetTime();
	simul::math::Vector3 view_pos(cam_pos.x,cam_pos.y,cam_pos.z);
	simul::math::Vector3 eye_dir(-viewInv(2,0),-viewInv(2,1),-viewInv(2,2));
	simul::math::Vector3 up_dir	(viewInv(1,0),viewInv(1,1),viewInv(1,2));

	float delta_t=(t-last_time)*cloudKeyframer->GetTimeFactor();
	if(!last_time)
		delta_t=0;
	last_time=t;

	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(viewport_id);
	helper->SetChurn(GetCloudInterface()->GetChurn());
	helper->Update(view_pos,GetCloudInterface()->GetWindOffset(),eye_dir,up_dir,delta_t,cubemap);

	simul::math::Matrix4x4 proj;
	glGetMatrix(proj.RowPointer(0),GL_PROJECTION_MATRIX);
	simul::math::Matrix4x4 view;
	glGetMatrix(view.RowPointer(0),GL_MODELVIEW_MATRIX);
	SetCloudPerViewConstants(cloudPerViewConstants,view,proj,exposure,viewport_id,viewportTextureRegionXYWH,mixedResTransformXYWH);
	cloudPerViewConstants.exposure=exposure;

	FixGlProjectionMatrix(helper->GetMaxCloudDistance()*1.1f);
	simul::math::Matrix4x4 worldViewProj;
	simul::math::Multiply4x4(worldViewProj,modelview,proj);
	setMatrixTranspose(program,"worldViewProj",worldViewProj);

	float left	=proj(0,0)+proj(0,3);
	float right	=proj(0,0)-proj(0,3);

	float tan_half_fov_vertical			=1.f/proj(1,1);
	float tan_half_fov_horizontal		=std::max(1.f/left,1.f/right);
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	float effective_world_radius_metres	=6378000.f;
	float base_alt=GetCloudInterface()->GetCloudBaseZ();
	if(cloudKeyframer->GetMeetHorizon())
		effective_world_radius_metres	=helper->GetEffectiveEarthRadiusToMeetHorizon(base_alt,helper->GetMaxCloudDistance());
	helper->MakeGeometry(GetCloudInterface(),GetCloudGridInterface(),effective_world_radius_metres,false,X1.z,false);

	SetCloudConstants(cloudConstants);
	cloudConstants.Apply();

	//UPDATE_GL_CONSTANT_BUFFER(cloudPerViewConstantsUBO,cloudPerViewConstants,cloudPerViewConstantsBindingIndex)
	cloudPerViewConstants.layerIndex=18;
	cloudPerViewConstants.Apply();
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
	GL_ERROR_CHECK
	SetLayerConstants(helper,layerConstants);
	layerConstants.thisLayerIndex=18;
	layerConstants.Apply();
//	UPDATE_GL_CONSTANT_BUFFER(layerDataConstantsUBO,layerConstants,layerDataConstantsBindingIndex)
	int idx=0;
	static int isolate_layer=-1;
	sphereMesh.BeginDraw(NULL,scene::SHADING_MODE_SHADED);
	for(SliceVector::const_iterator i=helper->GetSlices().begin();i!=helper->GetSlices().end();i++,idx++)
	{
	GL_ERROR_CHECK
		simul::clouds::Slice *RS=*i;
		clouds::SliceInstance s=helper->MakeLayerGeometry(RS,effective_world_radius_metres);
		const simul::clouds::IntVector &quad_strip_vertices=s.quad_strip_indices;
		size_t qs_vert=0;
//		int layer=(int)helper->GetSlices().size()-1-idx;
//		setParameter(program,"layerNumber",layer);
		const LayerData &L=layerConstants.layers[helper->GetSlices().size()-1-idx];
		singleLayerConstants.noiseOffset_	=L.noiseOffset;
		singleLayerConstants.layerFade_		=L.layerFade;
		singleLayerConstants.layerDistance_	=L.layerDistance;
		singleLayerConstants.verticalShift_	=L.verticalShift;
		singleLayerConstants.Apply();
		if(isolate_layer>=0&&idx!=isolate_layer)
			continue;
#if 1
		sphereMesh.Draw(NULL,0,scene::SHADING_MODE_SHADED);
#else
		glBegin(GL_QUAD_STRIP);
		if(quad_strip_vertices.size())
		for(QuadStripVector::const_iterator j=s.quadStrips.begin();
			j!=s.quadStrips.end();j++)
		{
			// The distance-fade for these clouds. At distance dist, how much of the cloud's colour is lost?
			for(unsigned k=0;k<j->num_vertices;k++,qs_vert++)
			{
				if(qs_vert<0||qs_vert>=quad_strip_vertices.size())
					continue;
				int v=quad_strip_vertices[qs_vert];
				if(v<0||v>=(int)s.vertices.size())
					continue;
				const Vertex &V=s.vertices[v];
				glVertex3f(V.x,V.y,V.z);
			}
		}
		glEnd();
#endif
	GL_ERROR_CHECK
	}
	sphereMesh.EndDraw(NULL);
GL_ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_BLEND);
    glUseProgram(NULL);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_2D);
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	glPopAttrib();
GL_ERROR_CHECK
	return true;
}

void SimulGLCloudRenderer::SetLossTexture(void *l)
{
	if(l)
	loss_tex=((GLuint)l);
}

void SimulGLCloudRenderer::SetInscatterTextures(void* i,void *s,void *o)
{
	inscatter_tex=((GLuint)i);
	skylight_tex=((GLuint)s);
	overcast_tex=((GLuint)o);
}

void SimulGLCloudRenderer::SetIlluminationTexture(void* i)
{
	illum_tex=((GLuint)i);
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

GL_ERROR_CHECK
	// If that block IS in the shader program, then BIND it to the relevant UBO.
	cloudConstants			.LinkToProgram(program,"CloudConstants",2);
	layerConstants			.LinkToProgram(program,"LayerConstants",4);
	singleLayerConstants	.LinkToProgram(program,"SingleLayerConstants",5);
	cloudPerViewConstants.LinkToProgram(program,"CloudPerViewConstants",13);
GL_ERROR_CHECK
}

void SimulGLCloudRenderer::RecompileShaders()
{
	if(!init)
		return;
current_program=0;
GL_ERROR_CHECK
	gpuCloudGenerator.RecompileShaders();
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
	noise_prog					=MakeProgram("simple.vert",NULL,"simul_noise.frag");
	edge_noise_prog				=MakeProgram("simple.vert",NULL,"simul_2d_noise.frag");

	cross_section_program		=MakeProgram("simul_cloud_cross_section");
/*	
	glfxDeleteEffect(effect);
	effect						=opengl::CreateEffect("clouds.glfx",defines);
	if(effect>=0)
	{
		GLuint p				=glfxCompileProgram(effect, "cross_section");
		if (!p)
		{
   			std::string log		= glfxGetEffectLog(effect);
   			std::cerr << "Error parsing effect: " << log << std::endl;
		}
		else
			cross_section_program=p;
	}*/
	SAFE_DELETE_PROGRAM(cloud_shadow_program);
	cloud_shadow_program=MakeProgram("simple.vert",NULL,"simul_cloud_shadow.frag");
	cloudConstants.LinkToProgram(cross_section_program,"CloudConstants",2);
	cloudConstants.LinkToProgram(clouds_background_program,"CloudConstants",2);
	cloudConstants.LinkToProgram(clouds_foreground_program,"CloudConstants",2);
	//glBindBufferRange(GL_UNIFORM_BUFFER,cloudConstantsBindingIndex,cloudConstantsUBO,0, sizeof(CloudConstants));
//	glBindBufferRange(GL_UNIFORM_BUFFER,layerDataConstantsBindingIndex,layerDataConstantsUBO,0, sizeof(LayerConstants));
	//glBindBufferRange(GL_UNIFORM_BUFFER,cloudPerViewConstantsBindingIndex,cloudPerViewConstantsUBO,0, sizeof(CloudPerViewConstants));
	cloudPerViewConstants.LinkToProgram(clouds_background_program,"CloudPerViewConstants",13);
	cloudPerViewConstants.LinkToProgram(clouds_foreground_program,"CloudPerViewConstants",13);

	layerConstants.LinkToProgram(clouds_background_program,"LayerConstants",4);
	layerConstants.LinkToProgram(clouds_foreground_program,"LayerConstants",4);
	
	singleLayerConstants.LinkToProgram(clouds_background_program,"SingleLayerConstants",5);
	singleLayerConstants.LinkToProgram(clouds_foreground_program,"SingleLayerConstants",5);
	
GL_ERROR_CHECK
	glUseProgram(0);
}

void SimulGLCloudRenderer::RestoreDeviceObjects(void *)
{
	init=true;
	gpuCloudGenerator.RestoreDeviceObjects(NULL);
	TextureStruct *ts[]={&cloud_textures[0],&cloud_textures[1],&cloud_textures[2]};
	gpuCloudGenerator.SetDirectTargets(ts);
	
	cloudConstants.RestoreDeviceObjects();
	layerConstants.RestoreDeviceObjects();
	singleLayerConstants.RestoreDeviceObjects();
	cloudPerViewConstants.RestoreDeviceObjects();

	RecompileShaders();
	//CreateVolumeNoise();
	using namespace simul::clouds;
	cloudKeyframer->SetBits(CloudKeyframer::DENSITY,CloudKeyframer::BRIGHTNESS,
		CloudKeyframer::SECONDARY,CloudKeyframer::AMBIENT);
//	cloudKeyframer->SetRenderCallback(this);
	glUseProgram(NULL);
	BuildSphereVBO();
}

struct vertt
{
	float x,y,z;
};

bool SimulGLCloudRenderer::BuildSphereVBO()
{
	SIMUL_ASSERT(CheckExtension("GL_ARB_vertex_buffer_object"));
GL_ERROR_CHECK
	unsigned el=16,az=32;
	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(0);
	//helper->GetGrid(el,az);
	std::vector<vec3> vertices;
	std::vector<unsigned int> indices;
	helper->GenerateSphereVertices(vertices,indices,el,az);
	sphereMesh.Initialize(vertices,indices);
GL_ERROR_CHECK
	return true;
}


void SimulGLCloudRenderer::InvalidateDeviceObjects()
{
	init=false;
	gpuCloudGenerator.InvalidateDeviceObjects();
	
	//glfxDeleteEffect(effect);
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

	glDeleteBuffers(1,&sphere_vbo);
	glDeleteBuffers(1,&sphere_ibo);
	sphere_vbo=sphere_ibo=0;

	volume_noise_tex=0;

	//glDeleteBuffers(1,&cloudConstantsUBO);
	cloudConstants.Release();
	cloudPerViewConstants.Release();
	layerConstants.Release();
	singleLayerConstants.Release();
	//cloudConstantsUBO=0;
	
	ClearIterators();
}

CloudShadowStruct SimulGLCloudRenderer::GetCloudShadowTexture()
{
	CloudShadowStruct s=BaseCloudRenderer::GetCloudShadowTexture();
	s.texture=(void*)cloud_shadow.GetColorTex();
	return s;
}

simul::sky::OvercastCallback *SimulGLCloudRenderer::GetOvercastCallback()
{
	return cloudKeyframer;
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
	sky::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	if(cloud_tex_width_x==width_x&&cloud_tex_length_y==length_y&&cloud_tex_depth_z==depth_z
		&&cloud_textures[0].tex>0)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	for(int i=0;i<3;i++)
	{
		cloud_textures[i].ensureTexture3DSizeAndFormat(NULL,width_x,length_y,depth_z,GL_RGBA,true);
GL_ERROR_CHECK
		glBindTexture(GL_TEXTURE_3D,cloud_textures[i].tex);
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
GL_ERROR_CHECK
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		int cycled_index=(texture_cycle+i)%3;
		clouds::GpuCloudsParameters g=cloudKeyframer->GetGpuCloudsParameters(i);
		gpuCloudGenerator.Update(cycled_index,g,NULL);
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

void SimulGLCloudRenderer::RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	glUseProgram(cross_section_program);

	static int u=4;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
	if(skyInterface)
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
			static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
			cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		h=(int)(kf->cloud_height_km*1000.f/GetCloudInterface()->GetCloudWidth()*(float)w);
		sky::float4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
		set3DTexture(cross_section_program,"cloudDensity",0,cloud_textures[(texture_cycle+i)%3].tex);
		
		cloudConstants.lightResponse		=light_response;
		cloudConstants.crossSectionOffset	=vec3(0.5f,0.5f,0.f);
		cloudConstants.yz					=0.f;
		cloudConstants.Apply();
		deviceContext.renderPlatform->DrawQuad(deviceContext.platform_context,x0+i*(w+1)+4,y0+4,w,h,NULL,(void*)cross_section_program);
		cloudConstants.yz					=1.f;
		cloudConstants.Apply();
		deviceContext.renderPlatform->DrawQuad(deviceContext.platform_context,x0+i*(w+1)+4,y0+h+8,w,w,NULL,(void*)cross_section_program);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,noise_tex);
	glUseProgram(Utilities::GetSingleton().simple_program);
	DrawQuad(x0+width-(w+8),y0+height-(w+8),w,w);
	glUseProgram(0);
}

void SimulGLCloudRenderer::RenderAuxiliaryTextures(crossplatform::DeviceContext &,int x0,int y0,int width,int height)
{
	static int u=4;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
}