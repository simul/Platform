

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
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Geometry/Orientation.h"
#include "Simul/Math/Pi.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Base/SmartPtr.h"
#include "LoadGLImage.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/SL/earth_shadow_uniforms.sl"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Camera/Camera.h"
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

	,current_program(0)
	,planet_program(0)
	,fade_3d_to_2d_program(0)
	,stars_program(0)
	,initialized(false)
{
/* Setup cube vertex data. */
	v[0][0] = v[1][0] = v[2][0] = v[3][0] = -100.f;
	v[4][0] = v[5][0] = v[6][0] = v[7][0] =  100.f;
	v[0][1] = v[1][1] = v[4][1] = v[5][1] = -100.f;
	v[2][1] = v[3][1] = v[6][1] = v[7][1] =  100.f;
	v[0][2] = v[3][2] = v[4][2] = v[7][2] =  100.f;
	v[1][2] = v[2][2] = v[5][2] = v[6][2] = -100.f;
//	skyKeyframer->SetFillTexturesAsBlocks(true);
	gpuSkyGenerator.SetEnabled(true);
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
	simul::sky::int3 i=skyKeyframer->GetTextureSizes();
	int num_dist=i.x;
	int num_elev=i.y;
	int num_alt=i.z;
	for(int i=0;i<3;i++)
	{
		loss_textures[i].ensureTexture3DSizeAndFormat(NULL,num_alt,num_elev,num_dist,GL_RGBA32F,true);
		insc_textures[i].ensureTexture3DSizeAndFormat(NULL,num_alt,num_elev,num_dist,GL_RGBA32F,true);
		skyl_textures[i].ensureTexture3DSizeAndFormat(NULL,num_alt,num_elev,num_dist,GL_RGBA32F,true);
	}
	GL_ERROR_CHECK
	numFadeDistances=num_dist;
	numFadeElevations=num_elev;
	numAltitudes=num_alt;
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	skylight_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	overcast_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	skylight_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	overcast_2d.InitColor_Tex(0,GL_RGBA32F_ARB);

	loss_texture.InvalidateDeviceObjects();
	insc_texture.InvalidateDeviceObjects();
	skyl_texture.InvalidateDeviceObjects();

	loss_texture.ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F_ARB);
	insc_texture.ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F_ARB);
	skyl_texture.ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F_ARB);


	GL_ERROR_CHECK
}

static simul::sky::float4 Lookup(crossplatform::DeviceContext &deviceContext,FramebufferGL &fb,float distance_texcoord,float elevation_texcoord)
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
	fb.Activate(deviceContext);
	glReadPixels(x,y,2,2,GL_RGBA,GL_FLOAT,(GLvoid*)data);
	fb.Deactivate(deviceContext.platform_context);
	simul::sky::float4 bottom	=simul::sky::lerp(x_interp,data[0],data[1]);
	simul::sky::float4 top		=simul::sky::lerp(x_interp,data[2],data[3]);
	simul::sky::float4 ret		=simul::sky::lerp(y_interp,bottom,top);
	return ret;
}

const float *SimulGLSkyRenderer::GetFastLossLookup(crossplatform::DeviceContext &deviceContext,float distance_texcoord,float elevation_texcoord)
{
	return Lookup(deviceContext,loss_2d,distance_texcoord,elevation_texcoord);
}

const float *SimulGLSkyRenderer::GetFastInscatterLookup(crossplatform::DeviceContext &deviceContext,float distance_texcoord,float elevation_texcoord)
{
	return Lookup(deviceContext,inscatter_2d,distance_texcoord,elevation_texcoord);
}

void SimulGLSkyRenderer::RenderIlluminationBuffer(crossplatform::DeviceContext &deviceContext)
{
	math::Vector3 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
	SetIlluminationConstants(earthShadowUniforms,skyConstants,cam_pos);
	skyConstants.Apply(deviceContext);
	earthShadowUniforms.Apply();
	{
		//D3DXHANDLE tech=m_pSkyEffect->GetTechniqueByName("illumination_buffer");
		//m_pSkyEffect->SetTechnique(tech);
		glUseProgram(techIlluminationBuffer->asGLuint());
		illumination_fb.Activate(deviceContext);
		illumination_fb.Clear(deviceContext.platform_context,1.0f,1.0f,1.0f,1.0f,1.f);
		DrawQuad(0,0,1,1);
		illumination_fb.Deactivate(deviceContext.platform_context);
	}
}
// Here we blend the four 3D fade textures (distance x elevation x altitude at two keyframes, for loss and inscatter)
// into pair of 2D textures (distance x elevation), eliminating the viewing altitude and time factor.
bool SimulGLSkyRenderer::Render2DFades(crossplatform::DeviceContext &deviceContext)
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
	RenderIlluminationBuffer(deviceContext);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	FramebufferGL *fb[]={&loss_2d,&inscatter_2d,&skylight_2d};
	GLuint target_textures[]={loss_texture.AsGLuint(),insc_texture.AsGLuint(),skyl_texture.AsGLuint()};
	simul::opengl::Texture *input_textures[]={loss_textures,insc_textures,skyl_textures};
	glUseProgram(techFade3DTo2D->asGLuint());
	skyConstants.skyInterp			=skyKeyframer->GetSubdivisionInterpolation(skyKeyframer->GetTime()).interpolation;
	math::Vector3 cam_pos			=camera::GetCameraPosVector(deviceContext.viewStruct.view);
	skyConstants.altitudeTexCoord	=skyKeyframer->GetAltitudeTexCoord(cam_pos.z/1000.f);
	sky::OvercastStruct o			=skyKeyframer->GetCurrentOvercastState();
	skyConstants.overcast			=o.overcast;
	skyConstants.overcastBaseKm		=o.overcast_base_km;
	skyConstants.overcastRangeKm	=o.overcast_scale_km;
	skyConstants.eyePosition		=cam_pos;
	skyConstants.cloudShadowRange	=sqrt(80.f/skyKeyframer->GetMaxDistanceKm());
	skyConstants.cycled_index		=texture_cycle%3;
	skyConstants.Apply(deviceContext);
	glUniform1i(fadeTexture1_fade,0);
	glUniform1i(fadeTexture2_fade,1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,1.0,0,1.0,-1.0,1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	GL_ERROR_CHECK
	for(int i=0;i<3;i++)
	{
	GL_ERROR_CHECK
		fb[i]->Activate(deviceContext);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][(texture_cycle+0)%3].AsGLuint());
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][(texture_cycle+1)%3].AsGLuint());
		DrawQuad(0,0,1,1);
	GL_ERROR_CHECK
		// copy to target:
		{
			glEnable(GL_TEXTURE_2D);
			glActiveTexture(GL_TEXTURE0);
	GL_ERROR_CHECK
			glBindTexture(GL_TEXTURE_2D,target_textures[i]);
	GL_ERROR_CHECK
			glCopyTexSubImage2D(GL_TEXTURE_2D,0
					,0,0
					,0,0,
 					numFadeDistances,numFadeElevations	);
			glDisable(GL_TEXTURE_2D);
	GL_ERROR_CHECK
		}
		fb[i]->Deactivate(deviceContext.platform_context);
	}
	glUseProgram(NULL);
	
	glUseProgram(techOvercastInscatter->asGLuint());

	earthShadowUniforms.targetTextureSize=vec2((float)numFadeDistances,(float)numFadeElevations);
	earthShadowUniforms.Apply();
	
	setTexture(techOvercastInscatter->asGLuint(),"inscTexture"			,0,(GLuint)inscatter_2d.GetColorTex());
	setTexture(techOvercastInscatter->asGLuint(),"illuminationTexture"	,1,(GLuint)illumination_fb.GetColorTex());

	overcast_2d.Activate(deviceContext);
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

bool SimulGLSkyRenderer::RenderFades(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	int size=width/3;
	if(height/4<size)
		size=height/4;
	if(size<2)
		return false;
	int s=size/numAltitudes-2;
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
	glBindTexture(GL_TEXTURE_2D,loss_texture.AsGLuint());
//	RenderTexture(x0+size+2,y	,size,size);
	deviceContext.renderPlatform->DrawTexture(deviceContext,x0+size+2,y,size,size,&loss_texture);
	y+=size+8;
	glBindTexture(GL_TEXTURE_2D,insc_texture.AsGLuint());
	deviceContext.renderPlatform->DrawTexture(deviceContext,x0+size+2,y	,size,size,&insc_texture);
	glBindTexture(GL_TEXTURE_2D,(GLuint)overcast_2d.GetColorTex());
	deviceContext.renderPlatform->DrawTexture(deviceContext,x0		,y	,size,size,overcast_2d.GetTexture());
	y+=size+8;

	deviceContext.renderPlatform->DrawTexture(deviceContext,x0+size+2	,y	,size,size,&skyl_texture);
	y+=size+8;
	bool show_3=true;
	//glBindTexture(GL_TEXTURE_2D,(GLuint)illumination_fb.GetColorTex());
	effect->SetTexture("illuminationTexture",illumination_fb.GetTexture());
	//effect->Apply(deviceContext,techShowIlluminationBuffer,0);
	//RenderTexture(x0+size+2	,y	,size,size);
	//deviceContext.renderPlatform->DrawQuad(deviceContext,x0+size+2	,y	,size,size,effect,techShowIlluminationBuffer);
	//effect->Unapply(deviceContext);
#if 1
	x0+=2*(size+8);
	for(int i=0;i<numAltitudes;i++)
	{
GL_ERROR_CHECK
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		int y=y0+i*(s+4);
	//	glUseProgram(techFade3DTo2D->asGLuint());
GL_ERROR_CHECK
	//	GLint fadeTexture1_fade			=glGetUniformLocation(techFade3DTo2D->asGLuint(),"fadeTexture1");
	//	GLint fadeTexture2_fade			=glGetUniformLocation(techFade3DTo2D->asGLuint(),"fadeTexture2");
GL_ERROR_CHECK
		skyConstants.skyInterp			=0.f;
		skyConstants.altitudeTexCoord	=atc;
		skyConstants.Apply(deviceContext);

	//	glUniform1i(fadeTexture1_fade,0);
	//	glUniform1i(fadeTexture2_fade,0);
		
	//	glActiveTexture(GL_TEXTURE0);
GL_ERROR_CHECK
		for(int j=0;j<(show_3?3:2);j++)
		{
			int x=x0+(s+8)*j;
			effect->SetTexture("fadeTexture1",&loss_textures[(texture_cycle+j)%3]);
			effect->SetTexture("fadeTexture2",&loss_textures[(texture_cycle+j)%3]);
GL_ERROR_CHECK
			deviceContext.renderPlatform->DrawQuad(deviceContext,x	,y+8			,s,s,effect,techShowCrossSection);
GL_ERROR_CHECK
			effect->SetTexture("fadeTexture1",&insc_textures[(texture_cycle+j)%3]);
			effect->SetTexture("fadeTexture2",&insc_textures[(texture_cycle+j)%3]);
			deviceContext.renderPlatform->DrawQuad(deviceContext,x	,y+16+size		,s,s,effect,techShowCrossSection);
GL_ERROR_CHECK
			effect->SetTexture("fadeTexture1",&skyl_textures[(texture_cycle+j)%3]);
			effect->SetTexture("fadeTexture2",&skyl_textures[(texture_cycle+j)%3]);
			deviceContext.renderPlatform->DrawQuad(deviceContext,x	,y+24+2*size	,s,s,effect,techShowCrossSection);
GL_ERROR_CHECK
		}
	}
#endif
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

bool SimulGLSkyRenderer::RenderPointStars(crossplatform::DeviceContext &deviceContext,float exposure)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	float sid[16];
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
	skyConstants.Apply(deviceContext);
	float mat1[16],mat2[16];
	glGetFloatv(GL_MODELVIEW_MATRIX,mat1);
	math::Vector3 cam_pos;
	CalcCameraPosition(cam_pos);
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

void SimulGLSkyRenderer::RenderSun(crossplatform::DeviceContext &deviceContext,float exposure)
{
	math::Vector3 cam_pos;
	CalcCameraPosition(cam_pos);
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
	
	//glUniform4f(sunlight_param,sunlight.x,sunlight.y,sunlight.z,sunlight.w);
	//skyConstants.colour=sunlight;
	GL_ERROR_CHECK
	glEnable(GL_BLEND);
	glDepthFunc(ReverseDepth?GL_GEQUAL:GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); 
	float rads=skyKeyframer->GetSkyInterface()->GetSunRadiusArcMinutes()/60.f*pi/180.f;
	SetConstantsForPlanet(skyConstants,deviceContext.viewStruct.view,deviceContext.viewStruct.proj,sun_dir,4.f*rads,sunlight,sun_dir);
	skyConstants.Apply(deviceContext);
	effect->Apply(deviceContext,techSun,0);
	effect->SetParameter("sunlight",(vec4)sunlight);//				=glGetUniformLocation(techSun->asGLuint(),"sunlight");
	renderPlatform->DrawQuad(deviceContext);
//	RenderAngledQuad(sun_dir,skyKeyframer->GetSkyInterface()->GetSunRadiusArcMinutes()/60.f*pi/180.f);
	effect->Unapply(deviceContext);
}

void SimulGLSkyRenderer::RenderPlanet(crossplatform::DeviceContext &deviceContext,void* tex,float planet_angular_size,const float *dir,const float *colr,bool do_lighting)
{
	GL_ERROR_CHECK
	math::Vector3 cam_pos;
	CalcCameraPosition(cam_pos);
//	float alt_km=0.001f*cam_pos.z;
	glDepthFunc(ReverseDepth?GL_GEQUAL:GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	simul::sky::float4 original_irradiance=skyKeyframer->GetSkyInterface()->GetSunIrradiance();

	simul::sky::float4 planet_dir4=dir;
	planet_dir4/=simul::sky::length(planet_dir4);

	simul::sky::float4 planet_colour(colr[0],colr[1],colr[2],1.f);
//	float planet_elevation=asin(planet_dir4.z);
	//planet_colour*=skyKeyframer->GetIsotropicColourLossFactor(alt_km,planet_elevation,0,1e10f);

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
	*l1=(void*)(uintptr_t)loss_texture.AsGLuint();
	*i1=(void*)(uintptr_t)insc_texture.AsGLuint();
	*s=(void*)(uintptr_t)skyl_texture.AsGLuint();
	*o=(void*)(uintptr_t)overcast_2d.GetColorTex();
}

void SimulGLSkyRenderer::FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d
	,const float *loss_float4_array,const float *inscatter_float4_array,
								const float *skylight_float4_array)
{
	if(!initialized)
		return;
	GLenum target=GL_TEXTURE_3D;
	glBindTexture(target,loss_textures[texture_index].AsGLuint());
		GL_ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)loss_float4_array);
		GL_ERROR_CHECK
	glBindTexture(target,insc_textures[texture_index].AsGLuint());
		GL_ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)inscatter_float4_array);
		GL_ERROR_CHECK
	glBindTexture(target,skyl_textures[texture_index].AsGLuint());
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
	sky::GpuSkyParameters p;
	sky::GpuSkyAtmosphereParameters a;
	sky::GpuSkyInfraredParameters ir;
	for(int i=0;i<3;i++)
	{
		skyKeyframer->GetGpuSkyParameters(p,a,ir,i);
		int cycled_index=(texture_cycle+i)%3;
		if(gpuSkyGenerator.GetEnabled())
		{
			if(p.fill_up_to_texels==32768)
				p.fill_up_to_texels=5461;
			gpuSkyGenerator.MakeLossAndInscatterTextures(cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
			if(p.fill_up_to_texels==5461)
				p.fill_up_to_texels=32768;
			gpuSkyGenerator.MakeLossAndInscatterTextures(cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
		}
		else
			skyKeyframer->cpuSkyGenerator.MakeLossAndInscatterTextures(cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
	}
}

void SimulGLSkyRenderer::EnsureTextureCycle()
{
	int cyc=(skyKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
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

	SAFE_DELETE_PROGRAM(planet_program);
	//SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	SAFE_DELETE_PROGRAM(stars_program);

	BaseSkyRenderer::RecompileShaders();
	
	std::map<std::string,std::string> defines;
	stars_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_stars.frag",defines);
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
GL_ERROR_CHECK
	//sun_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_sun.frag",defines);
	//sunlight_param				=glGetUniformLocation(sun_program,"sunlight");
	stars_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_stars.frag",defines);
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
GL_ERROR_CHECK
	planet_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_planet.frag",defines);
GL_ERROR_CHECK
	planetTexture_param				=glGetUniformLocation(planet_program,"planetTexture");
	planetColour_param				=glGetUniformLocation(planet_program,"colour");
	planetLightDir_param			=glGetUniformLocation(planet_program,"lightDir");
GL_ERROR_CHECK
	//fade_3d_to_2d_program		=MakeProgram("simul_fade_3d_to_2d");
	
	altitudeTexCoord_fade			=glGetUniformLocation(techFade3DTo2D->asGLuint(),"altitudeTexCoord");
	skyInterp_fade					=glGetUniformLocation(techFade3DTo2D->asGLuint(),"skyInterp");
GL_ERROR_CHECK
	fadeTexture1_fade				=glGetUniformLocation(techFade3DTo2D->asGLuint(),"fadeTexture1");
	fadeTexture2_fade				=glGetUniformLocation(techFade3DTo2D->asGLuint(),"fadeTexture2");

	//illumination_buffer_program		=MakeProgram("simple.vert",NULL,"illumination_buffer.frag");
	//overcast_inscatter_program		=MakeProgram("simple.vert",NULL,"overcast_inscatter.frag");
	//show_illumination_buffer_program=MakeProgram("simul_fade_3d_to_2d.vert",NULL,"show_illumination_buffer.frag");
	
GL_ERROR_CHECK
	skyConstants					.LinkToEffect(effect,"SkyConstants");
	/*skyConstants					.LinkToProgram(techFade3DTo2D->asGLuint()	,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(stars_program				,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(planet_program				,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(techSun->asGLuint()			,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(illumination_buffer_program	,"SkyConstants"			,10);
	skyConstants					.LinkToProgram(overcast_inscatter_program	,"SkyConstants"			,10);*/
	if(techIlluminationBuffer)
		earthShadowUniforms			.LinkToProgram(techIlluminationBuffer->asGLuint()	,"EarthShadowUniforms"	,9);
//	earthShadowUniforms				.LinkToProgram(overcast_inscatter_program	,"EarthShadowUniforms"	,9);
	if(techOvercastInscatter)
		earthShadowUniforms			.LinkToProgram(techOvercastInscatter->asGLuint(),"EarthShadowUniforms"	,9);
}

void SimulGLSkyRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform* r)
{
	renderPlatform=r;
GL_ERROR_CHECK
	initialized=true;
	gpuSkyGenerator.RestoreDeviceObjects(NULL);
	Texture *loss[3],*insc[3],*skyl[3];
	for(int i=0;i<3;i++)
	{
		loss[i]=&loss_textures[i];
		insc[i]=&insc_textures[i];
		skyl[i]=&skyl_textures[i];
	}
GL_ERROR_CHECK
	gpuSkyGenerator.SetDirectTargets(loss,insc,skyl,&light_table);
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	skylight_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	overcast_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	skylight_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	overcast_2d.InitColor_Tex(0,GL_RGBA32F_ARB);
	
	loss_texture.ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F);
	insc_texture.ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F);
	skyl_texture.ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F);
GL_ERROR_CHECK
	skyConstants.RestoreDeviceObjects(renderPlatform);
	earthShadowUniforms.RestoreDeviceObjects();
GL_ERROR_CHECK
	RecompileShaders();
	glUseProgram(NULL);
GL_ERROR_CHECK
	ReloadTextures();
GL_ERROR_CHECK
	ClearIterators();
GL_ERROR_CHECK
}

void SimulGLSkyRenderer::InvalidateDeviceObjects()
{
	initialized=false;
	gpuSkyGenerator.InvalidateDeviceObjects();
	
	skyConstants.InvalidateDeviceObjects();
	earthShadowUniforms.Release();
	
	BaseSkyRenderer::InvalidateDeviceObjects();

	SAFE_DELETE_PROGRAM(planet_program);
	for(int i=0;i<3;i++)
	{
		loss_textures[i].InvalidateDeviceObjects();
		insc_textures[i].InvalidateDeviceObjects();
		skyl_textures[i].InvalidateDeviceObjects();
	}
	loss_texture.InvalidateDeviceObjects();
	insc_texture.InvalidateDeviceObjects();
	skyl_texture.InvalidateDeviceObjects();
	
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

const char *SimulGLSkyRenderer::GetDebugText()
{
/*	simul::sky::EarthShadow e=skyKeyframer->GetEarthShadow(
								skyKeyframer->GetAltitudeKM()
								,skyKeyframer->GetDirectionToSun());*/
	static char txt[400];
//	sprintf_s(txt,400,"e.normal (%4.4g, %4.4g, %4.4g) r-1: %4.4g, cos: %4.4g, illum alt: %4.4g",e.normal.x,e.normal.y,e.normal.z
//		,e.radius_on_cylinder-1.f,e.terminator_cosine,e.illumination_altitude);
	return txt;
}