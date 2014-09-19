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
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Profiler.h"
#include "Simul/Platform/CrossPlatform/SL/noise_constants.sl"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/Base/SmartPtr.h"
#include "LoadGLProgram.h"
#include "Simul/Camera/Camera.h"

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
	,init(false)
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

void SimulGLCloudRenderer::RenderCloudShadowTexture(crossplatform::DeviceContext &deviceContext)
{
GL_ERROR_CHECK
	renderPlatform->StoreRenderState(deviceContext);
	cloud_shadow.ensureTexture2DSizeAndFormat(renderPlatform,cloud_tex_width_x,cloud_tex_length_y,crossplatform::RGBA_32_FLOAT,false,true);
	//cloud_shadow.SetWrapClampMode(GL_REPEAT);
	//cloud_shadow.InitColor_Tex(0,GL_RGBA);
	GLuint cloud_shadow_program=effect->GetTechniqueByName("cloud_shadow")->passAsGLuint(0);
	glUseProgram(cloud_shadow_program);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,cloud_textures[(texture_cycle+0)%3]->AsGLuint());
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,cloud_textures[(texture_cycle+1)%3]->AsGLuint());
	setParameter(cloud_shadow_program,"cloudDensity1"	,0);
	setParameter(cloud_shadow_program,"cloudDensity2"	,1);
	setParameter(cloud_shadow_program,"cloud_interp"			,cloudKeyframer->GetInterpolation());
	
	cloud_shadow.activateRenderTarget(deviceContext);
		//cloud_shadow.Clear(0.f,0.f,0.f,0.f);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,1.0,0,1.0,-1.0,1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		DrawQuad(0,0,1,1);
		GL_ERROR_CHECK;
	cloud_shadow.deactivateRenderTarget();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,0);
	
	glUseProgram(0);
    renderPlatform->RestoreRenderState(deviceContext);
GL_ERROR_CHECK
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
static float transitionDistance=0.01f;
//we require texture updates to occur while GL is active
// so better to update from within Render()
bool SimulGLCloudRenderer::Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap
								  ,crossplatform::NearFarPass nearFarPass,crossplatform::Texture *depth_alpha_tex,bool write_alpha
								  ,const simul::sky::float4& viewportTextureRegionXYWH
								  ,const simul::sky::float4& mixedResTransformXYWH)
{
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	simul::opengl::ProfileBlock profileBlock("SimulGLCloudRenderer::Render");
GL_ERROR_CHECK
	cubemap;
//cloud buffer alpha to screen = ?
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,write_alpha?GL_TRUE:GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	using namespace simul::clouds;
	simul::math::Vector3 X1,X2;
	cloudProperties.GetExtents(X1,X2);

	simul::math::Vector3 DX=X2-X1;
	simul::math::Matrix4x4 viewInv;
	Inverse(deviceContext.viewStruct.view,viewInv);

	simul::math::Vector3 cam_pos;
	cam_pos.x=viewInv(3,0);
	cam_pos.y=viewInv(3,1);
	cam_pos.z=viewInv(3,2);
	simul::math::Matrix4x4 modelview=deviceContext.viewStruct.view;
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
	bool default_fog=(glIsEnabled(GL_FOG)!=0);
	if(default_fog)
		glGetFloatv(GL_FOG_COLOR,gl_fog);
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
	
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,cloud_textures[(texture_cycle+0)%3]->AsGLuint());

    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,cloud_textures[(texture_cycle+1)%3]->AsGLuint());

    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,noise_texture->AsGLuint());

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,skyLossTexture->AsGLuint());

    glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D,overcInscTexture->AsGLuint());

    glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D,skylightTexture->AsGLuint());

    glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D,illuminationTexture->AsGLuint());
GL_ERROR_CHECK
    glActiveTexture(GL_TEXTURE7);
	GLuint program=effect->GetTechniqueByName(depth_alpha_tex?"layers_depth":"layers")->passAsGLuint(0);
		
GL_ERROR_CHECK
	if(depth_alpha_tex)
	{
		glBindTexture(GL_TEXTURE_2D,depth_alpha_tex->AsGLuint());
	}

GL_ERROR_CHECK
	UseShader(program);
	glUseProgram(program);
	effect->Apply(deviceContext,effect->GetTechniqueByName(depth_alpha_tex?"layers_depth":"layers"),0);
	effect->SetTexture(deviceContext,"cloudDensity1",cloud_textures[(texture_cycle+0)%3]);
	effect->SetTexture(deviceContext,"cloudDensity2",cloud_textures[(texture_cycle+1)%3]);

	glUniform1i(cloudDensity1_param,0);
	glUniform1i(cloudDensity2_param,1);
	glUniform1i(noiseSampler_param,2);
	glUniform1i(lossSampler_param,3);
	glUniform1i(inscatterSampler_param,4);
	glUniform1i(skylightSampler_param,5);
	glUniform1i(illumSampler_param,6);
	glUniform1i(depthTexture,7);
	
	static simul::sky::float4 scr_offset(0,0,0,0);
	
GL_ERROR_CHECK
	const clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();

	static float direct_light_mult=0.25f;
	static float indirect_light_mult=0.03f;
	simul::sky::float4 light_response(	direct_light_mult*K.direct_light
										,indirect_light_mult*K.indirect_light
										,0,0);
	
	simul::sky::float4 fractal_scales=simul::clouds::CloudGeometryHelper::GetFractalScales(cloudKeyframer);

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

	simul::clouds::CloudGeometryHelper *helper=GetCloudGeometryHelper(deviceContext.viewStruct.view_id);
	helper->SetChurn(cloudProperties.GetChurn());
	helper->Update(view_pos,cloudKeyframer->GetWindOffset(),eye_dir,up_dir,delta_t,cubemap);

	SetCloudPerViewConstants(cloudPerViewConstants,deviceContext.viewStruct,exposure,viewportTextureRegionXYWH,mixedResTransformXYWH);
	cloudPerViewConstants.exposure=exposure;

	FixGlProjectionMatrix(helper->GetMaxCloudDistance()*1.1f);

	float left	=deviceContext.viewStruct.proj(0,0)+deviceContext.viewStruct.proj(0,3);
	float right	=deviceContext.viewStruct.proj(0,0)-deviceContext.viewStruct.proj(0,3);

	float tan_half_fov_vertical			=1.f/deviceContext.viewStruct.proj(1,1);
	float tan_half_fov_horizontal		=std::max(1.f/left,1.f/right);
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	float effective_world_radius_metres	=6378000.f;
	float base_alt=K.cloud_base_km*1000.f;
	if(cloudKeyframer->GetMeetHorizon())
		effective_world_radius_metres	=helper->GetEffectiveEarthRadiusToMeetHorizon(base_alt,helper->GetMaxCloudDistance());
	helper->MakeGeometry(cloudKeyframer,GetCloudGridInterface(),false,X1.z,false);

	SetCloudConstants(cloudConstants);
	cloudConstants.Apply(deviceContext);

	//UPDATE_GL_CONSTANT_BUFFER(cloudPerViewConstantsUBO,cloudPerViewConstants,cloudPerViewConstantsBindingIndex)
	cloudPerViewConstants.layerIndex=18;
	cloudPerViewConstants.Apply(deviceContext);
	// Draw the layers of cloud from the furthest to the nearest. Each layer is a spherical shell,
	// which is drawn as a latitude-longitude sphere. But we only draw the parts that:
	// a) are in the view frustum
	//  ...and...
	// b) are in the cloud volume
	GL_ERROR_CHECK
	SetLayerConstants(helper,layerConstants);
	layerConstants.thisLayerIndex=18;
	layerConstants.Apply(deviceContext);
	int idx=0;
	static int isolate_layer=-1;
	sphereMesh.BeginDraw(deviceContext,crossplatform::SHADING_MODE_SHADED);
	for(SliceVector::const_iterator i=helper->GetSlices().begin();i!=helper->GetSlices().end();i++,idx++)
	{
	GL_ERROR_CHECK
		simul::clouds::Slice *RS=*i;
		clouds::SliceInstance s=helper->MakeLayerGeometry(RS,effective_world_radius_metres);
		const LayerData &L=layerConstants.layers[helper->GetSlices().size()-1-idx];
		singleLayerConstants.noiseOffset_	=L.noiseOffset;
		singleLayerConstants.layerFade_		=L.layerFade;
		singleLayerConstants.layerDistance_	=L.layerDistance;
		singleLayerConstants.verticalShift_	=L.verticalShift;
		singleLayerConstants.Apply(deviceContext);
		if(isolate_layer>=0&&idx!=isolate_layer)
			continue;
		sphereMesh.Draw(deviceContext,0,crossplatform::SHADING_MODE_SHADED);
	GL_ERROR_CHECK
	}
	sphereMesh.EndDraw(deviceContext);
GL_ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_BLEND);
    glUseProgram(NULL);
	effect->Unapply(deviceContext);
	
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	glPopAttrib();
GL_ERROR_CHECK
	return true;
}

void SimulGLCloudRenderer::UseShader(GLuint program)
{
GL_ERROR_CHECK
	eyePosition_param				=glGetUniformLocation(program,"eyePosition");
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
}

void SimulGLCloudRenderer::RecompileShaders()
{
	if(!init)
		return;
GL_ERROR_CHECK
	gpuCloudGenerator.RecompileShaders();
	

	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]=ReverseDepth?"1":"0";
	defines["DETAIL_NOISE"]="1";
	defines["USE_DEPTH_TEXTURE"]="1";

	
	SAFE_DELETE(effect);
	effect								=renderPlatform->CreateEffect("clouds",defines);
//	GLuint clouds_background_program	=effect->GetTechniqueByName("layers")->passAsGLuint(0);
//	GLuint clouds_foreground_program	=effect->GetTechniqueByName("layers_depth")->passAsGLuint(0);

//	GLuint cloud_shadow_program=MakeProgram("simple.vert",NULL,"simul_cloud_shadow.frag");
//	GLuint	cross_section_program		=effect->GetTechniqueByName("cross_section")->passAsGLuint(0);
	cloudConstants.LinkToEffect(effect,"CloudConstants");

	cloudPerViewConstants.LinkToEffect(effect,"CloudPerViewConstants");

	layerConstants.LinkToEffect(effect,"LayerConstants");
	
	singleLayerConstants.LinkToEffect(effect,"SingleLayerConstants");
	
GL_ERROR_CHECK
	glUseProgram(0);
}

void SimulGLCloudRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	init=true;
	BaseCloudRenderer::RestoreDeviceObjects(r);
	gpuCloudGenerator.RestoreDeviceObjects(NULL);
	gpuCloudGenerator.SetDirectTargets(cloud_textures);
	

	RecompileShaders();
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
	
	BaseCloudRenderer::InvalidateDeviceObjects();

	eyePosition_param			=0;
	
	cloudDensity1_param			=0;
	cloudDensity2_param			=0;
	noiseSampler_param			=0;
	illumSampler_param			=0;

	glDeleteBuffers(1,&sphere_vbo);
	glDeleteBuffers(1,&sphere_ibo);
	sphere_vbo=sphere_ibo=0;
	
	ClearIterators();
}

CloudShadowStruct SimulGLCloudRenderer::GetCloudShadowTexture(math::Vector3 cam_pos)
{
	CloudShadowStruct s=BaseCloudRenderer::GetCloudShadowTexture(cam_pos);
	s.texture=&cloud_shadow;
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
	int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	if(cloud_tex_width_x==width_x&&cloud_tex_length_y==length_y&&cloud_tex_depth_z==depth_z
		&&cloud_textures[0]->AsGLuint()>0)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	for(int i=0;i<3;i++)
	{
		cloud_textures[i]->ensureTexture3DSizeAndFormat(NULL,width_x,length_y,depth_z,crossplatform::RGBA_8_UNORM,true);
GL_ERROR_CHECK
		glBindTexture(GL_TEXTURE_3D,cloud_textures[i]->AsGLuint());
		if(cloudProperties.GetWrap())
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

void SimulGLCloudRenderer::EnsureTexturesAreUpToDate(crossplatform::DeviceContext &deviceContext)
{
	int a	=cloudKeyframer->GetEdgeNoiseTextureSize();
	int b	=cloudKeyframer->GetEdgeNoiseFrequency();
	int c	=cloudKeyframer->GetEdgeNoiseOctaves();
	float d	=cloudKeyframer->GetEdgeNoisePersistence();
	unsigned check=a+b+c+(*(unsigned*)(&d));
	if(check!=noise_checksum)
	{
		SAFE_DELETE(noise_texture);
		noise_checksum=check;
	}
	if(!noise_texture)
	{
		//while(!noise_texture)
		//	crossplatform::Effect *e=renderPlatform->CreateEffect("temp");
		CreateNoiseTexture(deviceContext);
	}
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

void SimulGLCloudRenderer::RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	BaseCloudRenderer::RenderCrossSections(deviceContext,x0,y0,width,height);
GL_ERROR_CHECK
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	
	effect->Apply(deviceContext,effect->GetTechniqueByName("cross_section"),0);

	static int u=4;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
GL_ERROR_CHECK
	h*=gi->GetGridHeight();
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	if(skyInterface)
	for(int i=0;i<3;i++)
	{
GL_ERROR_CHECK
		const simul::clouds::CloudKeyframer::Keyframe *kf=
			static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
			cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		h=(int)(kf->cloud_height_km*1000.f/cloudProperties.GetCloudWidth()*(float)w);
		sky::float4 light_response(kf->direct_light,kf->indirect_light,kf->ambient_light,0);
GL_ERROR_CHECK
		effect->SetTexture(deviceContext,"cloudDensity",cloud_textures[(texture_cycle+i)%3]);
		effect->Reapply(deviceContext);
		cloudConstants.lightResponse		=light_response;
		cloudConstants.crossSectionOffset	=vec3(0.5f,0.5f,0.f);
		cloudConstants.yz					=0.f;
		cloudConstants.Apply(deviceContext);
		//deviceContext.renderPlatform->DrawQuad(deviceContext,x0+i*(w+1)+4,y0+4,w,h,effect,(void*)cross_section_program);
		cloudConstants.yz					=1.f;
		cloudConstants.Apply(deviceContext);
		//deviceContext.renderPlatform->DrawQuad(deviceContext,x0+i*(w+1)+4,y0+h+8,w,w,effect,(void*)cross_section_program);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,noise_texture->AsGLuint());
	glUseProgram(Utilities::GetSingleton().simple_program);
	DrawQuad(x0+width-(w+8),y0+height-(w+8),w,w);
	glUseProgram(0);
	effect->Unapply(deviceContext);
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
	x0;y0;
}