#ifndef EARTH_SHADOW_UNIFORMS_SL
#define EARTH_SHADOW_UNIFORMS_SL

uniform_buffer EarthShadowUniforms R9
{
	uniform vec3 sunDir;
	uniform float radiusOnCylinder;
	uniform vec3 earthShadowNormal;
	uniform float maxFadeDistance;
	uniform float terminatorDistance;
};

#endif