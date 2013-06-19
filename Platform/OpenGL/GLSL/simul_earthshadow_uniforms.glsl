#ifndef EARTHSHADOWUNIFORMS_H
#define EARTHSHADOWUNIFORMS_H
uniform_buffer EarthShadowUniforms R9
{
	uniform vec3 sunDir;
	uniform float unused;
	uniform vec3 earthShadowNormal;
	uniform float unused1;
	uniform float radiusOnCylinder;
	uniform float maxFadeDistance;
	uniform float terminatorCosine;
	uniform float unused2;
	uniform float unused3;
};
#endif