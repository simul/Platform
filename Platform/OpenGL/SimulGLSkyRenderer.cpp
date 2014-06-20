

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

// Here we blend the four 3D fade textures (distance x elevation x altitude at two keyframes, for loss and inscatter)
// into pair of 2D textures (distance x elevation), eliminating the viewing altitude and time factor.
bool SimulGLSkyRenderer::Render2DFades(crossplatform::DeviceContext &deviceContext)
{
GL_ERROR_CHECK
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
GL_ERROR_CHECK
	BaseSkyRenderer::Render2DFades(deviceContext);
GL_ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,NULL);
	return true;
}

bool SimulGLSkyRenderer::RenderFades(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
GL_ERROR_CHECK
	glDisable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	BaseSkyRenderer::RenderFades(deviceContext,x0, y0, width, height);
GL_ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,NULL);
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
	illumination_2d->ensureTexture2DSizeAndFormat(renderPlatform,128,numFadeElevations,crossplatform::RGBA_32_FLOAT,false,true);
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
GL_ERROR_CHECK
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

	BaseSkyRenderer::RecompileShaders();
	
	std::map<std::string,std::string> defines;
	//sun_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_sun.frag",defines);
	//sunlight_param				=glGetUniformLocation(sun_program,"sunlight");
GL_ERROR_CHECK
	
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