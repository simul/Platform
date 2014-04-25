#version 330
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "simul_gpu_sky.glsl"

uniform sampler2D input_insc_texture;
uniform sampler1D density_texture;
uniform sampler3D loss_texture;

in vec2 texCoords;
out  vec4 outColor;

void main(void)
{
	vec4 previous_insc	=texture(input_insc_texture,texCoords.xy);
	vec3 previous_loss	=texture(loss_texture,vec3(texCoords.xy,pow(distanceKm/maxDistanceKm,0.5))).rgb;// should adjust texc - we want the PREVIOUS loss!
	float sin_e			=1.0-2.0*(texCoords.y*texSize.y-texelOffset)/(texSize.y-1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texCoords.x*texSize.x-texelOffset)/max(texSize.x-1.0,1.0);
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
	
	float x1			=mind*cos_e;
	float r1			=sqrt(x1*x1+y*y);
	float alt_1_km		=r1-planetRadiusKm;
	
	float x2			=maxd*cos_e;
	float r2			=sqrt(x2*x2+y*y);
	float alt_2_km		=r2-planetRadiusKm;
	
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture(density_texture,dens_texc);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=vec4(sunIrradiance,1.0)*getSunlightFactor(optical_depth_texture,alt_km,lightDir);
	vec4 insc			=light;
#ifdef OVERCAST
	insc*=1.0-getOvercastAtAltitudeRange(alt_1_km,alt_2_km);
#endif
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	insc.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-insc.w*stepLengthKm*haze_factor*hazeMie.x);
	insc.w				=saturate((1.0-mie_factor)/(1.0-total_ext.x+0.0001));
	
	insc.rgb			*=previous_loss.rgb;
	insc.rgb			+=previous_insc.rgb;
	float lossw=1.0;
	insc.w				=(lossw)*(1.0-previous_insc.w)*insc.w+previous_insc.w;

    outColor			=insc;
}
