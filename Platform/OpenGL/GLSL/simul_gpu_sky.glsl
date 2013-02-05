#ifndef SIMUL_GPU_SKY_GLSL
#define SIMUL_GPU_SKY_GLSL
#ifndef __cplusplus
#include "simul_inscatter_fns.glsl"
#endif
/*
layout(std140) uniform GpuSkyConstants
{
	uniform vec4 texSize;
	uniform vec4 tableSize;

	uniform float maxOutputAltKm;
	uniform float planetRadiusKm;
	uniform float maxDensityAltKm;
	uniform float hazeBaseHeightKm;
	uniform float hazeScaleHeightKm;

	uniform vec3 rayleigh;
	uniform float overcastBaseKm;
	uniform vec3 hazeMie;
	uniform float overcastRangeKm;
	uniform vec3 ozone;
	uniform float overcast;


	uniform vec3 sunIrradiance;
	uniform float maxDistanceKm;
	uniform vec3 lightDir;
	uniform float hazeEccentricity;

	uniform vec3 starlight;
	uniform vec3 mieRayleighRatio;
};*/
#define ALIGN
#define cbuffer layout(std140) uniform
#define R0
#ifndef __cplusplus
uniform sampler2D optical_depth_texture;
#endif
#include "../../CrossPlatform/simul_gpu_sky.sl"
#endif