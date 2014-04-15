#version 330

uniform sampler2D input_loss_texture;
uniform sampler1D density_texture;
uniform sampler2D optical_depth_texture;

#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/simul_gpu_sky.sl"

in vec2 texCoords;
out vec4 outColor;

void main(void)
{
	vec4 previous_loss	=texture(input_loss_texture,texCoords.xy);
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
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture(density_texture,dens_texc);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie+ozone*ozone_factor;
	vec4 loss;
	loss.rgb			=exp(-extinction*stepLengthKm);
	loss.a				=(loss.r+loss.g+loss.b)/3.0;
	loss				*=previous_loss;
    outColor			=loss;
}
