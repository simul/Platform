#ifndef EARTH_SHADOW_UNIFORMS_SL
#define EARTH_SHADOW_UNIFORMS_SL

SIMUL_CONSTANT_BUFFER(EarthShadowUniforms,9)
	uniform vec3 sunDir;
	uniform float radiusOnCylinder;
	uniform vec3 earthShadowNormal;
	uniform float maxFadeDistance;
	uniform vec2 targetTextureSize;
	uniform float terminatorDistance;
	uniform float temp6;
SIMUL_CONSTANT_BUFFER_END

#endif