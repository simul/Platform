#ifdef _MSC_VER
	#include <windows.h>
#endif
#ifdef WIN64
#pragma message("WIN64")
#undef WIN32
#endif
#include "Simul/Base/Timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <map>
#include <math.h>
#include <GL/glew.h>

#include "FreeImage.h"
#include <fstream>
#include <algorithm>

#include "SimulGL2DCloudRenderer.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Sky/Sky.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Simul/Math/Pi.h"
#include "Simul/Base/SmartPtr.h"
#include "Simul/LicenseKey.h"
#include "LoadGLImage.h"
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"

using namespace std;
void printShaderInfoLog(GLuint sh);
void printProgramInfoLog(GLuint obj);

#ifdef WIN32
#include "Simul/Platform/Windows/VisualStudioDebugOutput.h"
#pragma comment(lib,"freeImage.lib")
#endif

using std::map;

SimulGL2DCloudRenderer::SimulGL2DCloudRenderer(simul::clouds::CloudKeyframer *ck)
	:Base2DCloudRenderer(ck)
	,texture_scale(1.f)
	,scale(2.f)
	,texture_effect(1.f)
	,cloud_data(NULL)
	,image_tex(0)
	,loss_tex(0)
	,inscatter_tex(0)
	,skylight_tex(0)
	,clouds_program(0)
	,cross_section_program(0)
	,detail_fb(0,0,GL_TEXTURE_2D)
{
	helper->Initialize(16,400000.f);
}

bool SimulGL2DCloudRenderer::CreateNoiseTexture(bool override_file)
{
	unsigned size=1024;
    glGenTextures(1,&noise_tex);
    glBindTexture(GL_TEXTURE_2D,noise_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_R,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_INT,0);
	unsigned char *data=new unsigned char[4*size*size];
	bool got_data=false;
	if(!override_file)
	{
		ifstream ifs("noise_2d_clouds",ios_base::binary);
		if(ifs.good())
		{
			int size=0,octaves=0,freq=0;
			float pers=0.f;
			ifs.read(( char*)&size,sizeof(size));
			ifs.read(( char*)&freq,sizeof(freq));
			ifs.read(( char*)&octaves,sizeof(octaves));
			ifs.read(( char*)&pers,sizeof(pers));
			if(size==noise_texture_size&&freq==noise_texture_frequency&&octaves==texture_octaves&&pers==texture_persistence)
			{
				ifs.read(( char*)data,noise_texture_size*noise_texture_size*sizeof(unsigned));
				got_data=true;
			}
		}
	}
	if(!got_data)
	{
		simul::clouds::TextureGenerator::SetBits((unsigned)255<<24,(unsigned)255<<8,(unsigned)255<<16,(unsigned)255<<0,4,false);
		simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)data,size,16,8,.8f);
	}
	glTexSubImage2D(
		GL_TEXTURE_2D,0,
		0,0,
		size,size,
		GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
		data);
	gluBuild2DMipmaps(GL_TEXTURE_2D,6,size,size,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,data);
	if(!got_data)
	{
		ofstream ofs("noise_2d_clouds",ios_base::binary);
		ofs.write((const char*)&noise_texture_size		,sizeof(noise_texture_size));
		ofs.write((const char*)&noise_texture_frequency	,sizeof(noise_texture_frequency));
		ofs.write((const char*)&texture_octaves			,sizeof(texture_octaves));
		ofs.write((const char*)&texture_persistence		,sizeof(texture_persistence));
		ofs.write((const char*)data,noise_texture_size*noise_texture_size*sizeof(unsigned));
	}
	delete [] data;
	return true;
}

void SimulGL2DCloudRenderer::CreateImageTexture()
{
	image_tex=LoadGLImage("Cirrocumulus.png",GL_REPEAT);
	FramebufferGL	noise_fb(16,16,GL_TEXTURE_2D);
	noise_fb.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT,GL_REPEAT);
	noise_fb.Activate();
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,1.0,0,1.0,-1.0,1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		GLuint noise_prog=LoadPrograms("simple.vert",NULL,"simul_noise.frag");
		glUseProgram(noise_prog);
		DrawQuad(0,0,1,1);
		SAFE_DELETE_PROGRAM(noise_prog);
	}
	noise_fb.Deactivate();
	glUseProgram(0);
ERROR_CHECK	

	FramebufferGL dens_fb(512,512,GL_TEXTURE_2D);

	dens_fb.SetWidthAndHeight(512,512);
	dens_fb.InitColor_Tex(0,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,GL_REPEAT);
	dens_fb.Activate();
	{
		dens_fb.Clear(0.f,0.f,0.f,0.f);
		Ortho();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,noise_fb.GetColorTex());
		GLuint dens_prog=LoadPrograms("simple.vert",NULL,"simul_2d_cloud_detail.frag");
		glUseProgram(dens_prog);
		//GLint fadeTexture2_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture2");
		//glUniform1f(skyInterp_fade,skyKeyframer->GetInterpolation());
		DrawQuad(0,0,1,1);
		SAFE_DELETE_PROGRAM(dens_prog);
	}
	dens_fb.Deactivate();
	glUseProgram(0);
	

	detail_fb.SetWidthAndHeight(512,512);
	detail_fb.InitColor_Tex(0,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,GL_REPEAT);
	detail_fb.Activate();
	{
		detail_fb.Clear(0.f,0.f,0.f,0.f);
		Ortho();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,dens_fb.GetColorTex());
		GLuint lighting_prog=LoadPrograms("simple.vert",NULL,"simul_2d_cloud_detail_lighting.frag");
		glUseProgram(lighting_prog);
		GLint lightDir		=glGetUniformLocation(lighting_prog,"lightDir");
		glUniform3f(lightDir,1.f,0.f,0.f);
		DrawQuad(0,0,1,1);
		SAFE_DELETE_PROGRAM(lighting_prog);
	}
	detail_fb.Deactivate();
	glUseProgram(0);
	
	SAFE_DELETE_TEXTURE(noise_tex);
}

void SimulGL2DCloudRenderer::EnsureCorrectTextureSizes()
{
	simul::clouds::CloudKeyframer::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	if(cloud_tex_width_x==width_x&&cloud_tex_length_y==length_y&&cloud_tex_depth_z==1
		&&coverage_tex[0]>0)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=1;

	for(int i=0;i<3;i++)
	{
		glGenTextures(1,&(coverage_tex[i]));

		glBindTexture(GL_TEXTURE_2D,coverage_tex[i]);
		if(sizeof(simul::clouds::CloudTexelType)==sizeof(GLushort))
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA4,width_x,length_y,0,GL_RGBA,GL_UNSIGNED_SHORT,0);
		else if(sizeof(simul::clouds::CloudTexelType)==sizeof(GLuint))
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width_x,length_y,0,GL_RGBA,GL_UNSIGNED_INT,0);

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		if(GetCloudInterface()->GetWrap())
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		}
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
	}
// lighting is done in CreateCloudTexture, so memory has now been allocated
	unsigned cloud_mem=cloudKeyframer->GetMemoryUsage();
	std::cout<<"Cloud memory usage: "<<cloud_mem/1024<<"k"<<std::endl;

}
void SimulGL2DCloudRenderer::EnsureTexturesAreUpToDate()
{
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	typedef simul::clouds::CloudKeyframer::block_texture_fill iter;
	for(int i=0;i<3;i++)
	{
		if(!coverage_tex[i])
			continue;
		iter texture_fill;
		while((texture_fill=cloudKeyframer->GetBlockTextureFill(seq_texture_iterator[i])).w!=0)
		{
			if(!texture_fill.w||!texture_fill.l||!texture_fill.d)
				break;
			glBindTexture(GL_TEXTURE_2D,coverage_tex[i]);
ERROR_CHECK
			if(sizeof(simul::clouds::CloudTexelType)==sizeof(GLushort))
			{
				unsigned short *uint16_array=(unsigned short *)texture_fill.uint32_array;
				glTexSubImage2D(	GL_TEXTURE_2D,0,
									texture_fill.x,texture_fill.y,
									texture_fill.w,texture_fill.l,
									GL_RGBA,GL_UNSIGNED_SHORT_4_4_4_4,
									uint16_array);
ERROR_CHECK
			}
			else if(sizeof(simul::clouds::CloudTexelType)==sizeof(GLuint))
			{
				glTexSubImage2D(	GL_TEXTURE_2D,0,
									texture_fill.x,texture_fill.y,
									texture_fill.w,texture_fill.l,
									GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
									texture_fill.uint32_array);
ERROR_CHECK
			}
			//seq_texture_iterator[i].texel_index+=texture_fill.w*texture_fill.l*texture_fill.d;
		}
	}
}

void SimulGL2DCloudRenderer::EnsureTextureCycle()
{
	int cyc=(cloudKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(coverage_tex[0],coverage_tex[1]);
		std::swap(coverage_tex[1],coverage_tex[2]);
		std::swap(seq_texture_iterator[0],seq_texture_iterator[1]);
		std::swap(seq_texture_iterator[1],seq_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}
	
void SimulGL2DCloudRenderer::SetCloudTextureSize(unsigned width_x,unsigned length_y)
{
	tex_width=width_x;
	delete [] cloud_data;
	cloud_data=new unsigned char[4*width_x*length_y];
	for(int i=0;i<3;i++)
	{
		glGenTextures(1,&(coverage_tex[i]));
		glBindTexture(GL_TEXTURE_2D,coverage_tex[i]);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA4,width_x,length_y,0,GL_RGBA,GL_UNSIGNED_SHORT,0);

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	}
}

void SimulGL2DCloudRenderer::FillCloudTextureBlock(
	int texture_index,int x,int y,int w,int l,const unsigned *uint32_array)
{
	glBindTexture(GL_TEXTURE_2D,coverage_tex[texture_index]);
	{
		glTexSubImage2D(	GL_TEXTURE_2D,0,
							x,y,
							w,l,
							GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
							uint32_array);
	}
}

static void glGetMatrix(GLfloat *m,GLenum src=GL_PROJECTION_MATRIX)
{
	glGetFloatv(src,m);
}

bool SimulGL2DCloudRenderer::Render(bool, bool, bool, bool)
{
	EnsureTexturesAreUpToDate();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,image_tex);
	using namespace simul::clouds;
	if(skyInterface)
		cloudKeyframer->Update(skyInterface->GetTime());
	CloudInterface *ci=cloudKeyframer->GetCloudInterface();
	simul::math::Vector3 X1,X2;
	ci->GetExtents(X1,X2);
	simul::math::Vector3 DX=X2-X1;
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,detail_fb.GetColorTex());
ERROR_CHECK
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,coverage_tex[0]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,coverage_tex[1]);
    glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,loss_tex);
    glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D,inscatter_tex);
    glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D,skylight_tex);
	glUseProgram(clouds_program);
	glUniform1f(cloudInterp,cloudKeyframer->GetInterpolation());
	glUniform1f(maxFadeDistanceMetres_param,max_fade_distance_metres);
	glUniform1i(imageTexture_param,0);
	glUniform1i(coverageTexture1,1);
	glUniform1i(coverageTexture2,2);
	glUniform1i(lossSampler_param,3);
	glUniform1i(inscatterSampler_param,4);
	glUniform1i(skylightSampler_param,5);
static float ll=0.05f;
	glUniform4f(lightResponse_param,ci->GetLightResponse(),0,0,ll*ci->GetSecondaryLightResponse());
ERROR_CHECK
	glUniform3f(fractalScale_param,ci->GetFractalOffsetScale()/DX.x,
									ci->GetFractalOffsetScale()/DX.y,
									ci->GetFractalOffsetScale()/DX.z);
ERROR_CHECK
	glUniform1f(interp_param,cloudKeyframer->GetInterpolation());
ERROR_CHECK
	if(skyInterface)
	{
		simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(X1.z*0.001f);
		glUniform3f(sunlightColour_param,sunlight.x,sunlight.y,sunlight.z);
		glUniform1f(hazeEccentricity_param,skyInterface->GetMieEccentricity());
		simul::sky::float4 mieRayleighRatio=skyInterface->GetMieRayleighRatio();
		glUniform3f(mieRayleighRatio_param,mieRayleighRatio.x,mieRayleighRatio.y,mieRayleighRatio.z);
		simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight(X1.z*0.001f);
		glUniform3f(lightDir_param,sun_dir.x,sun_dir.y,sun_dir.z);
		simul::sky::float4 amb=skyInterface->GetAmbientLight(X1.z*.001f);
		glUniform3f(skylightColour_param,amb.x,amb.y,amb.z);
	}

	glUniform1f(cloudEccentricity_param,cloudKeyframer->GetInterpolatedKeyframe().light_asymmetry);
ERROR_CHECK
	glUniform1f(textureEffect_param,texture_effect);
ERROR_CHECK
	glUniform1f(layerDensity_param,DX.z*0.001f*ci->GetOpticalDensity());

	FixGlProjectionMatrix(helper->GetMaxCloudDistance()*1.1f);
	simul::math::Matrix4x4 modelview,proj;
	glGetMatrix((float*)&modelview,GL_MODELVIEW_MATRIX);
	glGetMatrix((float*)&proj,GL_PROJECTION_MATRIX);

ERROR_CHECK
	simul::math::Matrix4x4 viewInv;
	modelview.Inverse(viewInv);
	simul::math::Vector3 cam_pos(viewInv(3,0),viewInv(3,1),viewInv(3,2));

	glUniform3f(eyePosition_param,cam_pos.x,cam_pos.y,cam_pos.z);

	simul::math::Vector3 view_pos(cam_pos.x,cam_pos.y,cam_pos.z);

ERROR_CHECK
	simul::math::Vector3 eye_dir=viewInv.RowPointer(2);
	eye_dir*=-1.f;
	simul::math::Vector3 up_dir=viewInv.RowPointer(1);
	helper->Update(view_pos,ci->GetWindOffset(),eye_dir,up_dir);
	helper->Make2DGeometry(ci);
	static float noise_angle=0.8f;
	helper->Set2DNoiseTexturing(noise_angle,2.f,1.f);
	float image_scale=2000.f+texture_scale*20000.f;
	simul::math::Vector3 wind_offset=cloudKeyframer->GetCloudInterface()->GetWindOffset();

ERROR_CHECK
	const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
	size_t qs_vert=0;
	for(std::vector<Cloud2DGeometryHelper::QuadStrip>::const_iterator j=helper->GetQuadStrips().begin();
		j!=helper->GetQuadStrips().end();j++)
	{
		glBegin(GL_QUAD_STRIP);
		for(size_t k=0;k<(j)->num_vertices;k++,qs_vert++)
		{
			const Cloud2DGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
			glMultiTexCoord2f(GL_TEXTURE0,V.cloud_tex_x,V.cloud_tex_y);
			glMultiTexCoord2f(GL_TEXTURE3,V.noise_tex_x,V.noise_tex_y);
			glMultiTexCoord2f(GL_TEXTURE4,(V.x+wind_offset.x)/image_scale,(V.y+wind_offset.y)/image_scale);
			
			glVertex3f(V.x,V.y,V.z);
		}
		glEnd();
	}
ERROR_CHECK
   // glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_2D,0);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
	glUseProgram(0);
ERROR_CHECK
	return true;
}

void SimulGL2DCloudRenderer::RenderCrossSections(int width,int height)
{
	static int u=4;
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
	glEnable(GL_TEXTURE_2D);
	glUseProgram(cross_section_program);
ERROR_CHECK
static float mult=1.f;
	glUniform1i(cloudDensity1_param,0);
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				dynamic_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		simul::sky::float4 light_response(mult*kf->direct_light,mult*kf->indirect_light,mult*kf->ambient_light,0);

	ERROR_CHECK
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,coverage_tex[i]);
		glUniform1f(crossSectionOffset,GetCloudInterface()->GetWrap()?0.5f:0.f);
		glUniform4f(lightResponse_param,light_response.x,light_response.y,light_response.z,light_response.w);
		DrawQuad(i*(w+8)+8,h+16,w,w);
	}
	glUseProgram(0);	
}

void SimulGL2DCloudRenderer::SetLossTexture(void *l)
{
	if(l)
	loss_tex=((GLuint)l);
}

void SimulGL2DCloudRenderer::SetInscatterTextures(void *i,void *s)
{
	inscatter_tex=((GLuint)i);
	skylight_tex=((GLuint)s);
}

void SimulGL2DCloudRenderer::RestoreDeviceObjects(void*)
{
	CreateNoiseTexture();
ERROR_CHECK
	RecompileShaders();
ERROR_CHECK
}

void SimulGL2DCloudRenderer::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(clouds_program);
	clouds_program			=MakeProgram("simul_clouds_2d");
	glUseProgram(clouds_program);

	coverageTexture1		= glGetUniformLocation(clouds_program,"coverageTexture1");
	coverageTexture2		= glGetUniformLocation(clouds_program,"coverageTexture2");
	cloudInterp				= glGetUniformLocation(clouds_program,"cloudInterp");
	
	imageTexture_param		= glGetUniformLocation(clouds_program,"imageTexture");
	lightResponse_param		= glGetUniformLocation(clouds_program,"lightResponse");
	fractalScale_param		= glGetUniformLocation(clouds_program,"fractalScale");
	interp_param			= glGetUniformLocation(clouds_program,"interp");
	eyePosition_param		= glGetUniformLocation(clouds_program,"eyePosition");
	skylightColour_param	= glGetUniformLocation(clouds_program,"ambientColour");
	lightDir_param			= glGetUniformLocation(clouds_program,"lightDir");
	sunlightColour_param	= glGetUniformLocation(clouds_program,"sunlight");

	textureEffect_param		= glGetUniformLocation(clouds_program,"textureEffect");
	layerDensity_param		= glGetUniformLocation(clouds_program,"layerDensity");
	lossSampler_param		= glGetUniformLocation(clouds_program,"lossSampler");
	inscatterSampler_param	= glGetUniformLocation(clouds_program,"inscatterSampler");
	skylightSampler_param	= glGetUniformLocation(clouds_program,"skylightSampler");

	cloudEccentricity_param	= glGetUniformLocation(clouds_program,"cloudEccentricity");
	hazeEccentricity_param	= glGetUniformLocation(clouds_program,"hazeEccentricity");
	mieRayleighRatio_param	= glGetUniformLocation(clouds_program,"mieRayleighRatio");
	maxFadeDistanceMetres_param	= glGetUniformLocation(clouds_program,"maxFadeDistanceMetres");

//cloudKeyframer->SetRenderCallback(this);
	glUseProgram(0);
	
	SAFE_DELETE_PROGRAM(cross_section_program);
	cross_section_program=MakeProgram("simple");
	CreateImageTexture();
ERROR_CHECK
}

void SimulGL2DCloudRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(clouds_program);
	SAFE_DELETE_PROGRAM(cross_section_program);
	clouds_program			=0;
	lightResponse_param		=0;
	fractalScale_param		=0;
	interp_param			=0;
	eyePosition_param		=0;
	skylightColour_param	=0;
	lightDir_param			=0;
	sunlightColour_param	=0;
	textureEffect_param		=0;
	layerDensity_param		=0;
	lossSampler_param		=0;
	inscatterSampler_param	=0;
	image_tex				=0;
}

bool SimulGL2DCloudRenderer::Create()
{
// lighting is done in CreateCloudTexture, so memory has now been allocated
	unsigned cloud_mem=cloudKeyframer->GetMemoryUsage();
	std::cout<<"Cloud memory usage: "<<cloud_mem/1024<<"k"<<std::endl;
	return true;
}

SimulGL2DCloudRenderer::~SimulGL2DCloudRenderer()
{
	InvalidateDeviceObjects();
}