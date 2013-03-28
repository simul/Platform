#ifdef _MSC_VER
	#include <windows.h>
#endif
#include <GL/glew.h>
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
#include "Simul/Platform/OpenGL/Glsl.h"
#include "Simul/Platform/CrossPlatform/simul_2d_clouds.sl"

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
	,loss_tex(0)
	,inscatter_tex(0)
	,skylight_tex(0)
	,clouds_program(0)
	,cross_section_program(0)
	,cloud2DConstants(0)
	,cloud2DConstantsUBO(0)
	,cloud2DConstantsBindingIndex(2)
	,detail_fb(0,0,GL_TEXTURE_2D)
{
	helper->Initialize(16,400000.f);
}

bool SimulGL2DCloudRenderer::CreateNoiseTexture(bool override_file)
{
	//image_tex=LoadGLImage("Cirrocumulus.png",GL_REPEAT);
	FramebufferGL	noise_fb(16,16,GL_TEXTURE_2D);
	noise_fb.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT,GL_REPEAT);
	noise_fb.Activate();
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,1.0,0,1.0,-1.0,1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		GLuint noise_prog=MakeProgram("simple.vert",NULL,"simul_noise.frag");
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
		glBindTexture(GL_TEXTURE_2D,(GLuint)noise_fb.GetColorTex());
		GLuint dens_prog=MakeProgram("simple.vert",NULL,"simul_2d_cloud_detail.frag");
		glUseProgram(dens_prog);
		dens_fb.DrawQuad();
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
		glBindTexture(GL_TEXTURE_2D,(GLuint)dens_fb.GetColorTex());
		GLuint lighting_prog=MakeProgram("simple.vert",NULL,"simul_2d_cloud_detail_lighting.frag");
		glUseProgram(lighting_prog);
		GLint densTexture	=glGetUniformLocation(lighting_prog,"dens_texture");
		GLint lightDir		=glGetUniformLocation(lighting_prog,"lightDir");
		glUniform1i(densTexture,0);
		glUniform3f(lightDir,1.f,0.f,0.f);
		DrawQuad(0,0,1,1);
		SAFE_DELETE_PROGRAM(lighting_prog);
	}
	detail_fb.Deactivate();
	glUseProgram(0);
	return true;
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

void Set2DTexture(GLint shader_param,GLuint gl_texture,int channel)
{
	glActiveTexture(GL_TEXTURE0+channel);
	glBindTexture(GL_TEXTURE_2D,gl_texture);
	glUniform1i(shader_param,channel);
ERROR_CHECK
}

bool SimulGL2DCloudRenderer::Render(bool, void *, bool, bool)
{
	EnsureTexturesAreUpToDate();

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

	glUseProgram(clouds_program);
	Set2DTexture(imageTexture_param,(GLuint)detail_fb.GetColorTex(),0);
	Set2DTexture(coverageTexture1,coverage_tex[0],1);
	Set2DTexture(coverageTexture2,coverage_tex[1],2);
	Set2DTexture(lossTexture,loss_tex,3);
	Set2DTexture(inscatterSampler_param,inscatter_tex,4);
	Set2DTexture(skylightSampler_param,skylight_tex,5);
	
	simul::math::Vector3 wind_offset=cloudKeyframer->GetCloudInterface()->GetWindOffset();

	FixGlProjectionMatrix(helper->GetMaxCloudDistance()*1.1f);
	simul::math::Matrix4x4 modelview,proj;
	glGetMatrix((float*)&modelview,GL_MODELVIEW_MATRIX);
	glGetMatrix((float*)&proj,GL_PROJECTION_MATRIX);
	simul::math::Matrix4x4 viewInv;
	modelview.Inverse(viewInv);
	cam_pos.Set(viewInv(3,0),viewInv(3,1),viewInv(3,2),0.f);
static float ll=0.05f;
static float ff=100.f;
setMatrixTranspose(clouds_program,"projection_matrix",proj);
setMatrixTranspose(clouds_program,"modelview_matrix"	,modelview);
	Cloud2DConstants cloud2DConstants;
	cloud2DConstants.origin				=wind_offset;
	cloud2DConstants.globalScale		=ci->GetCloudWidth();
	cloud2DConstants.detailScale		=ff*ci->GetFractalWavelength();
	glUniform1f(globalScale				,ci->GetCloudWidth());
	glUniform1f(detailScale				,ff*ci->GetFractalWavelength());
	glUniform2f(origin					,wind_offset.x,wind_offset.y);
ERROR_CHECK
	cloud2DConstants.cloudEccentricity	=cloudKeyframer->GetInterpolatedKeyframe().light_asymmetry;
	cloud2DConstants.cloudInterp		=cloudKeyframer->GetInterpolation();
	cloud2DConstants.eyePosition		=cam_pos;
	if(skyInterface)
	{
		simul::sky::float4 sunlight			=skyInterface->GetLocalIrradiance(X1.z*0.001f);
		simul::sky::float4 mieRayleighRatio	=skyInterface->GetMieRayleighRatio();
		simul::sky::float4 sun_dir			=skyInterface->GetDirectionToLight(X1.z*0.001f);
		simul::sky::float4 amb				=skyInterface->GetAmbientLight(X1.z*.001f);
	
		cloud2DConstants.hazeEccentricity		=skyInterface->GetMieEccentricity();
		cloud2DConstants.lightDir				=sun_dir;
		cloud2DConstants.lightResponse			=simul::sky::float4(ci->GetLightResponse(),0,0,ll*ci->GetSecondaryLightResponse());
		cloud2DConstants.maxFadeDistanceMetres	=max_fade_distance_metres;
		cloud2DConstants.mieRayleighRatio		=mieRayleighRatio;
		cloud2DConstants.sunlight				=sunlight;
	}
	glBindBuffer(GL_UNIFORM_BUFFER, cloud2DConstantsUBO);
	glBufferSubData(GL_UNIFORM_BUFFER,0, sizeof(Cloud2DConstants), &cloud2DConstants);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
ERROR_CHECK
	glBindBufferBase(GL_UNIFORM_BUFFER,cloud2DConstantsBindingIndex,cloud2DConstantsUBO);
ERROR_CHECK
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
	static int u=8;
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
//GLint yz_param				= glGetUniformLocation(cross_section_program,"yz");
	GLint crossSectionOffset	= glGetUniformLocation(cross_section_program,"crossSectionOffset");

    glDisable(GL_BLEND);
(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
ERROR_CHECK
	glEnable(GL_TEXTURE_2D);
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
		glBindTexture(GL_TEXTURE_2D,coverage_tex[i]);
		glUniform1f(crossSectionOffset,GetCloudInterface()->GetWrap()?0.5f:0.f);
		glUniform4f(lightResponse_param,light_response.x,light_response.y,light_response.z,light_response.w);
		DrawQuad((i+1)*(w+8)+8,height-w-8,w,w);
	}
	
	glBindTexture(GL_TEXTURE_2D,(GLuint)detail_fb.GetColorTex());
	glUseProgram(Utilities::GetSingleton().simple_program);
	DrawQuad(8,height-8-w,w,w);
	
	glUseProgram(0);	
}

void SimulGL2DCloudRenderer::SetLossTexture(void *l)
{
	if(l)
	loss_tex=((GLuint)l);
}
static GLint earthShadowUniformsBindingIndex=3;

void SimulGL2DCloudRenderer::SetInscatterTextures(void *i,void *s)
{
	inscatter_tex=((GLuint)i);
	skylight_tex=((GLuint)s);
}

void SimulGL2DCloudRenderer::RestoreDeviceObjects(void*)
{
	CreateNoiseTexture();
	
	glGenBuffers(1, &cloud2DConstantsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, cloud2DConstantsUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(Cloud2DConstants), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	
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
	imageTexture_param		= glGetUniformLocation(clouds_program,"imageTexture");
	lossTexture				= glGetUniformLocation(clouds_program,"lossTexture");
	inscatterSampler_param	= glGetUniformLocation(clouds_program,"inscTexture");
	skylightSampler_param	= glGetUniformLocation(clouds_program,"skylTexture");

	globalScale				= glGetUniformLocation(clouds_program,"globalScale");
	detailScale				= glGetUniformLocation(clouds_program,"detailScale");
	origin					= glGetUniformLocation(clouds_program,"origin");
	
	cloud2DConstants		=glGetUniformBlockIndex(clouds_program,"Cloud2DConstants");
	// If that block IS in the shader program, then BIND it to the relevant UBO.
	if(cloud2DConstants>=0)
		glUniformBlockBinding(clouds_program,cloud2DConstants,cloud2DConstantsBindingIndex);
	glBindBufferRange(GL_UNIFORM_BUFFER,cloud2DConstantsBindingIndex,cloud2DConstantsUBO,0, sizeof(Cloud2DConstants));
	
	earthShadowUniforms		=glGetUniformBlockIndex(clouds_program, "EarthShadowUniforms");
	if(earthShadowUniforms>=0)
	{
		glUniformBlockBinding(clouds_program,earthShadowUniforms,earthShadowUniformsBindingIndex);
	}
//cloudKeyframer->SetRenderCallback(this);
	glUseProgram(0);
	
	SAFE_DELETE_PROGRAM(cross_section_program);
	cross_section_program=MakeProgram("simple");

//	CreateImageTexture();
ERROR_CHECK
}

void SimulGL2DCloudRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(clouds_program);
	SAFE_DELETE_PROGRAM(cross_section_program);
	clouds_program			=0;
	lossTexture		=0;
	inscatterSampler_param	=0;
	
	glDeleteBuffersARB(1,&cloud2DConstantsUBO);
	cloud2DConstants=-1;
	cloud2DConstantsUBO=-1;
	
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