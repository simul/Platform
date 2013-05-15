
#include "CppHlsl.hlsl"
cbuffer AtmosphericsUniforms R0
{
	vec3 lightDir;
	vec4 mieRayleighRatio;
	vec2 texelOffsets;
	float hazeEccentricity;
};