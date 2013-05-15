#version 330
#include "simul_gpu_sky.glsl"
#include "simul_inscatter_fns.glsl"

uniform sampler2D input_skyl_texture;
uniform sampler1D density_texture;
uniform sampler3D loss_texture;
uniform sampler3D insc_texture;

in vec2 texc;
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
	vec4 previous_skyl	=texture(input_skyl_texture,texc.xy);
	vec3 previous_loss	=texture(loss_texture,vec3(texc.xy,pow(distanceKm/maxDistanceKm,0.5))).rgb;
	// should adjust texc - we want the PREVIOUS loss!
	float sin_e			=1.0-2.0*(texc.y*texSize.y-texelOffset)/(texSize.y-1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texc.x*texSize.x-texelOffset)/max(texSize.x-1.0,1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distanceKm);
	float mind			=min(spaceDistKm,prevDistanceKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture(density_texture,dens_texc);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=vec4(starlight+getSkylight(alt_km),0.0);
	vec4 skyl			=light;
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	skyl.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-skyl.w*stepLengthKm*haze_factor*hazeMie.x);
	skyl.w				=saturate((1.f-mie_factor)/(1.f-total_ext.x+0.0001f));
	
	//skyl.w			=(loss.w)*(1.f-previous_skyl.w)*skyl.w+previous_skyl.w;
	skyl.rgb			*=previous_loss.rgb;
	skyl.rgb			+=previous_skyl.rgb;
	float lossw=1.0;
	skyl.w				=(lossw)*(1.0-previous_skyl.w)*skyl.w+previous_skyl.w;
    outColor			=skyl;
}
