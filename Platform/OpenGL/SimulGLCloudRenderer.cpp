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

SimulGLCloudRenderer::SimulGLCloudRenderer(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem)
	:BaseCloudRenderer(ck,mem)
	,texture_scale(1.f)
	,scale(2.f)
	,texture_effect(1.f)
	,init(false)
{
}


//we require texture updates to occur while GL is active
// so better to update from within Render()
bool SimulGLCloudRenderer::Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap
								  ,crossplatform::NearFarPass nearFarPass,crossplatform::Texture *depth_alpha_tex,bool write_alpha
								  ,const simul::sky::float4& viewportTextureRegionXYWH
								  ,const simul::sky::float4& mixedResTransformXYWH)
{
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	using namespace simul::clouds;
	simul::opengl::ProfileBlock profileBlock("SimulGLCloudRenderer::Render");
GL_ERROR_CHECK
	cubemap;
//cloud buffer alpha to screen = ?
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	simul::math::Vector3 X1,X2;
	cloudProperties.GetExtents(X1,X2);
GL_ERROR_CHECK
	renderPlatform->SetStandardRenderState(deviceContext,crossplatform::STANDARD_DEPTH_DISABLE);
	renderPlatform->SetRenderState(deviceContext,blendAndWriteAlpha);
	// disable alpha testing - if we enable this, the usual reference alpha is reversed because
	// the shaders return transparency, not opacity, in the alpha channel.
    glDisable(GL_ALPHA_TEST);
GL_ERROR_CHECK
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
GL_ERROR_CHECK
    effect->SetTexture(deviceContext,"cloudDensity1",cloud_textures[(texture_cycle+0)%3]);
    effect->SetTexture(deviceContext,"cloudDensity2",cloud_textures[(texture_cycle+1)%3]);
    effect->SetTexture(deviceContext,"noiseSampler",noise_texture);
	effect->SetTexture(deviceContext,"lossSampler",skyLossTexture);
    effect->SetTexture(deviceContext,"inscatterSampler",overcInscTexture);
    effect->SetTexture(deviceContext,"skylightSampler",skylightTexture);
    effect->SetTexture(deviceContext,"illumSampler",illuminationTexture);
GL_ERROR_CHECK
    glActiveTexture(GL_TEXTURE7);
	GLuint program=effect->GetTechniqueByName(depth_alpha_tex?"layers_depth":"layers")->passAsGLuint(0);
		
GL_ERROR_CHECK
	effect->SetTexture(deviceContext,"depthTexture",depth_alpha_tex);

GL_ERROR_CHECK
	glUseProgram(program);
	effect->Apply(deviceContext,effect->GetTechniqueByName(depth_alpha_tex?"layers_depth":"layers"),0);
	effect->SetTexture(deviceContext,"cloudDensity1",cloud_textures[(texture_cycle+0)%3]);
	effect->SetTexture(deviceContext,"cloudDensity2",cloud_textures[(texture_cycle+1)%3]);

	static simul::sky::float4 scr_offset(0,0,0,0);
	
GL_ERROR_CHECK
	const clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();

	static float direct_light_mult=0.25f;
	static float indirect_light_mult=0.03f;
	simul::sky::float4 light_response(	direct_light_mult*K.direct_light
										,indirect_light_mult*K.indirect_light
										,0,0);
	
	simul::sky::float4 fractal_scales=simul::clouds::CloudGeometryHelper::GetFractalScales(cloudKeyframer);

//	float base_alt_km=X1.z*.001f;
	float t=0.f;
	
	if(skyInterface)
		t=skyInterface->GetTime();
	simul::math::Vector3 view_pos;
	simul::math::Vector3 eye_dir;
	simul::math::Vector3 up_dir;
	camera::GetCameraPosVector(deviceContext.viewStruct.view,view_pos,eye_dir,up_dir);
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
    glDisable(GL_BLEND);
    glUseProgram(NULL);
	effect->Unapply(deviceContext);
	
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	//glPopAttrib();
GL_ERROR_CHECK
	return true;
}

void SimulGLCloudRenderer::RecompileShaders()
{
	if(!init)
		return;
GL_ERROR_CHECK
	GetBaseGpuCloudGenerator()->RecompileShaders();
}

void SimulGLCloudRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	init=true;
	BaseCloudRenderer::RestoreDeviceObjects(r);
	BuildSphereVBO();
}

bool SimulGLCloudRenderer::BuildSphereVBO()
{
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
	GetBaseGpuCloudGenerator()->InvalidateDeviceObjects();
	
	BaseCloudRenderer::InvalidateDeviceObjects();
	
	ClearIterators();
}

SimulGLCloudRenderer::~SimulGLCloudRenderer()
{
	InvalidateDeviceObjects();
}

#pragma warning(disable:4127) // "Conditional expression is constant".
void SimulGLCloudRenderer::EnsureCorrectTextureSizes()
{
	int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	int depth_z=i.z;
	bool uav = GetBaseGpuCloudGenerator()->GetEnabled();
	const clouds::CloudProperties &cloudProperties=cloudKeyframer->GetCloudProperties();
	for(int i=0;i<3;i++)
	{
		cloud_textures[i]->ensureTexture3DSizeAndFormat(renderPlatform,width_x,length_y,depth_z,crossplatform::RGBA_8_UNORM,true);
GL_ERROR_CHECK
		glBindTexture(GL_TEXTURE_3D,cloud_textures[i]->AsGLuint());
	}
	int shadow_tex_size = cloudKeyframer->GetShadowTextureSize();
	shadow_fb->ensureTexture2DSizeAndFormat(renderPlatform, shadow_tex_size, cloudKeyframer->GetGodraysSteps(), crossplatform::RGBA_8_UNORM, false, true);
	godrays_texture->ensureTexture2DSizeAndFormat(renderPlatform, shadow_tex_size * 2, cloudKeyframer->GetGodraysSteps(), crossplatform::R_16_FLOAT, true, false);
	moisture_fb->ensureTexture2DSizeAndFormat(renderPlatform, shadow_tex_size * 2, cloudKeyframer->GetGodraysSteps(), crossplatform::R_16_FLOAT, false, true);
	rain_map->ensureTexture2DSizeAndFormat(renderPlatform, width_x / 2, length_y / 2, crossplatform::RGBA_8_UNORM, false, true);
	if (!width_x || !length_y || !depth_z)
		return;
	if (width_x == cloud_tex_width_x&&length_y == cloud_tex_length_y&&depth_z == cloud_tex_depth_z&&cloud_textures[texture_cycle % 3]->AsD3D11UnorderedAccessView() != NULL)
		return;
	cloud_tex_width_x = width_x;
	cloud_tex_length_y = length_y;
	cloud_tex_depth_z = depth_z;

}