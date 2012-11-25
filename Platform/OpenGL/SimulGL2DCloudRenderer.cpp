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
		simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)data,size,16,8,.8);
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

bool SimulGL2DCloudRenderer::CreateImageTexture()
{
	image_tex=LoadGLImage("Cirrocumulus.png",GL_REPEAT);

#if 0 //def WIN32
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	char lpszPathName[256]="Cirrus2.jpg";
	fif = FreeImage_GetFileType(lpszPathName, 0);
	if(fif == FIF_UNKNOWN)
	{
		// no signature ?
		// try to guess the file format from the file extension
		fif = FreeImage_GetFIFFromFilename(lpszPathName);
	}
	// check that the plugin has reading capabilities ...
	if((fif == FIF_UNKNOWN) ||!FreeImage_FIFSupportsReading(fif))
		return false;

	
		// ok, let's load the file
	FIBITMAP *dib = FreeImage_Load(fif,lpszPathName);
	
	unsigned	width  = FreeImage_GetWidth(dib),
				height = FreeImage_GetHeight(dib);

	unsigned bpp=FreeImage_GetBPP(dib);
	if(bpp!=24)
		return false;

    glGenTextures(1,&image_tex);
    glBindTexture(GL_TEXTURE_2D,image_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_R,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
		BYTE *pixels = (BYTE*)FreeImage_GetBits(dib);
    glTexImage2D(GL_TEXTURE_2D,0, GL_RGB8,width,height,0,GL_RGB,GL_UNSIGNED_BYTE,
		pixels);
#endif
	return true;
}

void SimulGL2DCloudRenderer::SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	tex_width=width_x;
	delete [] cloud_data;
	cloud_data=new unsigned char[4*width_x*length_y*depth_z];
	for(int i=0;i<3;i++)
	{
		glGenTextures(1,&(cloud_tex[i]));
		glBindTexture(GL_TEXTURE_2D,cloud_tex[i]);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA4,width_x,length_y,0,GL_RGBA,GL_UNSIGNED_SHORT,0);

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	}
}

void SimulGL2DCloudRenderer::FillCloudTextureBlock(
	int texture_index,int x,int y,int ,int w,int l,int d,const unsigned *uint32_array)
{
	glBindTexture(GL_TEXTURE_3D,cloud_tex[texture_index]);
	{
		glTexSubImage2D(	GL_TEXTURE_3D,0,
							x,y,
							w,l,
							GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
							uint32_array);
	}
}

void SimulGL2DCloudRenderer::CycleTexturesForward()
{
	std::swap(cloud_tex[0],cloud_tex[1]);
	std::swap(cloud_tex[1],cloud_tex[2]);
}

static void glGetMatrix(GLfloat *m,GLenum src=GL_PROJECTION_MATRIX)
{
	glGetFloatv(src,m);
}

bool SimulGL2DCloudRenderer::Render(bool, bool, bool)
{
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
    glDisable(GL_TEXTURE_3D);
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,image_tex);
ERROR_CHECK
    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,loss_tex);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,inscatter_tex);
    glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,skylight_tex);
	glUseProgram(clouds_program);
	glUniform1f(maxFadeDistanceMetres_param,max_fade_distance_metres);
	glUniform1i(imageTexture_param,0);
	glUniform1i(lossSampler_param,1);
	glUniform1i(inscatterSampler_param,2);
	glUniform1i(skylightSampler_param,3);
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
		simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();
		glUniform3f(lightDir_param,sun_dir.x,sun_dir.y,sun_dir.z);
		simul::sky::float4 amb=skyInterface->GetAmbientLight(cam_pos.z*.001f);
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
	CreateImageTexture();
ERROR_CHECK
	RecompileShaders();
ERROR_CHECK
}

void SimulGL2DCloudRenderer::RecompileShaders()
{
	clouds_vertex_shader	=glCreateShader(GL_VERTEX_SHADER);
	clouds_fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);

	clouds_program			=glCreateProgram();
    clouds_vertex_shader	=LoadShader(clouds_vertex_shader,"simul_clouds_2d.vert");
	clouds_fragment_shader	=LoadShader(clouds_fragment_shader,"simul_clouds_2d.frag");
	glAttachShader(clouds_program, clouds_vertex_shader);
	glAttachShader(clouds_program, clouds_fragment_shader);
	glLinkProgram(clouds_program);
	glUseProgram(clouds_program);
	printProgramInfoLog(clouds_program);
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
}

void SimulGL2DCloudRenderer::InvalidateDeviceObjects()
{
	clouds_vertex_shader	=0;
	clouds_fragment_shader	=0;
	clouds_program			=0;
    clouds_vertex_shader	=0;
	clouds_fragment_shader	=0;
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