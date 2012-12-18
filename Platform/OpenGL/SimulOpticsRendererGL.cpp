
#include "SimulOpticsRendererGL.h"
#include "Simul/Math/Decay.h"
#include "Simul/Platform/OpenGL/LoadGLImage.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Math/Matrix4x4.h"

SimulOpticsRendererGL::SimulOpticsRendererGL()
	:flare_program(0)
{
}

SimulOpticsRendererGL::~SimulOpticsRendererGL()
{
	InvalidateDeviceObjects();
}

void SimulOpticsRendererGL::RestoreDeviceObjects(void *)
{
	RecompileShaders();
	flare_texture=LoadGLImage("SunFlare.png",GL_CLAMP_TO_EDGE);

	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_DELETE_TEXTURE(halo_textures[i]);
	}
	halo_textures.clear();
	int num_halo_textures=0;
	for(int i=0;i<lensFlare.GetNumArtifacts();i++)
	{
		int t=lensFlare.GetArtifactType(i);
		if(t+1>num_halo_textures)
			num_halo_textures=t+1;
	}
	halo_textures.resize(num_halo_textures);
	for(int i=0;i<num_halo_textures;i++)
	{
		std::string tn=lensFlare.GetTypeName(i);
		halo_textures[i]=LoadGLImage((tn+".png").c_str(),GL_CLAMP_TO_EDGE);
	}
}

void SimulOpticsRendererGL::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(flare_program);
	flare_program					=LoadPrograms("simul_sun_planet_flare.vert",NULL,"simul_flare.frag");
	flareColour_param				=glGetUniformLocation(flare_program,"flareColour");
	flareTexture_param				=glGetUniformLocation(flare_program,"flareTexture");
	printProgramInfoLog(flare_program);

}

void SimulOpticsRendererGL::InvalidateDeviceObjects()
{
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_DELETE_TEXTURE(halo_textures[i]);
	}
	halo_textures.clear();
	SAFE_DELETE_PROGRAM(flare_program);
}

void SimulOpticsRendererGL::RenderFlare(float exposure,const float *dir,const float *light)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	simul::sky::float4 sun_dir(dir);//skyKeyframer->GetDirectionToLight());
	float magnitude=exposure;//*(1.f-sun_occlusion);
	simul::math::FirstOrderDecay(flare_magnitude,magnitude,5.f,0.1f);
	if(flare_magnitude>1.f)
		flare_magnitude=1.f;
//float alt_km=0.001f*cam_pos.y;
	simul::sky::float4 sunlight(light);//=skyKeyframer->GetLocalIrradiance(alt_km);
	sunlight*=lensFlare.GetStrength();
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	static float sun_mult=0.05f;
	sunlight*=sun_mult*flare_magnitude;
	
	glEnable(GL_BLEND);
	
	glBlendFunc(GL_ONE,GL_ONE);
	
	glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,(GLuint)((long int)flare_texture));

	glUseProgram(flare_program);
		ERROR_CHECK

	glUniform3f(flareColour_param,sunlight.x,sunlight.y,sunlight.z);
	
	glUniform1i(flareTexture_param,0);
	//if(y_vertical)
	//	std::swap(sun_dir.y,sun_dir.z);
	simul::sky::float4 cam_dir,cam_pos;
	CalcCameraPosition(cam_pos,cam_dir);
	lensFlare.UpdateCamera(cam_dir,sun_dir);
	if(flare_magnitude>0.f)
	{
		RenderAngledQuad(sun_dir,flare_angular_size*flare_magnitude);
		sunlight*=0.25f;
		for(int i=0;i<lensFlare.GetNumArtifacts();i++)
		{
			simul::sky::float4 pos=lensFlare.GetArtifactPosition(i);
			float sz=lensFlare.GetArtifactSize(i);
			int t=lensFlare.GetArtifactType(i);
	glUseProgram(0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)((long int)halo_textures[t]));
	glUseProgram(flare_program);
			//m_pSkyEffect->SetTexture(flareTexture,halo_textures[t]);
			//m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&sunlight));
			RenderAngledQuad(pos,flare_angular_size*sz*flare_magnitude);
		}
	}
	glUseProgram(0);
	glPopAttrib();
}