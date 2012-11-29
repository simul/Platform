

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
	,skylight_2d(0,0,GL_TEXTURE_2D)
	,sky_program(0)
	,earthshadow_program(0)
	,current_program(0)
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
		loss_textures[i]=inscatter_textures[i]=skylight_textures[i]=0;
	}
//	skyKeyframer->SetFillTexturesAsBlocks(true);
	SetCameraPosition(0,0,skyKeyframer->GetAltitudeKM()*1000.f);
}

void SimulGLSkyRenderer::SetFadeTexSize(unsigned width_num_distances,unsigned height_num_elevations,unsigned num_alts)
{
	// If not initialized we might not have a valid GL context:
	if(!initialized)
		return;
	if(numFadeDistances==width_num_distances&&numFadeElevations==height_num_elevations&&numAltitudes==num_alts)
		return;
	numFadeDistances=width_num_distances;
	numFadeElevations=height_num_elevations;
	numAltitudes=num_alts;
	CreateFadeTextures();
}

void SimulGLSkyRenderer::CreateFadeTextures()
{
	unsigned *fade_tex_data=new unsigned[numFadeDistances*numFadeElevations*numAltitudes*sizeof(float)];
	glGenTextures(3,loss_textures);
	ERROR_CHECK
	glGenTextures(3,inscatter_textures);
	ERROR_CHECK
	glGenTextures(3,skylight_textures);
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
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
			glBindTexture(GL_TEXTURE_3D,inscatter_textures[i]);
	ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
			glBindTexture(GL_TEXTURE_3D,skylight_textures[i]);
	ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
		}
	}
	ERROR_CHECK
	delete [] fade_tex_data;
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	skylight_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	skylight_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
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
	FramebufferGL *fb[3];
	fb[0]=&loss_2d;
	fb[1]=&inscatter_2d;
	fb[2]=&skylight_2d;
	GLuint *input_textures[3];
	input_textures[0]=loss_textures;
	input_textures[1]=inscatter_textures;
	input_textures[2]=skylight_textures;
	for(int i=0;i<3;i++)
	{
		fb[i]->Activate();
		fb[i]->Clear(0.f,0.f,0.f,0.f);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,1.0,0,1.0,-1.0,1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][0]);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][1]);
		glUseProgram(fade_3d_to_2d_program);
		GLint altitudeTexCoord_fade	=glGetUniformLocation(fade_3d_to_2d_program,"altitudeTexCoord");
		GLint skyInterp_fade		=glGetUniformLocation(fade_3d_to_2d_program,"skyInterp");
		GLint fadeTexture1_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture1");
		GLint fadeTexture2_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture2");
		glUniform1f(skyInterp_fade,skyKeyframer->GetInterpolation());
		glUniform1f(altitudeTexCoord_fade,skyKeyframer->GetAltitudeTexCoord());
		glUniform1i(fadeTexture1_fade,0);
		glUniform1i(fadeTexture2_fade,1);
		DrawQuad(0,0,1,1);
		fb[i]->Deactivate();
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
	glBindTexture(GL_TEXTURE_2D,loss_2d.GetColorTex());
	RenderTexture(8,8,size,size);
	glBindTexture(GL_TEXTURE_2D,inscatter_2d.GetColorTex());
	RenderTexture(8,16+size,size,size);
	glBindTexture(GL_TEXTURE_2D,skylight_2d.GetColorTex());
	RenderTexture(8,24+2*size,size,size);
	int x=16+size;
	if(h/(numAltitudes+2)<size)
		size=h/(numAltitudes+2);
	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
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
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,inscatter_textures[0]);
		RenderTexture(x+16+0*(size+8)	,i*(size+8)+8, size,size);

		glBindTexture(GL_TEXTURE_3D,inscatter_textures[1]);
		RenderTexture(x+16+1*(size+8)	,i*(size+8)+8, size,size);
		
		glBindTexture(GL_TEXTURE_3D,loss_textures[0]);
		RenderTexture(x+16+2*(size+8)	,i*(size+8)+8, size,size);

		glBindTexture(GL_TEXTURE_3D,loss_textures[1]);
		RenderTexture(x+16+3*(size+8)	,i*(size+8)+8, size,size);
		
		glBindTexture(GL_TEXTURE_3D,skylight_textures[0]);
		RenderTexture(x+16+4*(size+8)	,i*(size+8)+8, size,size);

		glBindTexture(GL_TEXTURE_3D,skylight_textures[1]);
		RenderTexture(x+16+5*(size+8)	,i*(size+8)+8, size,size);
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

void SimulGLSkyRenderer::UseProgram(GLuint p)
{
	if(p&&p!=current_program)
	{
		current_program=p;
		MieRayleighRatio_param			=glGetUniformLocation(current_program,"mieRayleighRatio");
		lightDirection_sky_param		=glGetUniformLocation(current_program,"lightDir");
		sunDir_param					=glGetUniformLocation(current_program,"sunDir");
		hazeEccentricity_param			=glGetUniformLocation(current_program,"hazeEccentricity");
		skyInterp_param					=glGetUniformLocation(current_program,"skyInterp");
		skyTexture1_param				=glGetUniformLocation(current_program,"inscTexture");
		skylightTexture_param			=glGetUniformLocation(current_program,"skylightTexture");
			
		altitudeTexCoord_param			=glGetUniformLocation(current_program,"altitudeTexCoord");
		earthShadowNormal_param			=glGetUniformLocation(current_program,"earthShadowNormal");
		maxFadeDistance_param			=glGetUniformLocation(current_program,"maxFadeDistance");
		radiusOnCylinder_param			=glGetUniformLocation(current_program,"radiusOnCylinder");
		terminatorCosine_param			=glGetUniformLocation(current_program,"terminatorCosine");
		printProgramInfoLog(current_program);
	}
	glUseProgram(p);
}

bool SimulGLSkyRenderer::Render(bool blend)
{
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	EnsureTexturesAreUpToDate();
	simul::sky::float4 cam_dir;
	CalcCameraPosition(cam_pos,cam_dir);
	SetCameraPosition(cam_pos.x,cam_pos.y,cam_pos.z);
	Render2DFades();
ERROR_CHECK
	//if(!blend)
	//{
		glClearColor(0,0,0,1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	//}
	simul::sky::float4 ratio=skyKeyframer->GetMieRayleighRatio();
	simul::sky::float4 light_dir=skyKeyframer->GetDirectionToLight(skyKeyframer->GetAltitudeKM());
	simul::sky::float4 sun_dir=skyKeyframer->GetDirectionToSun();
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
	simul::sky::EarthShadow e=skyKeyframer->GetEarthShadow(
								skyKeyframer->GetAltitudeKM()
								,skyKeyframer->GetDirectionToSun());
	if(e.enable)
		UseProgram(earthshadow_program);
	else
		UseProgram(sky_program);

	glUniform1i(skyTexture1_param,0);
	
ERROR_CHECK
	glUniform1f(altitudeTexCoord_param,skyKeyframer->GetAltitudeTexCoord());
	glUniform3f(MieRayleighRatio_param,ratio.x,ratio.y,ratio.z);
	glUniform1f(hazeEccentricity_param,skyKeyframer->GetMieEccentricity());
	glUniform1f(skyInterp_param,skyKeyframer->GetInterpolation());
	glUniform3f(lightDirection_sky_param,light_dir.x,light_dir.y,light_dir.z);
	glUniform3f(sunDir_param,sun_dir.x,sun_dir.y,sun_dir.z);
	
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,inscatter_2d.GetColorTex());
	glUniform1i(skylightTexture_param,1);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,skylight_2d.GetColorTex());
	if(current_program==earthshadow_program)
	{
		glUniform1f(radiusOnCylinder_param,e.radius_on_cylinder);
		glUniform3f(earthShadowNormal_param,e.normal.x,e.normal.y,e.normal.z);
		glUniform1f(maxFadeDistance_param,skyKeyframer->GetMaxDistanceKm()/skyKeyframer->GetSkyInterface()->GetPlanetRadius());
		glUniform1f(terminatorCosine_param,e.terminator_cosine);
	}
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
	glDisable(GL_TEXTURE_3D);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,NULL);
	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,NULL);
	
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,NULL);

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

void SimulGLSkyRenderer::RenderSun()
{
	float alt_km=0.001f*cam_pos.z;
	simul::sky::float4 sun_dir(skyKeyframer->GetDirectionToSun());
	simul::sky::float4 sunlight=skyKeyframer->GetSkyInterface()->GetLocalIrradiance(alt_km,sun_dir);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=pow(1.f-sun_occlusion,0.25f)*2700.f;
	// But to avoid artifacts like aliasing at the edges, we will rescale the colour itself
	// to the range [0,1], and store a brightness multiplier in the alpha channel!
	sunlight.w=1.f;
	float max_bright=std::max(std::max(sunlight.x,sunlight.y),sunlight.z);
	if(max_bright>1.f)
	{
		sunlight*=1.f/max_bright;
		sunlight.w=max_bright;
	}
	glUseProgram(sun_program);
	ERROR_CHECK
	glUniform4f(sunlight_param,sunlight.x,sunlight.y,sunlight.z,sunlight.w);
	ERROR_CHECK
	//if(y_vertical)
	//	std::swap(sun_dir.y,sun_dir.z);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	RenderAngledQuad(sun_dir,sun_angular_size);
	glUseProgram(0);
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
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
		ERROR_CHECK
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		ERROR_CHECK

	bool res=RenderAngledQuad(dir,planet_angular_size);
	glUseProgram(NULL);
	return res;
}

void SimulGLSkyRenderer::Get2DLossAndInscatterTextures(void* *l1,void* *i1,void * *s)
{
	*l1=(void*)loss_2d.GetColorTex();
	*i1=(void*)inscatter_2d.GetColorTex();
	*s=(void*)skylight_2d.GetColorTex();
}

void SimulGLSkyRenderer::FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d
	,const float *loss_float4_array,const float *inscatter_float4_array,
								const float *skylight_float4_array)
{
	if(!initialized)
		return;
	GLenum target=GL_TEXTURE_3D;
	glBindTexture(target,loss_textures[texture_index]);
		ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)loss_float4_array);
		ERROR_CHECK
	glBindTexture(target,inscatter_textures[texture_index]);
		ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)inscatter_float4_array);
		ERROR_CHECK
	glBindTexture(target,skylight_textures[texture_index]);
		ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)skylight_float4_array);
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
		simul::sky::BaseKeyframer::seq_texture_iterator &ft=fade_texture_iterator[i];
		simul::sky::BaseKeyframer::block_texture_fill t;
		while((t=skyKeyframer->GetBlockFadeTextureFill(i,ft)).w!=0)
		{
			FillFadeTextureBlocks(i,t.x,t.y,t.z,t.w,t.l,t.d,(const float*)t.float_array_1,(const float*)t.float_array_2,(const float*)t.float_array_3);
		}
	}
}

void SimulGLSkyRenderer::EnsureTextureCycle()
{
	int cyc=(skyKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(loss_textures[0],loss_textures[1]);
		std::swap(loss_textures[1],loss_textures[2]);
		std::swap(inscatter_textures[0],inscatter_textures[1]);
		std::swap(inscatter_textures[1],inscatter_textures[2]);
		std::swap(skylight_textures[0],skylight_textures[1]);
		std::swap(skylight_textures[1],skylight_textures[2]);
		std::swap(fade_texture_iterator[0],fade_texture_iterator[1]);
		std::swap(fade_texture_iterator[1],fade_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
		for(int i=0;i<3;i++)
			fade_texture_iterator[i].texture_index=i;
	}
}

void SimulGLSkyRenderer::RecompileShaders()
{
	current_program=0;
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	SAFE_DELETE_PROGRAM(sky_program);
	SAFE_DELETE_PROGRAM(earthshadow_program);
	SAFE_DELETE_PROGRAM(planet_program);
	SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	SAFE_DELETE_PROGRAM(sun_program);
	SAFE_DELETE_PROGRAM(stars_program);
ERROR_CHECK
	sky_program						=MakeProgram("simul_sky");
	printProgramInfoLog(sky_program);
	earthshadow_program				=LoadPrograms("simul_sky.vert",NULL,"simul_earthshadow_sky.frag");
	printProgramInfoLog(earthshadow_program);
ERROR_CHECK
	sun_program						=LoadPrograms("simul_sun_planet_flare.vert",NULL,"simul_sun.frag");
	sunlight_param					=glGetUniformLocation(sun_program,"sunlight");
	printProgramInfoLog(sun_program);
	stars_program					=LoadPrograms("simul_sun_planet_flare.vert",NULL,"simul_stars.frag");
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
	printProgramInfoLog(stars_program);
ERROR_CHECK
	sun_program						=LoadPrograms("simul_sun_planet_flare.vert",NULL,"simul_sun.frag");
	sunlight_param					=glGetUniformLocation(sun_program,"sunlight");
	printProgramInfoLog(sun_program);
	stars_program					=LoadPrograms("simul_sun_planet_flare.vert",NULL,"simul_stars.frag");
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
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
ERROR_CHECK
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
ERROR_CHECK
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
ERROR_CHECK
	RecompileShaders();
ERROR_CHECK
	moon_texture=(void*)LoadGLImage("Moon.png",GL_CLAMP_TO_EDGE);
	SetPlanetImage(moon_index,moon_texture);

	glUseProgram(NULL);
	ClearIterators();
}

void SimulGLSkyRenderer::InvalidateDeviceObjects()
{
	initialized=false;
	SAFE_DELETE_PROGRAM(sky_program);
	SAFE_DELETE_PROGRAM(earthshadow_program);
	SAFE_DELETE_PROGRAM(planet_program);
	SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	glDeleteTextures(3,loss_textures);
	glDeleteTextures(3,inscatter_textures);
	glDeleteTextures(3,skylight_textures);
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
