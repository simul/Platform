
#include "CppHlsl.hlsl"
cbuffer AtmosphericsUniforms R9
{
	vec3 lightDir;
	vec4 mieRayleighRatio;
	vec2 texelOffsets;
	float hazeEccentricity;
};