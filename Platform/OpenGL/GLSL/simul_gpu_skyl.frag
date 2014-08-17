#version 330
#include "CppGlsl.hs"
#include "simul_gpu_sky.glsl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"

uniform sampler2D input_skyl_texture;
uniform sampler2D density_texture;
uniform sampler2D blackbody_texture;
uniform sampler3D loss_texture;
uniform sampler3D insc_texture;

in vec2 texCoords;
out  vec4 outColor;

// What spectral radiance is added on a light path towards the viewer, due to illumination of
// a volume of air by the surrounding sky?
// in cpp:
//	float cos0=dir_to_sun.z;
//	Skylight=GetAnisotropicInscatterFactor(true,hh,pif/2.f,0,1e5f,dir_to_sun,dir_to_moon,haze,false,1);
//	Skylight*=GetInscatterAngularMultiplier(cos0,Skylight.w,hh);
vec3 getSkylight(float alt_km)
{
// The inscatter factor, at this altitude looking straight up, is given by:
	vec4 insc		=texture(insc_texture,vec3(sqrt(alt_km/maxOutputAltKm),0.0,1.0));
	vec3 skylight	=InscatterFunction(insc,hazeEccentricity,0.0,mieRayleighRatio);
	return skylight;
//	return vec3(.05,.1,.2);
}

void main()
{
	vec4 previous_skyl	=texture(input_skyl_texture,texCoords.xy);
	vec3 previous_loss	=texture(loss_texture,vec3(texCoords.xy,pow(distanceKm/maxDistanceKm,0.5))).rgb;
	// should adjust texc - we want the PREVIOUS loss!
	float sin_e			=1.0-2.0*(texCoords.y*texSize.y-texelOffset)/(texSize.y-1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texCoords.x*texSize.x-texelOffset)/max(texSize.x-1.0,1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);

	outColor		=Skyl(insc_texture
						,density_texture
						,blackbody_texture
						,previous_loss
						,previous_skyl
						,maxOutputAltKm
						,maxDistanceKm
						,maxDensityAltKm
						,spaceDistKm
						,viewAltKm
						,distanceKm
						,prevDistanceKm
						,sin_e
						,cos_e);
}
