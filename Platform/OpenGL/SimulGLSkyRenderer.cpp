

#include "Simul/Base/Timer.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h> // for uintptr_t

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
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/earth_shadow_uniforms.sl"
using namespace simul;
using namespace opengl;

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

SimulGLSkyRenderer::SimulGLSkyRenderer(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *m)
	:BaseSkyRenderer(sk,m)
	,campos_updated(false)
	,short_ptr(NULL)
	,loss_2d(0,0,GL_TEXTURE_2D)
	,inscatter_2d(0,0,GL_TEXTURE_2D)
	,skylight_2d(0,0,GL_TEXTURE_2D)
	,overcast_2d(0,0,GL_TEXTURE_2D)

	,loss_texture(0)
	,insc_texture(0)
	,skyl_texture(0)

	,sky_program(0)
	,earthshadow_program(0)
	,current_program(0)
	,planet_program(0)
	,fade_3d_to_2d_program(0)
	,sun_program(0)
	,stars_program(0)
	,illumination_buffer_program(0)
	,overcast_inscatter_program(0)
	,initialized(false)
{
/* Setup cube vertex data. */
	v[0][0] = v[1][0] = v[2][0] = v[3][0] = -100.f;
	v[4][0] = v[5][0] = v[6][0] = v[7][0] =  100.f;
	v[0][1] = v[1][1] = v[4][1] = v[5][1] = -100.f;
	v[2][1] = v[3][1] = v[6][1] = v[7][1] =  100.f;
	v[0][2] = v[3][2] = v[4][2] = v[7][2] =  100.f;
	v[1][2] = v[2][2] = v[5][2] = v[6][2] = -100.f;
	for(int i=0;i<3;i++)
	{
		loss_textures[i]=inscatter_textures[i]=skylight_textures[i]=0;
	}
//	skyKeyframer->SetFillTexturesAsBlocks(true);
	SetCameraPosition(0,0,skyKeyframer->GetAltitudeKM()*1000.f);
}

void SimulGLSkyRenderer::SetFadeTexSize(int width_num_distances,int height_num_elevations,int num_alts)
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
	GL_ERROR_CHECK
	glGenTextures(3,inscatter_textures);
	GL_ERROR_CHECK
	glGenTextures(3,skylight_textures);
	GL_ERROR_CHECK
	for(int i=0;i<3;i++)
	{
		{
			glBindTexture(GL_TEXTURE_3D,loss_textures[i]);
	GL_ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
			glBindTexture(GL_TEXTURE_3D,inscatter_textures[i]);
	GL_ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
			glBindTexture(GL_TEXTURE_3D,skylight_textures[i]);
	GL_ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
		}
	}
	GL_ERROR_CHECK
	delete [] fade_tex_data;
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	skylight_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	overcast_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	skylight_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	overcast_2d.InitColor_Tex(0,GL_RGBA32F_ARB);

	SAFE_DELETE_TEXTURE(loss_texture);
	SAFE_DELETE_TEXTURE(insc_texture);
	SAFE_DELETE_TEXTURE(skyl_texture);
	loss_texture=make2DTexture(numFadeDistances,numFadeElevations);
	insc_texture=make2DTexture(numFadeDistances,numFadeElevations);
	skyl_texture=make2DTexture(numFadeDistances,numFadeElevations);


	GL_ERROR_CHECK
}

static simul::sky::float4 Lookup(void *context,FramebufferGL &fb,float distance_texcoord,float elevation_texcoord)
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
	fb.Activate(context);
	glReadPixels(x,y,2,2,GL_RGBA,GL_FLOAT,(GLvoid*)data);
	fb.Deactivate(context);
	simul::sky::float4 bottom	=simul::sky::lerp(x_interp,data[0],data[1]);
	simul::sky::float4 top		=simul::sky::lerp(x_interp,data[2],data[3]);
	simul::sky::float4 ret		=simul::sky::lerp(y_interp,bottom,top);
	return ret;
}

const float *SimulGLSkyRenderer::GetFastLossLookup(void *context,float distance_texcoord,float elevation_texcoord)
{
	return Lookup(context,loss_2d,distance_texcoord,elevation_texcoord);
}
const float *SimulGLSkyRenderer::GetFastInscatterLookup(void *context,float distance_texcoord,float elevation_texcoord)
{
	return Lookup(context,inscatter_2d,distance_texcoord,elevation_texcoord);
}


void SimulGLSkyRenderer::RenderIlluminationBuffer(void *context)
{
	SetIlluminationConstants(earthShadowUniforms,skyConstants);
	skyConstants.Apply();
	earthShadowUniforms.Apply();
	{
		//D3DXHANDLE tech=m_pSkyEffect->GetTechniqueByName("illumination_buffer");
		//m_pSkyEffect->SetTechnique(tech);
		glUseProgram(illumination_buffer_program);
		illumination_fb.Activate(context);
		illumination_fb.Clear(context,1.0f,1.0f,1.0f,1.0f,1.f);
		DrawQuad(0,0,1,1);
		illumination_fb.Deactivate(context);
	}
}
// Here we blend the four 3D fade textures (distance x elevation x altitude at two keyframes, for loss and inscatter)
// into pair of 2D textures (distance x elevation), eliminating the viewing altitude and time factor.
bool SimulGLSkyRenderer::Render2DFades(void *context)
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
	RenderIlluminationBuffer(context);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	FramebufferGL *fb[]={&loss_2d,&inscatter_2d,&skylight_2d};
	GLuint target_textures[]={loss_texture,insc_texture,skyl_texture};
	GLuint *input_textures[]={loss_textures,inscatter_textures,skylight_textures};
	glUseProgram(fade_3d_to_2d_program);
	skyConstants.skyInterp			=skyKeyframer->GetInterpolation();
	skyConstants.altitudeTexCoord	=skyKeyframer->GetAltitudeTexCoord();
	skyConstants.overcast			=skyKeyframer->GetSkyInterface()->GetOvercast();
	skyConstants.eyePosition		=cam_pos;
	skyConstants.cloudShadowRange	=sqrt(80.f/skyKeyframer->GetMaxDistanceKm());
	skyConstants.cycled_index		=texture_cycle%3;
	skyConstants.Apply();
	glUniform1i(fadeTexture1_fade,0);
	glUniform1i(fadeTexture2_fade,1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,1.0,0,1.0,-1.0,1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	for(int i=0;i<3;i++)
	{
		fb[i]->Activate(context);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][0]);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][1]);
		DrawQuad(0,0,1,1);
		// copy to target:
		{
			glEnable(GL_TEXTURE_2D);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,target_textures[i]);
			glCopyTexSubImage2D(GL_TEXTURE_2D,0
					,0,0
					,0,0,
 					numFadeDistances,numFadeElevations	);
			glDisable(GL_TEXTURE_2D);
		}
		fb[i]->Deactivate(context);
	}
	glUseProgram(NULL);
	
	glUseProgram(overcast_inscatter_program);

	earthShadowUniforms.targetTextureSize=vec2((float)numFadeDistances,(float)numFadeElevations);
	earthShadowUniforms.Apply();
	
	setTexture(overcast_inscatter_program,"inscTexture"			,0,(GLuint)inscatter_2d.GetColorTex());
	setTexture(overcast_inscatter_program,"illuminationTexture"	,1,(GLuint)illumination_fb.GetColorTex());

	overcast_2d.Activate(NULL);
		overcast_2d.Clear(NULL,1.f,1.f,0.f,1.f,1.f);
		DrawQuad(0,0,1,1);
	overcast_2d.Deactivate(NULL);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,NULL);
	glDisable(GL_TEXTURE_3D);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	return true;
}

bool SimulGLSkyRenderer::RenderFades(void *,int width,int height)
{
	int size=width/6;
	if(height/4<size)
		size=height/4;
	if(size<2)
		return false;
	int s=size/numAltitudes-2;
	int y0=width/2;
	int x0=8;
	if(width>height)
	{
		x0=width-(size+8)*2-(s+8)*3;
		y0=8;
	}
	int y=y0+8;
	static int main_viewport[]={0,0,1,1};
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,(double)main_viewport[2],(double)main_viewport[3],0,-1.0,1.0);
GL_ERROR_CHECK
	glUseProgram(0);
	glDisable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE0);

	glBindTexture(GL_TEXTURE_2D,loss_texture);
	RenderTexture(x0+size+2,y	,size,size);
	y+=size+8;
	glBindTexture(GL_TEXTURE_2D,insc_texture);
	RenderTexture(x0+size+2,y	,size,size);
	glBindTexture(GL_TEXTURE_2D,(GLuint)overcast_2d.GetColorTex());
	RenderTexture(x0		,y	,size,size);
	y+=size+8;

	glBindTexture(GL_TEXTURE_2D,skyl_texture);
	RenderTexture(x0+size+2	,y	,size,size);
	y+=size+8;
	bool show_3=true;
	glBindTexture(GL_TEXTURE_2D,(GLuint)illumination_fb.GetColorTex());
	glUseProgram(show_illumination_buffer_program);
	RenderTexture(x0+size+2	,y	,size,size);

	x0+=2*(size+8);
	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		int y=y0+i*(s+4);
		glUseProgram(fade_3d_to_2d_program);
		GLint fadeTexture1_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture1");
		GLint fadeTexture2_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture2");

		skyConstants.skyInterp			=0.f;
		skyConstants.altitudeTexCoord	=atc;
		skyConstants.Apply();

		glUniform1i(fadeTexture1_fade,0);
		glUniform1i(fadeTexture2_fade,0);
		
		glActiveTexture(GL_TEXTURE0);
		for(int j=0;j<(show_3?3:2);j++)
		{
			int x=x0+(s+8)*j;
			glBindTexture(GL_TEXTURE_3D,loss_textures[j]);
			RenderTexture(x	,y+8			,s,s);
			glBindTexture(GL_TEXTURE_3D,inscatter_textures[j]);
			RenderTexture(x	,y+16+size		,s,s);
			glBindTexture(GL_TEXTURE_3D,skylight_textures[j]);
			RenderTexture(x	,y+24+2*size	,s,s);
		}
	}

	glUseProgram(0);
GL_ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glDisable(GL_TEXTURE_2D);
GL_ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
GL_ERROR_CHECK
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
GL_ERROR_CHECK
return true;
}

void SimulGLSkyRenderer::UseProgram(GLuint p)
{
	if(p&&p!=current_program)
	{
		current_program=p;
		MieRayleighRatio_param		=glGetUniformLocation(current_program,"mieRayleighRatio");
		lightDirection_sky_param	=glGetUniformLocation(current_program,"lightDir");
		hazeEccentricity_param		=glGetUniformLocation(current_program,"hazeEccentricity");
		skyInterp_param				=glGetUniformLocation(current_program,"skyInterp");
		skyTexture1_param			=glGetUniformLocation(current_program,"inscTexture");
		skylightTexture_param		=glGetUniformLocation(current_program,"skylightTexture");
			
		altitudeTexCoord_param		=glGetUniformLocation(current_program,"altitudeTexCoord");
		
		earthShadowUniforms			.LinkToProgram(current_program,"EarthShadowUniforms",9);
		skyConstants				.LinkToProgram(current_program,"SkyConstants",10);

		cloudOrigin					=glGetUniformLocation(current_program,"cloudOrigin");
		cloudScale					=glGetUniformLocation(current_program,"cloudScale");
		maxDistance					=glGetUniformLocation(current_program,"maxDistance");
		viewPosition				=glGetUniformLocation(current_program,"viewPosition");
		overcast_param				=glGetUniformLocation(current_program,"overcast");
	}
	glUseProgram(p);
}

bool SimulGLSkyRenderer::Render(void *,bool )
{
	return true;
}

bool SimulGLSkyRenderer::RenderPointStars(void *,float exposure)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	float sid[16];
	CalcCameraPosition(cam_pos);
	GetSiderealTransform(sid);
	glEnable(GL_BLEND);
	glDepthFunc(ReverseDepth?GL_GEQUAL:GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
#if 1
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
			}
		}
	}
	glUseProgram(stars_program);
	float sb=skyKeyframer->GetStarlight().x;
	float star_brightness=sb*exposure*skyKeyframer->GetStarBrightness();
	skyConstants.starBrightness=star_brightness;
	skyConstants.Apply();
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
		glMultiTexCoord1f(GL_TEXTURE0,V.b);
		glVertex3f(V.x,V.y,V.z);
	}
	glEnd();
#endif
	glPopAttrib();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
	return true;
}

void SimulGLSkyRenderer::RenderSun(void *,float exposure)
{
	float alt_km=0.001f*cam_pos.z;
	simul::sky::float4 sun_dir(skyKeyframer->GetDirectionToSun());
	simul::sky::float4 sunlight=skyKeyframer->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=exposure*pow(1.f-sun_occlusion,0.25f)*2700.f;
	// But to avoid artifacts like aliasing at the edges, we will rescale the colour itself
	// to the range [0,1], and store a brightness multiplier in the alpha channel!
	sunlight.w=1.f;
	glUseProgram(sun_program);
	GL_ERROR_CHECK
	glUniform4f(sunlight_param,sunlight.x,sunlight.y,sunlight.z,sunlight.w);
	//skyConstants.colour=sunlight;
	GL_ERROR_CHECK
	glEnable(GL_BLEND);
	glDepthFunc(ReverseDepth?GL_GEQUAL:GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); 

	RenderAngledQuad(sun_dir,sun_angular_radius);
	glUseProgram(0);
}

void SimulGLSkyRenderer::RenderPlanet(void *,void* tex,float planet_angular_size,const float *dir,const float *colr,bool do_lighting)
{
		GL_ERROR_CHECK
	CalcCameraPosition(cam_pos);
	float alt_km=0.001f*cam_pos.z;
	glDepthFunc(ReverseDepth?GL_GEQUAL:GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

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
		GL_ERROR_CHECK
	glUniform3f(planetColour_param,planet_colour.x,planet_colour.y,planet_colour.z);
		GL_ERROR_CHECK
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
	if(do_lighting)
		glUniform3f(planetLightDir_param,sun2.x,sun2.y,sun2.z);
	GL_ERROR_CHECK
	glUniform1i(planetTexture_param,0);

		GL_ERROR_CHECK
	glEnable(GL_BLEND);
		GL_ERROR_CHECK
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		GL_ERROR_CHECK
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
		GL_ERROR_CHECK
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		GL_ERROR_CHECK

	RenderAngledQuad(dir,planet_angular_size);
	glUseProgram(NULL);
}

void SimulGLSkyRenderer::Get2DLossAndInscatterTextures(void* *l1,void* *i1,void * *s,void* *o)
{
	*l1=(void*)(uintptr_t)loss_texture;
	*i1=(void*)(uintptr_t)insc_texture;
	*s=(void*)(uintptr_t)skyl_texture;
	*o=(void*)(uintptr_t)overcast_2d.GetColorTex();
}

void SimulGLSkyRenderer::FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d
	,const float *loss_float4_array,const float *inscatter_float4_array,
								const float *skylight_float4_array)
{
	if(!initialized)
		return;
	GLenum target=GL_TEXTURE_3D;
	glBindTexture(target,loss_textures[texture_index]);
		GL_ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)loss_float4_array);
		GL_ERROR_CHECK
	glBindTexture(target,inscatter_textures[texture_index]);
		GL_ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)inscatter_float4_array);
		GL_ERROR_CHECK
	glBindTexture(target,skylight_textures[texture_index]);
		GL_ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)skylight_float4_array);
		GL_ERROR_CHECK
	glBindTexture(target,NULL);
		GL_ERROR_CHECK
}

void SimulGLSkyRenderer::EnsureTexturesAreUpToDate(void *)
{
	EnsureCorrectTextureSizes();
	illumination_fb.SetWidthAndHeight(128,numFadeElevations);
	illumination_fb.InitColor_Tex(0,GL_RGBA32F_ARB);
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

void SimulGLSkyRenderer::ReloadTextures()
{
	moon_texture=(void*)(uintptr_t)LoadGLImage(skyKeyframer->GetMoonTexture().c_str(),GL_CLAMP_TO_EDGE);
	SetPlanetImage(moon_index,moon_texture);
}

void SimulGLSkyRenderer::RecompileShaders()
{
	current_program=0;
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	gpuSkyGenerator.RecompileShaders();
	SAFE_DELETE_PROGRAM(sky_program);
	SAFE_DELETE_PROGRAM(earthshadow_program);
	SAFE_DELETE_PROGRAM(planet_program);
	SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	SAFE_DELETE_PROGRAM(sun_program);
	SAFE_DELETE_PROGRAM(stars_program);
GL_ERROR_CHECK
	sky_program						=MakeProgram("simul_sky");
	earthshadow_program				=MakeProgram("simul_sky.vert",NULL,"simul_earthshadow_sky.frag");
GL_ERROR_CHECK
	sun_program						=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_sun.frag");
	sunlight_param					=glGetUniformLocation(sun_program,"sunlight");
	stars_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_stars.frag");
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
GL_ERROR_CHECK
	sun_program						=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_sun.frag");
	sunlight_param					=glGetUniformLocation(sun_program,"sunlight");
	stars_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_stars.frag");
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
GL_ERROR_CHECK
	planet_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_planet.frag");
GL_ERROR_CHECK
	planetTexture_param				=glGetUniformLocation(planet_program,"planetTexture");
	planetColour_param				=glGetUniformLocation(planet_program,"colour");
	planetLightDir_param			=glGetUniformLocation(planet_program,"lightDir");
GL_ERROR_CHECK
	fade_3d_to_2d_program			=MakeProgram("simul_fade_3d_to_2d");
	
	altitudeTexCoord_fade	=glGetUniformLocation(fade_3d_to_2d_program,"altitudeTexCoord");
	skyInterp_fade		=glGetUniformLocation(fade_3d_to_2d_program,"skyInterp");
	fadeTexture1_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture1");
	fadeTexture2_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture2");

	illumination_buffer_program		=MakeProgram("simple.vert",NULL,"illumination_buffer.frag");
	overcast_inscatter_program		=MakeProgram("simple.vert",NULL,"overcast_inscatter.frag");
	show_illumination_buffer_program=MakeProgram("simul_fade_3d_to_2d.vert",NULL,"show_illumination_buffer.frag");

	skyConstants					.LinkToProgram(fade_3d_to_2d_program		,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(stars_program				,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(planet_program				,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(sun_program					,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(illumination_buffer_program	,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(overcast_inscatter_program	,"SkyConstants"			,10);
	earthShadowUniforms				.LinkToProgram(illumination_buffer_program	,"EarthShadowUniforms"	,9);
//	earthShadowUniforms				.LinkToProgram(overcast_inscatter_program	,"EarthShadowUniforms"	,9);
	earthShadowUniforms				.LinkToProgram(overcast_inscatter_program	,"EarthShadowUniforms"	,9);
}

void SimulGLSkyRenderer::RestoreDeviceObjects(void*)
{
GL_ERROR_CHECK
	initialized=true;
	gpuSkyGenerator.RestoreDeviceObjects(NULL);
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	skylight_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	overcast_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	skylight_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	overcast_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	SAFE_DELETE_TEXTURE(loss_texture);
	SAFE_DELETE_TEXTURE(insc_texture);
	SAFE_DELETE_TEXTURE(skyl_texture);
	loss_texture=make2DTexture(numFadeDistances,numFadeElevations);
	insc_texture=make2DTexture(numFadeDistances,numFadeElevations);
	skyl_texture=make2DTexture(numFadeDistances,numFadeElevations);

	skyConstants.RestoreDeviceObjects();
	earthShadowUniforms.RestoreDeviceObjects();
GL_ERROR_CHECK
	RecompileShaders();
	glUseProgram(NULL);
GL_ERROR_CHECK
	ReloadTextures();
	ClearIterators();
}

void SimulGLSkyRenderer::InvalidateDeviceObjects()
{
	initialized=false;
	gpuSkyGenerator.InvalidateDeviceObjects();
	
	skyConstants.Release();
	earthShadowUniforms.Release();

	SAFE_DELETE_PROGRAM(sky_program);
	SAFE_DELETE_PROGRAM(earthshadow_program);
	SAFE_DELETE_PROGRAM(planet_program);
	SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	glDeleteTextures(3,loss_textures);
	glDeleteTextures(3,inscatter_textures);
	glDeleteTextures(3,skylight_textures);
	SAFE_DELETE_TEXTURE(loss_texture);
	SAFE_DELETE_TEXTURE(insc_texture);
	SAFE_DELETE_TEXTURE(skyl_texture);
	
	loss_2d.InvalidateDeviceObjects();
	inscatter_2d.InvalidateDeviceObjects();
	skylight_2d.InvalidateDeviceObjects();
	illumination_fb.InvalidateDeviceObjects();
	overcast_2d.InvalidateDeviceObjects();
}

SimulGLSkyRenderer::~SimulGLSkyRenderer()
{
	InvalidateDeviceObjects();
}

void SimulGLSkyRenderer::DrawLines(void *,Vertext *lines,int vertex_count,bool strip)
{
	::DrawLines((VertexXyzRgba*)lines,vertex_count,strip);
}

void SimulGLSkyRenderer::PrintAt3dPos(void *,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	::PrintAt3dPos(p,text,colr,offsetx,offsety);
}

const char *SimulGLSkyRenderer::GetDebugText()
{
	simul::sky::EarthShadow e=skyKeyframer->GetEarthShadow(
								skyKeyframer->GetAltitudeKM()
								,skyKeyframer->GetDirectionToSun());
	static char txt[400];
//	sprintf_s(txt,400,"e.normal (%4.4g, %4.4g, %4.4g) r-1: %4.4g, cos: %4.4g, illum alt: %4.4g",e.normal.x,e.normal.y,e.normal.z
//		,e.radius_on_cylinder-1.f,e.terminator_cosine,e.illumination_altitude);
	return txt;
}