
#include "CppHlsl.hlsl"
cbuffer AtmosphericsUniforms R9
{
	float3 lightDir;
	float4 mieRayleighRatio;
	float2 texelOffsets;
	float hazeEccentricity;
};