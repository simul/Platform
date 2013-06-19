#ifndef SIMUL_EARTHSHADOW
#define SIMUL_EARTHSHADOW

uniform_buffer EarthShadowUniforms R9
{
	vec3 sunDir;
	float radiusOnCylinder;
	vec3 earthShadowNormal;
	float maxFadeDistance;
	float terminatorCosine;
};

#ifndef __cplusplus
SamplerState inscSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};
#include "../../CrossPlatform/earth_shadow.sl"
#endif
#endif