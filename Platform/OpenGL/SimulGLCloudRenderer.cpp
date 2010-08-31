#ifdef _MSC_VER
	#include <windows.h>
#endif

#ifdef WIN64
#pragma message("WIN64")
#undef WIN32
#endif
#ifdef WIN32
#pragma message("WIN32")
#endif

#include "Simul/Base/Timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <map>
#include <math.h>

#include "FreeImage.h"
#include <fstream>
#ifdef WIN64
#pragma message("WIN64")
#undef WIN32
#endif

#include "RenderTextureFBO.h"

#include "SimulGLCloudRenderer.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Simul/Math/Pi.h"
#include "Simul/Base/SmartPtr.h"
#include "Simul/LicenseKey.h"
#include "LoadGLProgram.h"


#ifdef WIN32
#include "Simul/Platform/Windows/VisualStudioDebugOutput.h"
#endif

#include <algorithm>
void printShaderInfoLog(GLuint obj);
void printProgramInfoLog(GLuint obj);

bool god_rays=false;
using std::map;
using namespace std;

float min_dist=180000.f;
float max_dist=320000.f;

simul::math::Vector3 next_sun_direction;
unsigned char *cloud_data=NULL;

SimulGLCloudRenderer::SimulGLCloudRenderer()
	: texture_scale(1.f)
	, scale(2.f)
	, texture_effect(1.f)
	, detail(1.f)
{
	cloudKeyframer->SetFillTexturesAsBlocks(true);
}

bool SimulGLCloudRenderer::CreateNoiseTexture
()
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
	delete [] tex;
}

void SimulGLCloudRenderer::CycleTexturesForward()
{
	std::swap(cloud_tex[0],cloud_tex[1]);
	std::swap(cloud_tex[1],cloud_tex[2]);
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
bool SimulGLCloudRenderer::Render(bool depth_testing)
{
	float gamma=cloudKeyframer->GetPrecalculatedGamma();
	if(gamma>0&&gamma!=1.f)
	{
		helper->EnablePrecalculatedGamma(gamma);
	}
	else
	{
		helper->DisablePrecalculatedGamma();
		gamma=1.f;
	}
	glPushAttrib(GL_ENABLE_BIT);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	using namespace simul::clouds;
	cloudKeyframer->Update(skyInterface->GetDaytime());
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
		glBlendFunc(GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE_MINUS_SRC_ALPHA);

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

	glUseProgram(clouds_program);

	glUniform1i(cloudDensity1_param,0);
	glUniform1i(cloudDensity2_param,1);
	glUniform1i(noiseSampler_param,2);

	static float ll=0.05f;
	simul::sky::float4 light_response(0,cloudInterface->GetLightResponse(),ll*cloudInterface->GetSecondaryLightResponse(),0);
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

	float tan_half_fov_vertical=1.f/proj(1,1);
	float tan_half_fov_horizontal=1.f/proj(0,0);
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	static float churn=1.f;
	helper->SetChurn(churn);
	
	helper->MakeGeometry(cloudNode.get(),god_rays,X1.z,god_rays);

	// Here we make the helper calculate loss and inscatter due to atmospherics.
	// This is an approach that calculates per-vertex atmospheric values that are then
	// passed to the shader.
	// The alternative is to genererate fade textures in the SkyRenderer,
	// then lookup those textures in the cloud shader.
	helper->CalcInscatterFactors(skyInterface,fadeTableInterface,god_rays);
	simul::sky::float4 sunlight1=skyInterface->GetLocalIrradiance(X1.z*.001f);
	simul::sky::float4 sunlight2=skyInterface->GetLocalIrradiance(X2.z*.001f);

	//sunlight1=simul::sky::Pow(sunlight1,gamma);
	//sunlight2=simul::sky::Pow(sunlight2,gamma);

	// Draw the layers of cloud from the furthest to the nearest. Each layer is a spherical shell,
	// which is drawn as a latitude-longitude sphere. But we only draw the parts that:
	// a) are in the view frustum
	//  ...and...
	// b) are in the cloud volume
	int layers_drawn=0;
	for(std::vector<CloudGeometryHelper::RealtimeSlice*>::const_iterator i=helper->GetSlices().begin();
		i!=helper->GetSlices().end();i++)
	{
		// How thick is this layer, optically speaking?
		float dens=(*i)->fadeIn;
		if(!dens)
			continue;
		layers_drawn++;
		helper->MakeLayerGeometry(cloudNode.get(),*i);
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
				simul::sky::float4 loss			=helper->GetLoss(*i,V);
				simul::sky::float4 inscatter	=helper->GetInscatter(*i,V);
				glMultiTexCoord3f(GL_TEXTURE0,V.cloud_tex_x,V.cloud_tex_y,V.cloud_tex_z);
				glMultiTexCoord2f(GL_TEXTURE1,V.noise_tex_x,V.noise_tex_y);
				glMultiTexCoord1f(GL_TEXTURE2,dens);
				float h=max(0.f,min((V.z-X1.z)/DX.z,1.f));
				simul::sky::float4 sunlight=lerp(h,sunlight1,sunlight2);
				// Here we're passing sunlight values per-vertex, loss and inscatter
				// The per-vertex sunlight allows different altitudes of cloud to have different
				// sunlight colour - good for dawn/sunset.
				// The per-vertex loss and inscatter is cheap for the pixel shader as it \
				// then doesn't need fade-texture lookups.
				glMultiTexCoord3f(GL_TEXTURE3,sunlight.x,sunlight.y,sunlight.z);
				glMultiTexCoord3f(GL_TEXTURE4,loss.x,loss.y,loss.z);
				glMultiTexCoord3f(GL_TEXTURE5,inscatter.x,inscatter.y,inscatter.z);
				glVertex3f(V.x,V.y,V.z);
			}
		}
		glEnd();
	}
	glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_BLEND);
    glUseProgram(NULL);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_2D);
	glPopAttrib();
	return true;
}

bool SimulGLCloudRenderer::RestoreDeviceObjects()
{
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

	printProgramInfoLog(clouds_program);

	// Because in this sample we are using 32-bit values in the cloud texture:
	//cloudKeyframer->SetUserKeyframes(false);
	cloudKeyframer->SetUse16Bit(false);
	using namespace simul::clouds;
	cloudKeyframer->SetBits(CloudKeyframer::DENSITY,CloudKeyframer::BRIGHTNESS,
		CloudKeyframer::SECONDARY,CloudKeyframer::AMBIENT);
	cloudKeyframer->SetRenderCallback(this);
	glUseProgram(NULL);
	return true;
}

void SimulGLCloudRenderer::SetWind(float spd,float dir_deg)
{
	cloudKeyframer->SetWindSpeed(spd);
	cloudKeyframer->SetWindHeadingDegrees(dir_deg);
	simul::clouds::CloudKeyframer::Keyframe *K=cloudKeyframer->GetNextModifiableKeyframe();
	if(K)
	{
		K->wind_speed=spd;
		K->wind_direction=dir_deg*pi/180.f;
	}
}

void SimulGLCloudRenderer::SetCloudiness(float h)
{
	simul::clouds::CloudKeyframer::Keyframe *K=cloudKeyframer->GetNextModifiableKeyframe();
	if(K)
	{
		K->cloudiness=h;
	}
}

bool SimulGLCloudRenderer::Create()
{
	cloudKeyframer->SetOpenGL(true);
	cloudNode->SetLicense(SIMUL_LICENSE_KEY);
	helper=new simul::clouds::CloudGeometryHelper();
	CreateNoiseTexture();

	cloudNode->SetSeparateSecondaryLight(true);
	cloudNode->SetWrap(true);

	cloudNode->SetRandomSeed(1);

	cloudNode->SetGridLength(128);
	cloudNode->SetGridWidth(128);
	cloudNode->SetGridHeight(16);

	cloudNode->SetCloudBaseZ(1100.f);
	cloudNode->SetCloudWidth(30000.f);
	cloudNode->SetCloudLength(30000.f);
	cloudNode->SetCloudHeight(2000.f);

	cloudNode->SetOpticalDensity(.4f);

	cloudNode->SetExtinction(.27f);
	cloudNode->SetLightResponse(.5f);
	cloudNode->SetSecondaryLightResponse(.5f);
	cloudNode->SetAmbientLightResponse(.5f);

	cloudInterface->SetNoiseResolution(8);
	cloudInterface->SetNoiseOctaves(3);
	cloudInterface->SetNoisePersistence(.7f);

	cloudInterface->SetNoisePeriod(1.f);

//   float gen_start_time=glutGet(GLUT_ELAPSED_TIME) / 1000.0;
	cloudNode->Generate();
	//float gen_end_time=glutGet(GLUT_ELAPSED_TIME) / 1000.0;
	//std::cout<<"Generate time: "<<(gen_end_time-gen_start_time)<<" milliseconds"<<std::endl;

	helper->Initialize((unsigned)(120.f*detail),min_dist+(max_dist-min_dist)*detail);
	unsigned el_grid=24;
	unsigned az_grid=15;
	helper->SetGrid(el_grid,az_grid);
	helper->SetCurvedEarth(true);
// lighting is done in CreateCloudTexture, so memory has now been allocated
	unsigned cloud_mem=cloudNode->GetMemoryUsage();
	std::cout<<"Cloud memory usage: "<<cloud_mem/1024<<"k"<<std::endl;
	// Try to use Threading Building Blocks?
#ifdef _MSC_VER
	cloudInterface->SetUseTbb(true);
#else
	cloudInterface->SetUseTbb(false);
#endif
	return true;
}

void SimulGLCloudRenderer::SetDetail(float d)
{
	if(d!=detail)
	{
		detail=d;
		helper->Initialize((unsigned)(120.f*detail),min_dist+(max_dist-min_dist)*detail);
	}
}

simul::sky::OvercastCallback *SimulGLCloudRenderer::GetOvercastCallback()
{
	return cloudKeyframer.get();
}

bool SimulGLCloudRenderer::Destroy()
{
	return true;
}
SimulGLCloudRenderer::~SimulGLCloudRenderer()
{
	Destroy();
}

void SimulGLCloudRenderer::SetSkyInterface(simul::sky::BaseSkyInterface *si)
{
	skyInterface=si;
	cloudKeyframer->SetSkyInterface(si);
}

void SimulGLCloudRenderer::SetFadeTable(simul::sky::FadeTableInterface *fti)
{
	fadeTableInterface=fti;
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