// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulGLCloudRenderer.cpp A renderer for 3d clouds.

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

SimulGLCloudRenderer::SimulGLCloudRenderer(const char *license_key)
	:BaseCloudRenderer(license_key)
	,texture_scale(1.f)
	,scale(2.f)
	,texture_effect(1.f)
	,loss_tex(0)
	,inscatter_tex(0)
{
	cloudKeyframer->SetFillTexturesAsBlocks(true);
	cloudKeyframer->SetOpenGL(true);
	cloudNode->SetLicense(license_key);
}

bool SimulGLCloudRenderer::Create()
{
	cloudInterface->Generate();
	//cloudInterface->SetHumidity(0.5f);
	// Must use this next line to prevent the above properties from being overwritten by the keyframes
	//cloudKeyframer->InitKeyframesFromClouds();
	return true;
}

bool SimulGLCloudRenderer::CreateNoiseTexture(bool override_file)
{
	int size=512;

    glGenTextures(1,&noise_tex);
    glBindTexture(GL_TEXTURE_2D,noise_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_R,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_INT,0);
	unsigned char *data=new unsigned char[4*size*size];
	simul::clouds::TextureGenerator::SetBits((unsigned)255<<24,(unsigned)255<<8,(unsigned)255<<16,(unsigned)255<<0,4,false);
	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)data,size,16,6,.79f);
	glTexSubImage2D(
		GL_TEXTURE_2D,0,
		0,0,
		size,size,
		GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
		data);
	glGenerateMipmap(GL_TEXTURE_2D);
	delete [] data;
	return true;
}

void SimulGLCloudRenderer::FillCloudTextureBlock(int texture_index,int x,int y,int z,int w,int l,int d,const unsigned *uint32_array)
{
	glBindTexture(GL_TEXTURE_3D,cloud_tex[texture_index]);
	if(cloudKeyframer->GetUse16Bit())
	{
		unsigned short *uint16_array=(unsigned short *)uint32_array;
		glTexSubImage3D(	GL_TEXTURE_3D,0,
							x,y,z,
							w,l,d,
							GL_RGBA,GL_UNSIGNED_SHORT_4_4_4_4,
							uint16_array);
	}
	else
	{
		glTexSubImage3D(	GL_TEXTURE_3D,0,
							x,y,z,
							w,l,d,
							GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
							uint32_array);
	}
}


void SimulGLCloudRenderer::SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	int *tex=new int[3*depth_z];
	//glGenTextures(3,cloud_tex);
	for(int i=0;i<3;i++)
	{
		glGenTextures(1,&(cloud_tex[i]));
		//cloud_tex[i]=tex[i*depth_z];
		glBindTexture(GL_TEXTURE_3D,cloud_tex[i]);
		if(cloudKeyframer->GetUse16Bit())
			glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA4,width_x,length_y,depth_z,0,GL_RGBA,GL_UNSIGNED_SHORT,0);
		else
			glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA,width_x,length_y,depth_z,0,GL_RGBA,GL_UNSIGNED_INT,0);

		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		if(cloudInterface->GetWrap())
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
	unsigned cloud_mem=cloudNode->GetMemoryUsage();
	std::cout<<"Cloud memory usage: "<<cloud_mem/1024<<"k"<<std::endl;

	delete [] tex;
}

void SimulGLCloudRenderer::CycleTexturesForward()
{
	std::swap(cloud_tex[0],cloud_tex[1]);
	std::swap(cloud_tex[1],cloud_tex[2]);
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

void SimulGLCloudRenderer::FillIlluminationBlock(int source_index,int x,int y,int z,int w,int l,int d,const unsigned char *uchar8_array)
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

//we require texture updates to occur while GL is active
// so better to update from within Render()
bool SimulGLCloudRenderer::Render(bool cubemap,bool depth_testing,bool default_fog)
{
	if(glStringMarkerGREMEDY)
		glStringMarkerGREMEDY(38,"SimulGLCloudRenderer::Render");
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	using namespace simul::clouds;
	simul::math::Vector3 X1,X2;
	cloudInterface->GetExtents(X1,X2);
	if(god_rays)
		X1.z=2.f*X1.z-X2.z;
	simul::math::Vector3 DX=X2-X1;
	simul::math::Matrix4x4 modelview;
	glGetMatrix(modelview.RowPointer(0),GL_MODELVIEW_MATRIX);
	simul::math::Matrix4x4 viewInv;
	Inverse(modelview,viewInv);
	cam_pos[0]=viewInv(3,0);
	cam_pos[1]=viewInv(3,1);
	cam_pos[2]=viewInv(3,2);
    glEnable(GL_BLEND);
	if(god_rays)
		glBlendFunc(GL_ONE,GL_SRC_ALPHA);
	else
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	simul::sky::float4 gl_fog;
	if(default_fog)
	{
		glEnable(GL_FOG);
		glGetFloatv(GL_FOG_COLOR,gl_fog);
	}
	else
		glDisable(GL_FOG);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_STENCIL_TEST);
	glDepthMask(GL_FALSE);
	// disable alpha testing - if we enable this, the usual reference alpha is reversed because
	// the shaders return transparency, not opacity, in the alpha channel.
    glDisable(GL_ALPHA_TEST);
	if(depth_testing)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

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
	glBindTexture(GL_TEXTURE_3D,illum_tex);

	glUseProgram(clouds_program);

	glUniform1i(cloudDensity1_param,0);
	glUniform1i(cloudDensity2_param,1);
	glUniform1i(noiseSampler_param,2);
	glUniform1i(illumSampler_param,3);
ERROR_CHECK
	if(enable_lightning)
	{
		static float bb=.1f;
		simul::sky::float4 lightning_multipliers;
		simul::sky::float4 lightning_colour=lightningRenderInterface->GetLightningColour();
		for(unsigned i=0;i<4;i++)
		{
			if(i<lightningRenderInterface->GetNumLightSources())
				lightning_multipliers[i]=bb*lightningRenderInterface->GetLightSourceBrightness(i);
			else lightning_multipliers[i]=0;
		}
		static float lightning_effect_on_cloud=20.f;
		lightning_colour.w=lightning_effect_on_cloud;
	
//GLint lightningMultipliers;
//GLint lightningColour;
//		m_pCloudEffect->SetVector	(lightningMultipliers_param	,(const float*)(&lightning_multipliers));
//		m_pCloudEffect->SetVector	(lightningColour_param		,(const float*)(&lightning_colour));

		simul::math::Vector3 light_X1,light_X2,light_DX;
		light_X1=lightningRenderInterface->GetIlluminationOrigin();
		light_DX=lightningRenderInterface->GetIlluminationScales();

		glUniform3fv	(illuminationOrigin_param,1,(const float *)(&light_X1));
		glUniform3fv	(illuminationScales_param,1,(const float *)(&light_DX));
	}
ERROR_CHECK

	static float light_direct=0.25f;
	static float light_indirect=0.03f;
	simul::sky::float4 light_response(0,light_direct*cloudInterface->GetLightResponse(),
		light_indirect*cloudInterface->GetSecondaryLightResponse(),0);
//light_response*=gamma;
	//light_response=simul::sky::Pow(light_response,gamma);
	// gamma-compensate for direct light beta function:
	//light_response.y*=pow(0.079577f,gamma)/(0.079577f);

	glUniform4f(lightResponse_param,light_response.x,light_response.y,light_response.z,light_response.w);
	
	simul::sky::float4 fractal_scales=helper->GetFractalScales(cloudInterface);
	glUniform3f(fractalScale_param,fractal_scales.x,fractal_scales.y,fractal_scales.z);
	glUniform1f(interp_param,cloudKeyframer->GetInterpolation());

	glUniform3f(eyePosition_param,cam_pos[0],cam_pos[1],cam_pos[2]);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();
	glUniform3f(lightDirection_param,sun_dir.x,sun_dir.y,sun_dir.z);
	simul::sky::float4 amb=cloudInterface->GetAmbientLightResponse()*skyInterface->GetAmbientLight(X1.z*.001f);
	//amb=simul::sky::Pow(amb,gamma);
	glUniform3f(skylightColour_param,amb.x,amb.y,amb.z);

	glUniform1f(cloudEccentricity_param,cloudInterface->GetMieAsymmetry());
	if(skyEccentricity_param)
		glUniform1f(skyEccentricity_param,skyInterface->GetMieEccentricity());
	simul::sky::float4 mieRayleighRatio=skyInterface->GetMieRayleighRatio();
	if(mieRayleighRatio_param)
		glUniform3f(mieRayleighRatio_param,mieRayleighRatio.x,mieRayleighRatio.y,mieRayleighRatio.z);

	simul::math::Vector3 view_pos(cam_pos[0],cam_pos[1],cam_pos[2]);
	simul::math::Vector3 eye_dir(-viewInv(2,0),-viewInv(2,1),-viewInv(2,2));
	simul::math::Vector3 up_dir	(viewInv(1,0),viewInv(1,1),viewInv(1,2));

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	
	helper->Update(view_pos,cloudNode->GetWindOffset(),eye_dir,up_dir);
	simul::math::Matrix4x4 proj;
	glGetMatrix(proj.RowPointer(0),GL_PROJECTION_MATRIX);

	float zFar=proj(3,2)/(1.f+proj(2,2));
	float zNear=proj(3,2)/(proj(2,2)-1.f);
	zFar=helper->GetMaxCloudDistance()*1.1f;
	proj(2,2)=-(zFar+zNear)/(zFar-zNear);
	proj(3,2)=-2.f*(zNear*zFar)/(zFar-zNear);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(proj.RowPointer(0));

	float left	=proj(0,0)+proj(0,3);
	float right	=proj(0,0)-proj(0,3);

	float tan_half_fov_vertical=1.f/proj(1,1);
	float tan_half_fov_horizontal=std::max(1.f/left,1.f/right);
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	static float churn=1.f;
	helper->SetChurn(churn);
	helper->MakeGeometry(cloudNode.get(),god_rays,X1.z,god_rays);
	// Here we make the helper calculate loss and inscatter due to atmospherics.
	// This is an approach that calculates per-vertex atmospheric values that are then
	// passed to the shader.
	// The alternative is to generate fade textures in the SkyRenderer,
	// then lookup those textures in the cloud shader.
	if(!default_fog)
		helper->CalcInscatterFactors(skyInterface,god_rays);
#if 1
	simul::sky::float4 sunlight1=skyInterface->GetLocalIrradiance(X1.z*.001f);
	simul::sky::float4 sunlight2=skyInterface->GetLocalIrradiance(X2.z*.001f);

	//sunlight1=simul::sky::Pow(sunlight1,gamma);
	//sunlight2=simul::sky::Pow(sunlight2,gamma);

	// Draw the layers of cloud from the furthest to the nearest. Each layer is a spherical shell,
	// which is drawn as a latitude-longitude sphere. But we only draw the parts that:
	// a) are in the view frustum
	//  ...and...
	// b) are in the cloud volume
ERROR_CHECK
	int layers_drawn=0;
	for(std::vector<CloudGeometryHelper::RealtimeSlice*>::const_iterator i=helper->GetSlices().begin();
		i!=helper->GetSlices().end();i++)
	{
		// How thick is this layer, optically speaking?
		float dens=(*i)->fadeIn;
		if(!dens)
			continue;
		glUniform1f(layerDistance_param,(*i)->distance);
		simul::sky::float4 loss			;
		simul::sky::float4 inscatter	;
		if(default_fog)
		{
			float mix_fog;
			GLint fogMode;
			GLfloat fogDens;
			glGetIntegerv(GL_FOG_MODE,&fogMode);
			glGetFloatv(GL_FOG_DENSITY,&fogDens);
			switch(fogMode)
			{
			case GL_EXP:
				mix_fog=1.f-exp(-fogDens*(*i)->distance);
				break;
			case GL_EXP2:
				mix_fog=1.f-exp(-fogDens*(*i)->distance*(*i)->distance);
				mix_fog=mix_fog*mix_fog;
				break;
			default:
				{
					GLfloat fogStart,fogEnd;
					glGetFloatv(GL_FOG_START,&fogStart);
					glGetFloatv(GL_FOG_END,&fogEnd);
					mix_fog=(fogEnd-(*i)->distance)/(fogEnd-fogStart);
					mix_fog=std::min(1.f,mix_fog);
					mix_fog=std::max(0.f,mix_fog);
				}
				break;
			};

			loss=simul::sky::float4(1,1,1,1)*(1.f-mix_fog);
			inscatter=gl_fog*mix_fog;
		}
		layers_drawn++;
		helper->MakeLayerGeometry(cloudNode.get(),*i);
#if 0
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, sphere_vbo);         // for vertex coordinates
		//glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sphere_ibo); // for indices

		glEnableClientState(GL_VERTEX_ARRAY);
		//glEnableClientState(GL_INDEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, 0);
		int first_primitive=((*i)->index_start);
		int num_primitives=((*i)->index_end-(*i)->index_start);
		//use indexing
		glDrawElements(GL_QUAD_STRIP, num_primitives, GL_UNSIGNED_SHORT, &(pIndices[first_primitive]));

		//glDrawElements(GL_QUAD_STRIP,num_primitives, GL_UNSIGNED_SHORT,&(pIndices[first_primitive])); //last 0 is offset in element-array

		//glDisableClientState(GL_INDEX_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);            // deactivate vertex array

		// bind with 0, so, switch back to normal pointer operation
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		//glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
#else
		const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
		size_t qs_vert=0;
		glBegin(GL_QUAD_STRIP);
		for(std::vector<const CloudGeometryHelper::QuadStrip*>::const_iterator j=(*i)->quad_strips.begin();
			j!=(*i)->quad_strips.end();j++)
		{
			// The distance-fade for these clouds. At distance dist, how much of the cloud's colour is lost?
			for(unsigned k=0;k<(*j)->num_vertices;k++,qs_vert++)
			{
				const CloudGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
				if(!default_fog)
				{
					loss		=helper->GetLoss(*i,V);
					inscatter	=helper->GetInscatter(*i,V);
				}
				glMultiTexCoord3f(GL_TEXTURE0,V.cloud_tex_x,V.cloud_tex_y,V.cloud_tex_z);
				glMultiTexCoord2f(GL_TEXTURE1,V.noise_tex_x,V.noise_tex_y);
				glMultiTexCoord1f(GL_TEXTURE2,dens);
				float h=max(0.f,min((V.z-X1.z)/DX.z,1.f));
				simul::sky::float4 sunlight=lerp(h,sunlight1,sunlight2);
				// Here we're passing sunlight values per-vertex, loss and inscatter
				// The per-vertex sunlight allows different altitudes of cloud to have different
				// sunlight colour - good for dawn/sunset.
				// The per-vertex loss and inscatter is cheap for the pixel shader as it
				// then doesn't need fade-texture lookups.
				glMultiTexCoord3f(GL_TEXTURE3,sunlight.x,sunlight.y,sunlight.z);
				glMultiTexCoord3f(GL_TEXTURE4,loss.x,loss.y,loss.z);
				glMultiTexCoord3f(GL_TEXTURE5,inscatter.x,inscatter.y,inscatter.z);
				glVertex3f(V.x,V.y,V.z);
			}
		}
		glEnd();
ERROR_CHECK
#endif
	}
#endif
ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_BLEND);
    glUseProgram(NULL);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_2D);
	glPopAttrib();
ERROR_CHECK
	return true;
}

void SimulGLCloudRenderer::SetLossTextures(void *l)
{
	if(l)
	loss_tex=((GLuint)l);
}

void SimulGLCloudRenderer::SetInscatterTextures(void *i)
{
	if(i)
	inscatter_tex=((GLuint)i);
}

bool SimulGLCloudRenderer::RestoreDeviceObjects(void*)
{
	CreateNoiseTexture();
	clouds_vertex_shader	=glCreateShader(GL_VERTEX_SHADER);
	clouds_fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);

	clouds_program			=glCreateProgram();
	clouds_vertex_shader	=LoadProgram(clouds_vertex_shader,"simul_clouds.vert");
	clouds_fragment_shader	=LoadProgram(clouds_fragment_shader,"simul_clouds.frag","#define DETAIL_NOISE\r\n#define GAMMA_CORRECTION");
	glAttachShader(clouds_program, clouds_vertex_shader);
	glAttachShader(clouds_program, clouds_fragment_shader);
	glLinkProgram(clouds_program);
	glUseProgram(clouds_program);
	printProgramInfoLog(clouds_program);

	lightResponse_param		= glGetUniformLocation(clouds_program,"lightResponse");
	fractalScale_param		= glGetUniformLocation(clouds_program,"fractalScale");
	interp_param			= glGetUniformLocation(clouds_program,"cloud_interp");
	eyePosition_param		= glGetUniformLocation(clouds_program,"eyePosition");
	skylightColour_param	= glGetUniformLocation(clouds_program,"ambientColour");
	lightDirection_param	= glGetUniformLocation(clouds_program,"lightDir");
	sunlightColour_param	= glGetUniformLocation(clouds_program,"sunlight");

	cloudEccentricity_param	= glGetUniformLocation(clouds_program,"cloudEccentricity");
	skyEccentricity_param	= glGetUniformLocation(clouds_program,"skyEccentricity");
	mieRayleighRatio_param	= glGetUniformLocation(clouds_program,"mieRayleighRatio");

	cloudDensity1_param		= glGetUniformLocation(clouds_program,"cloudDensity1");
	cloudDensity2_param		= glGetUniformLocation(clouds_program,"cloudDensity2");
	noiseSampler_param		= glGetUniformLocation(clouds_program,"noiseSampler");
	illumSampler_param		= glGetUniformLocation(clouds_program,"illumSampler");

	layerDistance_param		= glGetUniformLocation(clouds_program,"layerDistance");
	printProgramInfoLog(clouds_program);

	// Because in this sample we are using 32-bit values in the cloud texture:
	//cloudKeyframer->SetUserKeyframes(false);
	cloudKeyframer->SetUse16Bit(false);
	using namespace simul::clouds;
	cloudKeyframer->SetBits(CloudKeyframer::DENSITY,CloudKeyframer::BRIGHTNESS,
		CloudKeyframer::SECONDARY,CloudKeyframer::AMBIENT);
	cloudKeyframer->SetRenderCallback(this);
	glUseProgram(NULL);

	BuildSphereVBO();
	return true;
}
struct vertt
{
	float x,y,z;
};
bool SimulGLCloudRenderer::BuildSphereVBO()
{
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
		for(int j=0;j<az+1;j++)
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
	for(int i=0;i<el;i++)
	{
		int base=i*(az+1);
		for(int j=0;j<az+1;j++)
		{
			*idx=base+j;
			idx++;
			*idx=base+(az+1)+j;
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


bool SimulGLCloudRenderer::InvalidateDeviceObjects()
{
	clouds_program			=0;
	clouds_vertex_shader	=0;
	clouds_fragment_shader	=0;
	lightResponse_param		=0;
	fractalScale_param		=0;
	interp_param			=0;
	eyePosition_param		=0;
	skylightColour_param	=0;
	lightDirection_param	=0;
	sunlightColour_param	=0;
	cloudEccentricity_param	=0;
	skyEccentricity_param	=0;
	mieRayleighRatio_param	=0;
	cloudDensity1_param		=0;
	cloudDensity2_param		=0;
	noiseSampler_param		=0;
	illumSampler_param		=0;

	glDeleteBuffersARB(1,&sphere_vbo);
	glDeleteBuffersARB(1,&sphere_ibo);
	sphere_vbo=sphere_ibo=0;
	return true;
}

void **SimulGLCloudRenderer::GetCloudTextures()
{
	return (void**)cloud_tex;
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
	sprintf(txt,"%3.3g",cloudKeyframer->GetInterpolation());
	return txt;
}

// Save and load a sky sequence
std::ostream &SimulGLCloudRenderer::Save(std::ostream &os) const
{
	return cloudKeyframer->Save(os);
}

std::istream &SimulGLCloudRenderer::Load(std::istream &is) const
{
	return cloudKeyframer->Load(is);
}

void SimulGLCloudRenderer::New()
{
	cloudKeyframer->New();
}
