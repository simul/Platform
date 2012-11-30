#version 330
#include "simul_gpu_sky.glsl"

uniform sampler2D input_insc_texture;
uniform sampler1D density_texture;
uniform sampler3D loss_texture;

uniform float maxDistanceKm;
uniform vec3 sunIrradiance;
uniform vec3 lightDir;

in vec2 texc;
out  vec4 outColor;

void main(void)
{
	vec4 previous_insc	=texture(input_insc_texture,texc.xy);
	vec3 previous_loss	=texture(loss_texture,vec3(texc.xy,distKm/maxDistanceKm)).rgb;// should adjust texc - we want the PREVIOUS loss!
	float sin_e			=1.0-2.0*(texc.y*texSize.y-0.5)/(texSize.y-1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texc.x*texSize.x-0.5)/(texSize.x-1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distKm);
	float mind			=min(spaceDistKm,prevDistKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+0.5)/tableSize.x;
	vec4 lookups		=texture(density_texture,dens_texc);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=getSunlightFactor(alt_km,lightDir)*vec4(sunIrradiance,1.0);
	vec4 insc			=light;
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	insc.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-insc.w*stepLengthKm*haze_factor*hazeMie.x);
	insc.w				=saturate((1.f-mie_factor)/(1.f-total_ext.x+0.0001f));
	
	//insc.w				=(loss.w)*(1.f-previous_insc.w)*insc.w+previous_insc.w;
	
	insc.rgb			*=previous_loss.rgb;
	insc.rgb			+=previous_insc.rgb;
	float lossw=1.0;
	insc.w				=(lossw)*(1.0-previous_insc.w)*insc.w+previous_insc.w;

    outColor			=insc;
}
