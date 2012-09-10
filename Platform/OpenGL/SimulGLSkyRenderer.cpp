

#include "Simul/Base/Timer.h"
#include <stdio.h>
#include <math.h>

#include <GL/glew.h>

#include <fstream>
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"

#include "SimulGLSkyRenderer.h"
#include "Simul/Sky/Sky.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Geometry/Orientation.h"
#include "Simul/Math/Pi.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Base/SmartPtr.h"
#include "LoadGLImage.h"

void printShaderInfoLog(GLuint obj);
void printProgramInfoLog(GLuint obj);

#if 0//def _MSC_VER
GLenum sky_tex_format=GL_HALF_FLOAT_NV;
GLenum internal_format=GL_RGBA16F_ARB;
#else
GLenum sky_tex_format=GL_FLOAT;
GLenum internal_format=GL_RGBA32F_ARB;
#endif

static GLint faces[6][4] = {  /* Vertex indices for the 6 faces of a cube. */
  {0, 1, 2, 3}, {3, 2, 6, 7}, {7, 6, 5, 4},
  {4, 5, 1, 0}, {5, 6, 2, 1}, {7, 4, 0, 3} };
static GLfloat v[8][3];  /* Will be filled in with X,Y,Z vertexes. */

SimulGLSkyRenderer::SimulGLSkyRenderer(simul::sky::SkyKeyframer *sk)
	:BaseSkyRenderer(sk)
	,campos_updated(false)
	,short_ptr(NULL)
	,loss_2d(0,0,GL_TEXTURE_2D)
	,inscatter_2d(0,0,GL_TEXTURE_2D)
	,sky_program(0)
	,planet_program(0)
	,fade_3d_to_2d_program(0)
	,sun_program(0)
	,stars_program(0)
	,initialized(false)
{
//	EnableColourSky(UseColourSky);
/* Setup cube vertex data. */
	v[0][0] = v[1][0] = v[2][0] = v[3][0] = -1000.f;
	v[4][0] = v[5][0] = v[6][0] = v[7][0] =  1000.f;
	v[0][1] = v[1][1] = v[4][1] = v[5][1] = -1000.f;
	v[2][1] = v[3][1] = v[6][1] = v[7][1] =  1000.f;
	v[0][2] = v[3][2] = v[4][2] = v[7][2] =  1000.f;
	v[1][2] = v[2][2] = v[5][2] = v[6][2] = -1000.f;
	for(int i=0;i<3;i++)
	{
		sky_tex[i]=0;
		loss_textures[i]=inscatter_textures[i]=0;
	}
//	skyKeyframer->SetFillTexturesAsBlocks(true);
	SetCameraPosition(0,0,skyKeyframer->GetAltitudeKM()*1000.f);
}

void SimulGLSkyRenderer::SetSkyTexSize(unsigned size)
{
	// If not initialized we might not have a valid GL context:
	if(!initialized)
		return;
	skyTexSize=size;
	CreateSkyTextures();
}

void SimulGLSkyRenderer::CreateSkyTextures()
{
	for(int i=0;i<3;i++)
	{
		glGenTextures(1,&(sky_tex[i]));
		glBindTexture(GL_TEXTURE_2D,sky_tex[i]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D,0,internal_format,skyTexSize,numAltitudes,0,GL_RGBA,sky_tex_format,0);
	}
	
}

void SimulGLSkyRenderer::SetFadeTexSize(unsigned width_num_distances,unsigned height_num_elevations,unsigned num_alts)
{
	// If not initialized we might not have a valid GL context:
	if(!initialized)
		return;
	if(fadeTexWidth==width_num_distances&&fadeTexHeight==height_num_elevations&&numAltitudes==num_alts)
		return;
	fadeTexWidth=width_num_distances;
	fadeTexHeight=height_num_elevations;
	numAltitudes=num_alts;
	CreateFadeTextures();
}

void SimulGLSkyRenderer::CreateFadeTextures()
{
	unsigned *fade_tex_data=new unsigned[fadeTexWidth*fadeTexHeight*numAltitudes*sizeof(float)];
	glGenTextures(3,loss_textures);
	ERROR_CHECK
	glGenTextures(3,inscatter_textures);
	ERROR_CHECK
	for(int i=0;i<3;i++)
	{
		{
			glBindTexture(GL_TEXTURE_3D,loss_textures[i]);
	ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,fadeTexWidth,fadeTexHeight,numAltitudes,0,GL_RGBA,sky_tex_format,fade_tex_data);
			glBindTexture(GL_TEXTURE_3D,inscatter_textures[i]);
	ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,fadeTexWidth,fadeTexHeight,numAltitudes,0,GL_RGBA,sky_tex_format,fade_tex_data);
		}
	}
	ERROR_CHECK
	delete [] fade_tex_data;
	loss_2d.SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	ERROR_CHECK
}

static void PartialTextureFill(bool is_3d,int tex_width,int z,int texel_index,int num_texels,const float *float4_array)
{
ERROR_CHECK
	// Convert the array of floats into float16 values for the texture.
	static short short_ptr[512];
	short *sptr=short_ptr;
	const char *cptr=(const char *)float4_array;
	int texel_size=4*sizeof(float);
	if(sky_tex_format==GL_HALF_FLOAT_NV)
	{
		texel_size=4*sizeof(short);
		for(int i=0;i<num_texels*4;i++)
			*sptr++=simul::sky::TextureGenerator::ToFloat16(*float4_array++);
		cptr=(const char *)short_ptr;
	}
	int x_offset=texel_index%tex_width;
	int y_offset=texel_index/tex_width;
	int top_row=tex_width-texel_index%tex_width;
	int width=num_texels<top_row?num_texels:top_row;
	int height=1;
	if(!is_3d)
		glTexSubImage2D(GL_TEXTURE_2D,0,x_offset,y_offset,width,1,GL_RGBA,sky_tex_format,cptr);
	else
		glTexSubImage3D(GL_TEXTURE_2D,0,x_offset,y_offset,z,width,1,1,GL_RGBA,sky_tex_format,cptr);
ERROR_CHECK
	int num_done=width;
	if(num_texels>top_row)
	{
		num_texels-=top_row;
		y_offset++;
		cptr+=width*texel_size;
		height=num_texels*tex_width;
		if(!is_3d)
			glTexSubImage2D(GL_TEXTURE_2D,0,0,y_offset,tex_width,height,GL_RGBA,sky_tex_format,sptr);
		else
			glTexSubImage3D(GL_TEXTURE_3D,0,0,y_offset,z,tex_width,height,1,GL_RGBA,sky_tex_format,sptr);
ERROR_CHECK
		num_done+=tex_width*height;
		if(num_texels>height*tex_width)
		{
			int last_num=num_texels-height*tex_width;
			sptr+=height*tex_width*texel_size;
			y_offset+=height;
			if(!is_3d)
				glTexSubImage2D(GL_TEXTURE_2D,0,0,y_offset,last_num,1,GL_RGBA,sky_tex_format,sptr);
			else
				glTexSubImage3D(GL_TEXTURE_3D,0,0,y_offset,z,last_num,1,1,GL_RGBA,sky_tex_format,sptr);
ERROR_CHECK
			num_done+=last_num;
		}

	}
	if(num_done!=num_texels)
	{
		assert(0);
	}
ERROR_CHECK
}

static simul::sky::float4 Lookup(FramebufferGL fb,float distance_texcoord,float elevation_texcoord)
{
	distance_texcoord*=(float)fb.GetWidth();
	int x=(int)(distance_texcoord);
	if(x<0)
		x=0;
	if(x>fb.GetWidth()-2)
		x=fb.GetWidth()-2;
	float x_interp=distance_texcoord-x;
	elevation_texcoord*=(float)fb.GetHeight();
	int  	y=(int)(elevation_texcoord);
	if(y<0)
		y=0;
	if(y>fb.GetWidth()-2)
		y=fb.GetWidth()-2;
	float y_interp=elevation_texcoord-y;
	// four floats per texel, four texels.
	simul::sky::float4 data[4];
	fb.Activate();
	glReadPixels(x,y,2,2,GL_RGBA,GL_FLOAT,(GLvoid*)data);
	fb.Deactivate();
	simul::sky::float4 bottom	=simul::sky::lerp(x_interp,data[0],data[1]);
	simul::sky::float4 top		=simul::sky::lerp(x_interp,data[2],data[3]);
	simul::sky::float4 ret		=simul::sky::lerp(y_interp,bottom,top);
	return ret;
}

const float *SimulGLSkyRenderer::GetFastLossLookup(float distance_texcoord,float elevation_texcoord)
{
	return Lookup(loss_2d,distance_texcoord,elevation_texcoord);
}
const float *SimulGLSkyRenderer::GetFastInscatterLookup(float distance_texcoord,float elevation_texcoord)
{
	return Lookup(inscatter_2d,distance_texcoord,elevation_texcoord);
}

// Here we blend the four 3D fade textures (distance x elevation x altitude at two keyframes, for loss and inscatter)
// into pair of 2D textures (distance x elevation), eliminating the viewing altitude and time factor.
bool SimulGLSkyRenderer::Render2DFades()
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
ERROR_CHECK
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	for(int i=0;i<2;i++)
	{
		if(i==0)
			loss_2d.Activate();
		else
			inscatter_2d.Activate();
		glClearColor(0.f,0.f,0.f,0.f);
		ERROR_CHECK
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		ERROR_CHECK
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,1.0,0,1.0,-1.0,1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		//2147482496
		glActiveTexture(GL_TEXTURE0);
		if(i==0)
			glBindTexture(GL_TEXTURE_3D,loss_textures[0]);
		else
			glBindTexture(GL_TEXTURE_3D,inscatter_textures[0]);
		ERROR_CHECK
		glActiveTexture(GL_TEXTURE1);
		if(i==0)
			glBindTexture(GL_TEXTURE_3D,loss_textures[1]);
		else
			glBindTexture(GL_TEXTURE_3D,inscatter_textures[1]);
		ERROR_CHECK
		glUseProgram(fade_3d_to_2d_program);
		GLint			altitudeTexCoord_fade;
		GLint			skyInterp_fade;
		GLint			fadeTexture1_fade;
		GLint			fadeTexture2_fade;
		altitudeTexCoord_fade	=glGetUniformLocation(fade_3d_to_2d_program,"altitudeTexCoord");
		skyInterp_fade			=glGetUniformLocation(fade_3d_to_2d_program,"skyInterp");
		fadeTexture1_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture1");
		fadeTexture2_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture2");
		glUniform1f(skyInterp_fade,skyKeyframer->GetInterpolation());
		glUniform1f(altitudeTexCoord_fade,skyKeyframer->GetAltitudeTexCoord());
		glUniform1i(fadeTexture1_fade,0);
		glUniform1i(fadeTexture2_fade,1);

		glBegin(GL_QUADS);
		glTexCoord2f(0.f,1.f);
		glVertex2f(0.f,1.f);
		glTexCoord2f(1.f,1.f);
		glVertex2f(1.f,1.f);
		glTexCoord2f(1.0,0.f);
		glVertex2f(1.f,0.f);
		glTexCoord2f(0.f,0.f);
		glVertex2f(0.f,0.f);
		glEnd();
		ERROR_CHECK
		if(i==0)
			loss_2d.Deactivate();
		else
			inscatter_2d.Deactivate();
		glUseProgram(NULL);
	}
ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,NULL);
	glDisable(GL_TEXTURE_3D);
ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
ERROR_CHECK
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
ERROR_CHECK
	return true;
}
void RenderTexture(int x,int y,int w,int h)
{
	ERROR_CHECK		
	//glMatrixMode(GL_TEXTURE);
	//glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBegin(GL_QUADS);
	glTexCoord2f(0.f,1.f);
	glVertex2f((float)x,(float)(y+h));
	glTexCoord2f(1.f,1.f);
	glVertex2f((float)(x+w),(float)(y+h));
	glTexCoord2f(1.f,0.f);
	glVertex2f((float)(x+w),(float)y);
	glTexCoord2f(0.f,0.f);
	glVertex2f((float)x,(float)y);
	glEnd();
	ERROR_CHECK
}
/*
void SimulGLSkyRenderer::RenderFades(int width)
{
	int w=width/3-8;
    glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
ERROR_CHECK
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glUseProgram(0);
ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,loss_2d.GetColorTex());
	DrawQuad(8,8,w,w);
	glBindTexture(GL_TEXTURE_2D,inscatter_2d.GetColorTex());
	DrawQuad(16+w,8,w,w);
}
*/
bool SimulGLSkyRenderer::RenderFades(int w,int h)
{
	int size=w/4;
	if(h/(numAltitudes+2)<size)
		size=h/(numAltitudes+2);
	if(glStringMarkerGREMEDY)
		glStringMarkerGREMEDY(11,"RenderFades");
	static int main_viewport[]={0,0,1,1};
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,(double)main_viewport[2],(double)main_viewport[3],0,-1.0,1.0);
ERROR_CHECK
	glUseProgram(0);
	glDisable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,inscatter_2d.GetColorTex());
	ERROR_CHECK
	RenderTexture(8,32,size,size);
	glBindTexture(GL_TEXTURE_2D,sky_tex[0]);
	RenderTexture(16+size,32,8,size);
	glBindTexture(GL_TEXTURE_2D,sky_tex[1]);
	RenderTexture(32+size,32,8,size);
	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,inscatter_textures[0]);

		glUseProgram(fade_3d_to_2d_program);
		GLint			altitudeTexCoord_fade;
		GLint			skyInterp_fade;
		GLint			fadeTexture1_fade;
		GLint			fadeTexture2_fade;
		altitudeTexCoord_fade	=glGetUniformLocation(fade_3d_to_2d_program,"altitudeTexCoord");
		skyInterp_fade			=glGetUniformLocation(fade_3d_to_2d_program,"skyInterp");
		fadeTexture1_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture1");
		fadeTexture2_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture2");

		glUniform1f(skyInterp_fade,0);
		glUniform1f(altitudeTexCoord_fade,atc);
		glUniform1i(fadeTexture1_fade,0);
		glUniform1i(fadeTexture2_fade,0);
		RenderTexture(8+2*(size+8)	,i*(size+8)+8, size,size);

		glBindTexture(GL_TEXTURE_3D,inscatter_textures[1]);
		RenderTexture(8+3*(size+8)	,i*(size+8)+8, size,size);
	}
	glUseProgram(0);
ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glDisable(GL_TEXTURE_2D);
ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
ERROR_CHECK
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
ERROR_CHECK
return true;
}

bool SimulGLSkyRenderer::Render(bool blend)
{
	EnsureTexturesAreUpToDate();
	simul::sky::float4 cam_dir;
	CalcCameraPosition(cam_pos,cam_dir);
	SetCameraPosition(cam_pos.x,cam_pos.y,cam_pos.z);
	Render2DFades();
ERROR_CHECK
	glClearColor(1,1,0,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	simul::sky::float4 ratio=skyKeyframer->GetMieRayleighRatio();
	simul::sky::float4 sun_dir=skyKeyframer->GetDirectionToLight();
	glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,sky_tex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,sky_tex[1]);
ERROR_CHECK
    glDisable(GL_DEPTH_TEST);
// We normally BLEND the sky because there may be hi-res things behind it like planets.
	if(blend)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
	}
	else
		glDisable(GL_BLEND);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

	campos_updated=true;
	glTranslatef(cam_pos[0],cam_pos[1],cam_pos[2]);
	glUseProgram(sky_program);

	glUniform1i(skyTexture1_param,0);
	glUniform1i(skyTexture2_param,1);
ERROR_CHECK
	glUniform1f(altitudeTexCoord_param,skyKeyframer->GetAltitudeTexCoord());
	glUniform3f(MieRayleighRatio_param,ratio.x,ratio.y,ratio.z);
	glUniform1f(hazeEccentricity_param,skyKeyframer->GetMieEccentricity());
	skyInterp_param=glGetUniformLocation(sky_program,"skyInterp");
	glUniform1f(skyInterp_param,skyKeyframer->GetInterpolation());
	glUniform3f(lightDirection_sky_param,sun_dir.x,sun_dir.y,sun_dir.z);
ERROR_CHECK
	for(int i=0;i<6;i++)
	{
		glBegin(GL_QUADS);
		glVertex3fv(&v[faces[i][0]][0]);
		glVertex3fv(&v[faces[i][1]][0]);
		glVertex3fv(&v[faces[i][2]][0]);
		glVertex3fv(&v[faces[i][3]][0]);
		glEnd();
	}
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

	glUseProgram(NULL);
ERROR_CHECK
	return true;
}

bool SimulGLSkyRenderer::RenderPointStars()
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	float sid[16];
	CalcCameraPosition(cam_pos);
	GetSiderealTransform(sid);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE,GL_SRC_ALPHA,GL_ONE);
	int current_num_stars=skyKeyframer->stars.GetNumStars();
	if(!star_vertices||current_num_stars!=num_stars)
	{
		num_stars=current_num_stars;
		delete [] star_vertices;
		star_vertices=new StarVertext[num_stars];
		static float d=100.f;
		{
			for(int i=0;i<num_stars;i++)
			{
				float ra=(float)skyKeyframer->stars.GetStar(i).ascension;
				float de=(float)skyKeyframer->stars.GetStar(i).declination;
				star_vertices[i].x= d*cos(de)*sin(ra);
				star_vertices[i].y= d*cos(de)*cos(ra);
				star_vertices[i].z= d*sin(de);
				star_vertices[i].b=(float)exp(-skyKeyframer->stars.GetStar(i).magnitude);
				star_vertices[i].c=1.f;
			}
		}
	}
	glUseProgram(stars_program);
	float sb=skyKeyframer->GetSkyInterface()->GetStarlight().x;
	float star_brightness=sb*skyKeyframer->GetStarBrightness();
	glUniform1f(starBrightness_param,star_brightness);
	float mat1[16],mat2[16];
	glGetFloatv(GL_MODELVIEW_MATRIX,mat1);

	glTranslatef(cam_pos.x,cam_pos.y,cam_pos.z);
	glGetFloatv(GL_MODELVIEW_MATRIX,mat2);

	glMultMatrixf(sid);
	glGetFloatv(GL_MODELVIEW_MATRIX,mat1);

	glBegin(GL_POINTS);
	for(int i=0;i<num_stars;i++)
	{
		StarVertext &V=star_vertices[i];
		glMultiTexCoord2f(GL_TEXTURE0,V.b,V.c);
		glVertex3f(V.x,V.y,V.z);
	}
	glEnd();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
	return true;
}

bool SimulGLSkyRenderer::RenderSun()
{
	float alt_km=0.001f*cam_pos.z;
	simul::sky::float4 sunlight=skyKeyframer->GetSkyInterface()->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=pow(1.f-sun_occlusion,0.25f)*25.f;//2700.f;
	glUseProgram(sun_program);
		ERROR_CHECK
	glUniform3f(sunlight_param,sunlight.x,sunlight.y,sunlight.z);
		ERROR_CHECK
	simul::sky::float4 sun_dir(skyKeyframer->GetDirectionToSun());
	//if(y_vertical)
	//	std::swap(sun_dir.y,sun_dir.z);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE,GL_SRC_ALPHA,GL_ONE);

	RenderAngledQuad(sun_dir,sun_angular_size);
	glUseProgram(0);
	return true;
}

bool SimulGLSkyRenderer::RenderPlanet(void* tex,float planet_angular_size,const float *dir,const float *colr,bool do_lighting)
{
		ERROR_CHECK
	CalcCameraPosition(cam_pos);
	float alt_km=0.001f*cam_pos.z;
	if(do_lighting)
	{
	}
	else
	{
	}

	simul::sky::float4 original_irradiance=skyKeyframer->GetSkyInterface()->GetSunIrradiance();

	simul::sky::float4 planet_dir4=dir;
	planet_dir4/=simul::sky::length(planet_dir4);

	simul::sky::float4 planet_colour(colr[0],colr[1],colr[2],1.f);
	float planet_elevation=asin(planet_dir4.z);
	planet_colour*=skyKeyframer->GetIsotropicColourLossFactor(alt_km,planet_elevation,0,1e10f);

	glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,(GLuint)((long int)tex));
	glUseProgram(planet_program);
		ERROR_CHECK
	glUniform3f(planetColour_param,planet_colour.x,planet_colour.y,planet_colour.z);
		ERROR_CHECK
	glUniform1i(planetTexture_param,0);
	// Transform light into planet space
	float Yaw=atan2(dir[0],dir[1]);
	float Pitch=-asin(dir[2]);
	simul::math::Vector3 sun_dir=skyKeyframer->GetDirectionToSun();
	simul::math::Vector3 sun2;
	{
		simul::geometry::SimulOrientation or;
		or.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
		or.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
		simul::math::Matrix4x4 inv_world;
		or.T4.Inverse(inv_world);
		simul::math::Multiply3(sun2,sun_dir,inv_world);
	}
		ERROR_CHECK
	glUniform3f(planetLightDir_param,sun2.x,sun2.y,sun2.z);
		ERROR_CHECK
	glUniform1i(planetTexture_param,0);

		ERROR_CHECK
	glEnable(GL_BLEND);
		ERROR_CHECK
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		ERROR_CHECK
	glBlendEquationSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		ERROR_CHECK
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		ERROR_CHECK

	bool res=RenderAngledQuad(dir,planet_angular_size);
	glUseProgram(NULL);
	return res;
}

void SimulGLSkyRenderer::Get3DLossAndInscatterTextures(void* *l1,void* *l2,void* *i1,void* *i2)
{
	*l1=(void*)loss_textures[0];
	*l2=(void*)loss_textures[1];
	*i1=(void*)inscatter_textures[0];
	*i2=(void*)inscatter_textures[1];
}

void SimulGLSkyRenderer::Get2DLossAndInscatterTextures(void* *l1,void* *i1)
{
	*l1=(void*)loss_2d.GetColorTex();
	*i1=(void*)inscatter_2d.GetColorTex();
}

void SimulGLSkyRenderer::FillSkyTex(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array)
{
	if(!initialized)
		return;
ERROR_CHECK
	if(0&&numAltitudes==1)
	{
		glBindTexture(GL_TEXTURE_1D,sky_tex[texture_index]);

		if(sky_tex_format==GL_HALF_FLOAT_NV)
		{
			// Convert the array of floats into float16 values for the texture.
			short *sptr=short_ptr;
			for(int i=0;i<num_texels*4;i++)
				*sptr++=simul::sky::TextureGenerator::ToFloat16(*float4_array++);
			glTexSubImage1D(GL_TEXTURE_1D,0,texel_index,num_texels,GL_RGBA,sky_tex_format,short_ptr);
		}
		else
		{
			glTexSubImage1D(GL_TEXTURE_1D,0,texel_index,num_texels,GL_RGBA,sky_tex_format,float4_array);
		}
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D,sky_tex[texture_index]);
ERROR_CHECK
		PartialTextureFill(false,skyTexSize,0,texel_index+skyTexSize*alt_index,num_texels,float4_array);
	}
}

void SimulGLSkyRenderer::FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d,const float *loss_float4_array,const float *inscatter_float4_array)
{
	if(!initialized)
		return;
	GLenum target=GL_TEXTURE_2D;
	if(1||numAltitudes>1)
		target=GL_TEXTURE_3D;
	glBindTexture(target,loss_textures[texture_index]);
		ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)loss_float4_array);
		ERROR_CHECK
	glBindTexture(target,inscatter_textures[texture_index]);
		ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)inscatter_float4_array);
		ERROR_CHECK
	
	glBindTexture(target,NULL);
		ERROR_CHECK
}

void SimulGLSkyRenderer::EnsureTexturesAreUpToDate()
{
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		for(int j=0;j<numAltitudes;j++)
		{
			simul::sky::BaseKeyframer::seq_texture_iterator &it=sky_texture_iterator[i][j];
			simul::sky::BaseKeyframer::seq_texture_fill texture_fill=skyKeyframer->GetSkyTextureFill(j,i,it);
			if(texture_fill.num_texels&&sky_tex[i])
			{
				FillSkyTex(j,i,texture_fill.texel_index,texture_fill.num_texels,(const float*)texture_fill.float_array_1);
			}
			simul::sky::BaseKeyframer::seq_texture_iterator &ft=fade_texture_iterator[i][j];
			simul::sky::BaseKeyframer::block_texture_fill t;
			while((t=skyKeyframer->GetBlockFadeTextureFill(j,i,ft)).w!=0)
			{
				FillFadeTextureBlocks(i,t.x,t.y,t.z,t.w,t.l,t.d,(const float*)t.float_array_1,(const float*)t.float_array_2);
			}
		}
	}
}

void SimulGLSkyRenderer::EnsureTextureCycle()
{
	int cyc=(skyKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(sky_tex[0],sky_tex[1]);
		std::swap(sky_tex[1],sky_tex[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}

void SimulGLSkyRenderer::RecompileShaders()
{
	loss_2d.SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
	inscatter_2d.SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	SAFE_DELETE_PROGRAM(sky_program);
	SAFE_DELETE_PROGRAM(planet_program);
	SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	SAFE_DELETE_PROGRAM(sun_program);
	SAFE_DELETE_PROGRAM(stars_program);
ERROR_CHECK
	sky_program=MakeProgram("simul_sky");
	MieRayleighRatio_param			=glGetUniformLocation(sky_program,"mieRayleighRatio");
	lightDirection_sky_param		=glGetUniformLocation(sky_program,"lightDir");
	hazeEccentricity_param			=glGetUniformLocation(sky_program,"hazeEccentricity");
	skyInterp_param					=glGetUniformLocation(sky_program,"skyInterp");
	skyTexture1_param				=glGetUniformLocation(sky_program,"skyTexture1");
	skyTexture2_param				=glGetUniformLocation(sky_program,"skyTexture2");
	altitudeTexCoord_param			=glGetUniformLocation(sky_program,"altitudeTexCoord");
	printProgramInfoLog(sky_program);
ERROR_CHECK
	sun_program						=LoadPrograms("simul_sun_planet_flare.vert","simul_sun.frag");
	sunlight_param					=glGetUniformLocation(sun_program,"sunlight");
	printProgramInfoLog(sun_program);
	stars_program					=LoadPrograms("simul_sun_planet_flare.vert","simul_stars.frag");
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
	printProgramInfoLog(stars_program);
ERROR_CHECK
	sun_program						=LoadPrograms("simul_sun_planet_flare.vert","simul_sun.frag");
	sunlight_param					=glGetUniformLocation(sun_program,"sunlight");
	printProgramInfoLog(sun_program);
	stars_program					=LoadPrograms("simul_sun_planet_flare.vert","simul_stars.frag");
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
	printProgramInfoLog(stars_program);
ERROR_CHECK
	planet_program					=glCreateProgram();
	GLuint planet_vertex_shader		=glCreateShader(GL_VERTEX_SHADER);
	GLuint planet_fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);
    planet_vertex_shader			=LoadShader(planet_vertex_shader,"simul_sun_planet_flare.vert");
    planet_fragment_shader			=LoadShader(planet_fragment_shader,"simul_planet.frag");
	glAttachShader(planet_program,planet_vertex_shader);
	glAttachShader(planet_program,planet_fragment_shader);
	glLinkProgram(planet_program);
	glUseProgram(planet_program);
	printProgramInfoLog(planet_program);
ERROR_CHECK
	planetTexture_param		=glGetUniformLocation(planet_program,"planetTexture");
	planetColour_param		=glGetUniformLocation(planet_program,"colour");
	planetLightDir_param	=glGetUniformLocation(planet_program,"lightDir");
	printProgramInfoLog(planet_program);
ERROR_CHECK
	fade_3d_to_2d_program			=glCreateProgram();
	GLuint fade_vertex_shader		=glCreateShader(GL_VERTEX_SHADER);
	GLuint fade_fragment_shader		=glCreateShader(GL_FRAGMENT_SHADER);
    fade_vertex_shader				=LoadShader(fade_vertex_shader,"simul_fade_3d_to_2d.vert");
    fade_fragment_shader			=LoadShader(fade_fragment_shader,"simul_fade_3d_to_2d.frag");
	glAttachShader(fade_3d_to_2d_program,fade_vertex_shader);
	glAttachShader(fade_3d_to_2d_program,fade_fragment_shader);
	glLinkProgram(fade_3d_to_2d_program);
	glUseProgram(fade_3d_to_2d_program);
	printProgramInfoLog(fade_3d_to_2d_program);
}

void SimulGLSkyRenderer::RestoreDeviceObjects(void*)
{
ERROR_CHECK
	initialized=true;
	loss_2d.SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
ERROR_CHECK
	inscatter_2d.SetWidthAndHeight(fadeTexWidth,fadeTexHeight);
ERROR_CHECK
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
ERROR_CHECK
	RecompileShaders();
ERROR_CHECK
	moon_texture=(void*)LoadGLImage("Moon.png",GL_CLAMP);
	SetPlanetImage(moon_index,moon_texture);

	glUseProgram(NULL);
	ClearIterators();
}

void SimulGLSkyRenderer::InvalidateDeviceObjects()
{
	initialized=false;
	SAFE_DELETE_PROGRAM(sky_program);
	SAFE_DELETE_PROGRAM(planet_program);
	SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	glDeleteTextures(3,loss_textures);
	glDeleteTextures(3,inscatter_textures);
}

SimulGLSkyRenderer::~SimulGLSkyRenderer()
{
	InvalidateDeviceObjects();
}

void SimulGLSkyRenderer::DrawLines(Vertext *lines,int vertex_count,bool strip)
{
	::DrawLines((VertexXyzRgba*)lines,vertex_count,strip);
}

void SimulGLSkyRenderer::PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	::PrintAt3dPos(p,text,colr,offsetx,offsety);
}
