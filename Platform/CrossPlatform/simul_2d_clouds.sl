#ifndef SIMUL_2D_CLOUDS_SL
#define SIMUL_2D_CLOUDS_SL

uniform_buffer Cloud2DConstants R9
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
};

#ifndef __cplusplus

#endif

#endif