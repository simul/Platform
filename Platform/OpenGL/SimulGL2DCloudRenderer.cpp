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
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Sky/SkyNode.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Simul/Math/Pi.h"
#include "Simul/Base/SmartPtr.h"
#include "Simul/LicenseKey.h"
#include "LoadGLProgram.h"

using namespace std;
void printShaderInfoLog(GLuint obj);
void printProgramInfoLog(GLuint obj);

#ifdef WIN32
#include "Simul/Platform/Windows/VisualStudioDebugOutput.h"
#pragma comment(lib,"freeImage.lib")
#endif


static simul::base::SmartPtr<simul::clouds::FastCloudNode> cloudNode;
using std::map;


SimulGL2DCloudRenderer::SimulGL2DCloudRenderer()
	: texture_scale(1.f)
	, scale(2.f)
	, texture_effect(1.f)
	, cloud_data(NULL)
{
	cloudKeyframer->SetFillTexturesAsBlocks(true);
}

bool SimulGL2DCloudRenderer::CreateNoiseTexture()
{
	unsigned size=1024;
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
	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)data,size,16,8,.8);
	glTexSubImage2D(
		GL_TEXTURE_2D,0,
		0,0,
		size,size,
		GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
		data);
	gluBuild2DMipmaps(GL_TEXTURE_2D,6,size,size,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,data);
	delete [] data;
	return true;
}

bool SimulGL2DCloudRenderer::CreateImageTexture()
{
#ifdef WIN32
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
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
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

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	}
}

void SimulGL2DCloudRenderer::FillCloudTextureBlock(
	int texture_index,int x,int y,int z,int w,int l,int d,const unsigned *uint32_array)
{
	glBindTexture(GL_TEXTURE_3D,cloud_tex[texture_index]);
	if(cloudKeyframer->GetUse16Bit())
	{
		unsigned short *uint16_array=(unsigned short *)uint32_array;
		glTexSubImage2D(	GL_TEXTURE_3D,0,
							x,y,
							w,l,
							GL_RGBA,GL_UNSIGNED_SHORT_4_4_4_4,
							uint16_array);
	}
	else
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
	using namespace simul::clouds;
	cloudKeyframer->Update(skyInterface->GetTime());
	CloudInterface *ci=cloudNode.get();
	simul::math::Vector3 X1,X2;
	ci->GetExtents(X1,X2);
	simul::math::Vector3 DX=X2-X1;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

	glUseProgram(clouds_program);
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,cloud_tex[0]);
    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,cloud_tex[1]);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,noise_tex);
    glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,image_tex);
static float ll=0.05f;
	glUniform4f(lightResponse_param,ll*ci->GetLightResponse(),0,0,ll*ci->GetSecondaryLightResponse());
	glUniform3f(fractalScale_param,ci->GetFractalOffsetScale()/DX.x,
									ci->GetFractalOffsetScale()/DX.y,
									ci->GetFractalOffsetScale()/DX.z);
	glUniform1f(interp_param,cloudKeyframer->GetInterpolation());
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(X1.z*0.001f);
	glUniform3f(sunlightColour_param,sunlight.x,sunlight.y,sunlight.z);

	glUniform1f(textureEffect_param,texture_effect);
	glUniform1f(layerDensity_param,DX.z*0.001f*ci->GetOpticalDensity());


	simul::math::Matrix4x4 modelview,proj;
	glGetMatrix((float*)&modelview,GL_MODELVIEW_MATRIX);
	glGetMatrix((float*)&proj,GL_PROJECTION_MATRIX);

	simul::math::Matrix4x4 viewInv;
	viewInv.Inverse(modelview);
	//nv::matrix4f viewInv=inverse(modelview);
	//nv::vec4f nv_cam_pos(viewInv._41,viewInv._42,viewInv._43)
	simul::math::Vector3 cam_pos(viewInv(3,0),viewInv(3,1),viewInv(3,2));

	glUniform3f(eyePosition_param,cam_pos.x,cam_pos.y,cam_pos.z);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();
	glUniform3f(lightDir_param,sun_dir.x,sun_dir.y,sun_dir.z);

	simul::sky::float4 amb=skyInterface->GetSkyColour(simul::sky::float4(0,0,1.f,0),
		cam_pos.z*.001f);
	glUniform3f(skylightColour_param,amb.x,amb.y,amb.z);

	simul::math::Vector3 view_pos(cam_pos.x,cam_pos.y,cam_pos.z);

	simul::math::Vector3 eye_dir=viewInv.RowPointer(2);eye_dir*=-1.f;//(-viewInv._31,-viewInv._32,-viewInv._33);
	simul::math::Vector3 up_dir=viewInv.RowPointer(1);//(viewInv._21,viewInv._22,viewInv._23);
	
	helper->Update(view_pos,ci->GetWindOffset(),eye_dir,up_dir);
	float tan_half_fov_vertical=1.f/proj(1,1);//proj._22;
	float tan_half_fov_horizontal=1.f/proj(0,0);//proj._11;
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);

	simul::sky::float4 view_km=view_pos.FloatPointer(0);
	// convert metres to km:
	view_km*=0.001f;
	helper->MakeGeometry(ci);
	static float noise_angle=.8;
	helper->Set2DNoiseTexturing(noise_angle,2.f,1.f);
	helper->CalcInscatterFactors(cloudInterface,skyInterface,0.f);
	float image_scale=2000.f+texture_scale*20000.f;
	simul::math::Vector3 wind_offset=cloudNode->GetWindOffset();

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
			simul::sky::float4 inscatter=helper->GetInscatter(V);
			simul::sky::float4 loss		=helper->GetLoss(V);
			glMultiTexCoord3f(GL_TEXTURE1,loss.x,loss.y,loss.z);
			glMultiTexCoord3f(GL_TEXTURE2,inscatter.x,inscatter.y,inscatter.z);
			glMultiTexCoord2f(GL_TEXTURE3,V.noise_tex_x,V.noise_tex_y);
			glMultiTexCoord2f(GL_TEXTURE4,(V.x+wind_offset.x)/image_scale,(V.y+wind_offset.y)/image_scale);
			
			glVertex3f(V.x,V.y,V.z);
		}
		glEnd();
	}
	
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
	glUseProgram(NULL);
	return true;
}

bool SimulGL2DCloudRenderer::RestoreDeviceObjects(void*)
{
	clouds_vertex_shader	=glCreateShader(GL_VERTEX_SHADER);
	clouds_fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);

	clouds_program			=glCreateProgram();
    clouds_vertex_shader=LoadProgram(clouds_vertex_shader,"simul_clouds_2d.vert");
	clouds_fragment_shader=LoadProgram(clouds_fragment_shader,"simul_clouds_2d.frag");
	glAttachShader(clouds_program, clouds_vertex_shader);
	glAttachShader(clouds_program, clouds_fragment_shader);
	glLinkProgram(clouds_program);
	glUseProgram(clouds_program);
	printProgramInfoLog(clouds_program);

	lightResponse_param		= glGetUniformLocation(clouds_program,"lightResponse");
	fractalScale_param		= glGetUniformLocation(clouds_program,"fractalScale");
	interp_param			= glGetUniformLocation(clouds_program,"interp");
	eyePosition_param		= glGetUniformLocation(clouds_program,"eyePosition");
	skylightColour_param	= glGetUniformLocation(clouds_program,"ambientColour");
	lightDir_param			= glGetUniformLocation(clouds_program,"lightDir");
	sunlightColour_param	= glGetUniformLocation(clouds_program,"sunlight");

	textureEffect_param		= glGetUniformLocation(clouds_program,"textureEffect");
	layerDensity_param		= glGetUniformLocation(clouds_program,"layerDensity");
	cloudKeyframer->SetRenderCallback(this);
	return true;
}

bool SimulGL2DCloudRenderer::InvalidateDeviceObjects()
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
	return true;
}

bool SimulGL2DCloudRenderer::Create()
{
	helper=new simul::clouds::Cloud2DGeometryHelper();
	CreateNoiseTexture();
	CreateImageTexture();
	cloudInterface=cloudNode.get();
	cloudNode->SetLicense(SIMUL_LICENSE_KEY);

	cloudNode->SetSeparateSecondaryLight(true);
	cloudNode->SetWrap(true);
	cloudNode->SetThinLayer(true);

	cloudNode->SetRandomSeed(239);

	cloudNode->SetGridLength(128);
	cloudNode->SetGridWidth(128);
	cloudNode->SetGridHeight(1);

	cloudNode->SetCloudBaseZ(12600.f);
	cloudNode->SetCloudWidth(120000.f);
	cloudNode->SetCloudLength(120000.f);
	cloudNode->SetCloudHeight(4000.f);

	cloudNode->SetOpticalDensity(.5f);
	cloudNode->SetHumidity(0.65f);

	cloudNode->SetExtinction(1.5f);
	cloudNode->SetLightResponse(1.f);
	cloudNode->SetSecondaryLightResponse(1.f);
	cloudNode->SetAmbientLightResponse(1.f);
	cloudNode->SetDiffusivity(1.f);

	unsigned noise_octave=(unsigned)scale;
	unsigned noise_res=1<<(unsigned)scale;
	unsigned octaves=5-noise_octave;
	cloudNode->SetNoiseResolution(noise_res);
	cloudNode->SetNoiseOctaves(octaves);
	cloudNode->SetNoisePeriod(4);

	cloudNode->Generate();

	helper->Initialize(16,100000.f);
	cloudKeyframer=new simul::clouds::CloudKeyframer(cloudInterface,NULL,true);
	cloudKeyframer->SetOpenGL(true);
	cloudKeyframer->SetUserKeyframes(false);
// lighting is done in CreateCloudTexture, so memory has now been allocated
	unsigned cloud_mem=cloudNode->GetMemoryUsage();
	std::cout<<"Cloud memory usage: "<<cloud_mem/1024<<"k"<<std::endl;
	return true;
}

SimulGL2DCloudRenderer::~SimulGL2DCloudRenderer()
{
	InvalidateDeviceObjects();
}

void SimulGL2DCloudRenderer::SetSkyInterface(simul::sky::SkyInterface *si)
{
	skyInterface=si;
}

void SimulGL2DCloudRenderer::SetFadeTable(simul::sky::FadeTableInterface *fti)
{
	fadeTableInterface=fti;
}
