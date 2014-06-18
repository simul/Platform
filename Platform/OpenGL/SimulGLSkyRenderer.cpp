

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


SimulGLSkyRenderer::SimulGLSkyRenderer(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *m)
	:BaseSkyRenderer(sk,m)
	,planet_program(0)
	,initialized(false)
{
	gpuSkyGenerator.SetEnabled(true);
	loss_2d		=new(memoryInterface) Texture();
	insc_2d		=new(memoryInterface) Texture();
	over_2d		=new(memoryInterface) Texture();
	skyl_2d		=new(memoryInterface) Texture();

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
		loss_textures[i]->ensureTexture3DSizeAndFormat(renderPlatform,num_alt,num_elev,num_dist,GL_RGBA32F,true);
		insc_textures[i]->ensureTexture3DSizeAndFormat(renderPlatform,num_alt,num_elev,num_dist,GL_RGBA32F,true);
		skyl_textures[i]->ensureTexture3DSizeAndFormat(renderPlatform,num_alt,num_elev,num_dist,GL_RGBA32F,true);
	}
	GL_ERROR_CHECK
	numFadeDistances=num_dist;
	numFadeElevations=num_elev;
	numAltitudes=num_alt;

	loss_2d->InvalidateDeviceObjects();
	insc_2d->InvalidateDeviceObjects();
	skyl_2d->InvalidateDeviceObjects();
	over_2d->InvalidateDeviceObjects();

	loss_2d->ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F_ARB,false,true);
	insc_2d->ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F_ARB,false,true);
	skyl_2d->ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F_ARB,false,true);
	over_2d->ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,GL_RGBA32F_ARB,false,true);


	GL_ERROR_CHECK
}

static simul::sky::float4 Lookup(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *tex,float distance_texcoord,float elevation_texcoord)
{
	distance_texcoord*=(float)tex->GetWidth();
	int x=(int)(distance_texcoord);
	if(x<0)
		x=0;
	if(x>tex->GetWidth()-2)
		x=tex->GetWidth()-2;
	float x_interp=distance_texcoord-x;
	elevation_texcoord*=(float)tex->GetLength();
	int  	y=(int)(elevation_texcoord);
	if(y<0)
		y=0;
	if(y>tex->GetWidth()-2)
		y=tex->GetWidth()-2;
	float y_interp=elevation_texcoord-y;
	// four floats per texel, four texels.
	simul::sky::float4 data[4];
	tex->activateRenderTarget(deviceContext);
	glReadPixels(x,y,2,2,GL_RGBA,GL_FLOAT,(GLvoid*)data);
	tex->deactivateRenderTarget();
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
	return Lookup(deviceContext,insc_2d,distance_texcoord,elevation_texcoord);
}

void SimulGLSkyRenderer::RenderIlluminationBuffer(crossplatform::DeviceContext &deviceContext)
{
	math::Vector3 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
	SetIlluminationConstants(earthShadowUniforms,skyConstants,cam_pos);
	skyConstants.Apply(deviceContext);
	earthShadowUniforms.Apply(deviceContext);
	{
		//D3DXHANDLE tech=m_pSkyEffect->GetTechniqueByName("illumination_buffer");
		//m_pSkyEffect->SetTechnique(tech);
		glUseProgram(techIlluminationBuffer->asGLuint());
		illumination_2d->activateRenderTarget(deviceContext);
		//illumination_2d->Clear(deviceContext.platform_context,1.0f,1.0f,1.0f,1.0f,1.f);
		DrawQuad(0,0,1,1);
		illumination_2d->deactivateRenderTarget();
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
	crossplatform::Texture *target_textures[]={loss_2d,insc_2d,skyl_2d};
	//GLuint target_textures[]={loss_2d->AsGLuint(),insc_2d->AsGLuint(),skyl_2d->AsGLuint()};
	simul::crossplatform::Texture **input_textures[]={loss_textures,insc_textures,skyl_textures};
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
	for(int i=0;i<3;i++)
	{
	GL_ERROR_CHECK
		target_textures[i]->activateRenderTarget(deviceContext);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][(texture_cycle+0)%3]->AsGLuint());
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][(texture_cycle+1)%3]->AsGLuint());
	GL_ERROR_CHECK
		DrawQuad(0,0,1,1);
	GL_ERROR_CHECK
		// copy to target:
		target_textures[i]->deactivateRenderTarget();
	}
	glUseProgram(NULL);
	
	glUseProgram(techOvercastInscatter->asGLuint());

	earthShadowUniforms.targetTextureSize=vec2((float)numFadeDistances,(float)numFadeElevations);
	earthShadowUniforms.Apply(deviceContext);
	
	setTexture(techOvercastInscatter->asGLuint(),"inscTexture"			,0,insc_2d->AsGLuint());
	setTexture(techOvercastInscatter->asGLuint(),"illuminationTexture"	,1,illumination_2d->AsGLuint());

	over_2d->activateRenderTarget(deviceContext);
		DrawQuad(0,0,1,1);
	over_2d->deactivateRenderTarget();

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
	glOrtho(0,(double)main_viewport[2],(double)main_viewport[3],0,-1.0,1.0);
GL_ERROR_CHECK
	glUseProgram(0);
	glDisable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	BaseSkyRenderer::RenderFades(deviceContext,x0, y0, width, height);
	
GL_ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glDisable(GL_TEXTURE_2D);
GL_ERROR_CHECK
return true;
}

void SimulGLSkyRenderer::UseProgram(GLuint p)
{
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
	glUseProgram(effect->GetTechniqueByName("stars")->asGLuint());
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


void SimulGLSkyRenderer::Get2DLossAndInscatterTextures(void* *l1,void* *i1,void * *s,void* *o)
{
	*l1=(void*)(uintptr_t)loss_2d->AsGLuint();
	*i1=(void*)(uintptr_t)insc_2d->AsGLuint();
	*s=(void*)(uintptr_t)skyl_2d->AsGLuint();
	*o=(void*)(uintptr_t)over_2d->AsGLuint();
}

void SimulGLSkyRenderer::FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d
	,const float *loss_float4_array,const float *inscatter_float4_array,
								const float *skylight_float4_array)
{
	if(!initialized)
		return;
	GLenum target=GL_TEXTURE_3D;
	glBindTexture(target,loss_textures[texture_index]->AsGLuint());
		GL_ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)loss_float4_array);
		GL_ERROR_CHECK
	glBindTexture(target,insc_textures[texture_index]->AsGLuint());
		GL_ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)inscatter_float4_array);
		GL_ERROR_CHECK
	glBindTexture(target,skyl_textures[texture_index]->AsGLuint());
		GL_ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)skylight_float4_array);
		GL_ERROR_CHECK
	glBindTexture(target,NULL);
		GL_ERROR_CHECK
}

void SimulGLSkyRenderer::EnsureTexturesAreUpToDate(void *)
{
	EnsureCorrectTextureSizes();
	illumination_2d->ensureTexture2DSizeAndFormat(renderPlatform,128,numFadeElevations,GL_RGBA32F,false,true);
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
	moon_texture=renderPlatform->CreateTexture(skyKeyframer->GetMoonTexture().c_str());
	SetPlanetImage(moon_index,moon_texture);
}

void SimulGLSkyRenderer::RecompileShaders()
{
	gpuSkyGenerator.RecompileShaders();

	SAFE_DELETE_PROGRAM(planet_program);
	BaseSkyRenderer::RecompileShaders();
	
	std::map<std::string,std::string> defines;
	//sun_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_sun.frag",defines);
	//sunlight_param				=glGetUniformLocation(sun_program,"sunlight");
GL_ERROR_CHECK
	planet_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_planet.frag",defines);
GL_ERROR_CHECK
	planetTexture_param				=glGetUniformLocation(planet_program,"planetTexture");
	planetColour_param				=glGetUniformLocation(planet_program,"colour");
	planetLightDir_param			=glGetUniformLocation(planet_program,"lightDir");
GL_ERROR_CHECK
	//fade_3d_to_2d_program		=MakeProgram("simul_fade_3d_to_2d");
	
GL_ERROR_CHECK
	fadeTexture1_fade				=glGetUniformLocation(techFade3DTo2D->asGLuint(),"fadeTexture1");
	fadeTexture2_fade				=glGetUniformLocation(techFade3DTo2D->asGLuint(),"fadeTexture2");
	
GL_ERROR_CHECK
	skyConstants					.LinkToEffect(effect,"SkyConstants");
	earthShadowUniforms				.LinkToEffect(effect,"EarthShadowUniforms");
}

void SimulGLSkyRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform* r)
{
	renderPlatform=r;
	BaseSkyRenderer::RestoreDeviceObjects(r);
GL_ERROR_CHECK
	initialized=true;
	gpuSkyGenerator.RestoreDeviceObjects(NULL);
	crossplatform::Texture *loss[3],*insc[3],*skyl[3];
	for(int i=0;i<3;i++)
	{
		loss[i]=loss_textures[i];
		insc[i]=insc_textures[i];
		skyl[i]=skyl_textures[i];
	}
GL_ERROR_CHECK
	gpuSkyGenerator.SetDirectTargets(loss,insc,skyl,&light_table);
	
GL_ERROR_CHECK
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

	SAFE_DELETE_PROGRAM(planet_program);
	
	BaseSkyRenderer::InvalidateDeviceObjects();
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