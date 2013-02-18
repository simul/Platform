#ifndef SIMUL_2D_CLOUDS_SL
#define SIMUL_2D_CLOUDS_SL

#ifdef __cplusplus
	#define R0
	#define uniform
#endif

ALIGN cbuffer Cloud2DConstants R0
{
	uniform vec2 origin;
	uniform float globalScale;
	uniform float detailScale;

	uniform vec4 lightResponse;
	uniform vec3 lightDir;
	uniform float cloudEccentricity;
	uniform vec3 eyePosition;
	uniform float maxFadeDistanceMetres;
	uniform vec3 sunlight;
	uniform float cloudInterp;
	uniform vec3 mieRayleighRatio;
	uniform float hazeEccentricity;
};

#ifndef __cplusplus

#endif

#endif