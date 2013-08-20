#ifndef EARTH_SHADOW_UNIFORMS_SL
#define EARTH_SHADOW_UNIFORMS_SL

uniform_buffer EarthShadowUniforms SIMUL_BUFFER_REGISTER(9)
{
	uniform vec3 sunDir;
	uniform float temp1;
	uniform float radiusOnCylinder;
	uniform float temp2,temp3,temp4;
	uniform vec3 earthShadowNormal;
	uniform float temp5;
	uniform float maxFadeDistance;
	uniform float terminatorDistance;
	uniform float temp6,temp7;
	uniform vec2 targetTextureSize;
	uniform float temp8,temp9;
};

#endif